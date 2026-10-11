#!/usr/bin/env python3
"""Black-box HTTP / SQLite checks using only the Python standard library.

Every process uses a temporary working directory and database. Existing project
data is never opened. The server executable must have been built beforehand.
"""

import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
import http.client
import json
import os
from pathlib import Path
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time


PROJECT_ROOT = Path(__file__).resolve().parent.parent
CONFIG_KEYS = (
    "SCHOOL_HOST", "SCHOOL_PORT", "SCHOOL_DB_PATH", "SCHOOL_WEB_ROOT",
    "SCHOOL_ADMIN_USER", "SCHOOL_ADMIN_PASSWORD",
)
MISSING = object()


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def equal(actual, expected, context):
    require(actual == expected, f"{context}: expected {expected!r}, got {actual!r}")


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def payload(identifier="T001", kind="teacher"):
    person = {
        "type": kind, "id": identifier, "name": "张三", "gender": "男",
        "age": 35, "phone": "13800138000", "department": "计算机学院",
        "salary": 12000.34,
    }
    person["title" if kind == "teacher" else "position"] = (
        "副教授" if kind == "teacher" else "办公室主任"
    )
    return person


class Server:
    def __init__(self, executable, directory, extra_env=None):
        self.executable = executable
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.database = self.directory / "test.db"
        self.port = free_port()
        self.log_path = self.directory / "server.log"
        self.process = None
        self.log = None
        self.environment = os.environ.copy()
        for key in CONFIG_KEYS:
            self.environment.pop(key, None)
        self.environment.update({
            "SCHOOL_HOST": "127.0.0.1", "SCHOOL_PORT": str(self.port),
            "SCHOOL_DB_PATH": str(self.database),
            "SCHOOL_WEB_ROOT": str(PROJECT_ROOT / "web"),
        })
        self.environment.update(extra_env or {})

    def spawn(self):
        self.log = self.log_path.open("wb")
        try:
            self.process = subprocess.Popen(
                [str(self.executable)], cwd=self.directory, env=self.environment,
                stdin=subprocess.DEVNULL, stdout=self.log, stderr=self.log,
                creationflags=(subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0),
            )
        except Exception:
            self.log.close()
            self.log = None
            raise

    def diagnostics(self):
        if self.log:
            self.log.flush()
        return self.log_path.read_text(encoding="utf-8", errors="replace")[-3000:]

    def start(self):
        self.spawn()
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise AssertionError("Server exited during startup:\n" + self.diagnostics())
            try:
                status, body, _ = self.request("GET", "/api/health", timeout=0.5)
                if status == 200 and body.get("database") == "ok":
                    return self
            except (OSError, http.client.HTTPException, ValueError):
                pass
            time.sleep(0.05)
        raise AssertionError("Server did not become ready:\n" + self.diagnostics())

    def stop(self):
        try:
            if self.process and self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=5)
        finally:
            if self.log:
                self.log.close()
                self.log = None

    def __enter__(self):
        try:
            return self.start()
        except Exception:
            self.stop()
            raise

    def __exit__(self, *_):
        self.stop()

    def request(self, method, path, data=MISSING, raw=None, headers=None, timeout=15):
        request_headers = dict(headers or {})
        if data is not MISSING:
            raw = json.dumps(data, ensure_ascii=True, allow_nan=False).encode("utf-8")
        if raw is not None:
            request_headers.setdefault("Content-Type", "application/json")
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=timeout)
        try:
            connection.request(method, path, body=raw, headers=request_headers)
            response = connection.getresponse()
            body = response.read()
            content_type = response.getheader("Content-Type", "")
            parsed = json.loads(body.decode("utf-8")) if "application/json" in content_type else body
            return response.status, parsed, dict(response.getheaders())
        finally:
            connection.close()

    def expect(self, method, path, status, **kwargs):
        actual, body, headers = self.request(method, path, **kwargs)
        equal(actual, status, f"{method} {path}")
        if status >= 400 and path != "/api/health":
            require(isinstance(body, dict) and isinstance(body.get("error"), str),
                    f"{method} {path}: error response must have an error string")
        return body, headers


def test_health_static(server):
    health, _ = server.expect("GET", "/api/health", 200)
    equal(health, {"status": "ok", "service": "school-personnel", "database": "ok"}, "health")
    for path, marker in (("/", b"<!DOCTYPE"), ("/app.js", b"/api/"), ("/styles.css", b"{")):
        body, headers = server.expect("GET", path, 200)
        require(marker.lower() in body.lower(), f"Static asset {path} has unexpected content")
        equal(headers.get("X-Content-Type-Options"), "nosniff", "response security header")
    records, _ = server.expect("GET", "/api/people", 200)
    equal(records, [], "new test database should be empty")


def test_crud(server):
    teacher = payload()
    admin = payload("A001", "admin")
    for person in (teacher, admin):
        body, _ = server.expect("POST", "/api/people", 201, data=person)
        equal(body, {"ok": True}, "create acknowledgement")
    server.expect("POST", "/api/people", 409, data=teacher)
    people, _ = server.expect("GET", "/api/people", 200)
    equal({p["id"]: p for p in people}, {"T001": teacher, "A001": admin}, "teacher and admin roundtrip")

    # Rename a teacher and switch the same record into the administrative class.
    changed = payload("A002", "admin")
    changed.update(name="李四", gender="女", age=42, salary=9900.56)
    server.expect("PUT", "/api/people/T001", 200, data=changed)
    people, _ = server.expect("GET", "/api/people", 200)
    equal({p["id"]: p for p in people}, {"A002": changed, "A001": admin}, "rename and type switch")
    collision = dict(changed, id="A001")
    server.expect("PUT", "/api/people/A002", 409, data=collision)
    server.expect("PUT", "/api/people/NO_SUCH_ID", 404, data=teacher)
    server.expect("DELETE", "/api/people/NO_SUCH_ID", 404)
    server.expect("DELETE", "/api/people/A001", 200)
    server.expect("DELETE", "/api/people/A001", 404)
    people, _ = server.expect("GET", "/api/people", 200)
    equal(people, [changed], "delete must preserve the other record")


def test_invalid_requests(server):
    before, _ = server.expect("GET", "/api/people", 200)
    valid = payload("INVALID_ROW")
    invalid_fields = {
        "type": ("student", None, 1, True),
        "age": ("35", None, True, 17, 101, -1, 35.5, 10 ** 80),
        "salary": ("12000", None, True, -0.01, 100000001, 10 ** 80),
        "id": ("", "bad/id", "A" * 33, None),
        "name": ("", "　 ", "张" * 65, None, "张\x00三"),
        "gender": ("unknown", None),
        "phone": ("not-a-phone", None),
        "department": ("", None),
        "title": ("", None),
    }
    for field, values in invalid_fields.items():
        for value in values:
            candidate = dict(valid)
            candidate[field] = value
            try:
                server.expect("POST", "/api/people", 400, data=candidate)
            except AssertionError as error:
                raise AssertionError(f"Invalid {field}={value!r}: {error}") from error
    for field in ("type", "id", "name", "gender", "age", "department", "title", "salary"):
        candidate = dict(valid)
        del candidate[field]
        server.expect("POST", "/api/people", 400, data=candidate)
    malformed = (b'{"type":', b'{"type":"teacher",}', b'[]', b'"text"', b'null')
    for raw in malformed:
        server.expect("POST", "/api/people", 400, raw=raw)
    raw = json.dumps(valid, ensure_ascii=True).encode("ascii")
    server.expect("POST", "/api/people", 400, raw=raw.replace(b"12000.34", b"1e999"))
    server.expect("POST", "/api/people", 400, raw=raw.replace(b"12000.34", b"NaN"))
    server.expect("POST", "/api/people", 415, raw=b"not-json", headers={"Content-Type": "text/plain"})
    after, _ = server.expect("GET", "/api/people", 200)
    equal(after, before, "invalid requests must not change stored records")


def test_unicode_money(server):
    person = payload("UNICODE")
    person.update(name='王小明 "老师" \\ 🌟', salary=12345678.91)
    raw = json.dumps(person, ensure_ascii=True).encode("ascii")
    require(b"\\u738b" in raw and b"\\ud83c" in raw, "Test body must contain Unicode escapes")
    server.expect("POST", "/api/people", 201, raw=raw)
    people, _ = server.expect("GET", "/api/people", 200)
    stored = next(p for p in people if p["id"] == "UNICODE")
    equal(stored, person, "Unicode escapes, quoted name and salary cents roundtrip")


def test_payload_limit(server):
    raw = b'{"padding":"' + b"a" * (64 * 1024) + b'"}'
    require(len(raw) > 65536, "Oversized test body must exceed 64 KiB")
    server.expect("POST", "/api/people", 413, raw=raw)
    server.expect("GET", "/api/health", 200)


def test_concurrent_duplicate(server):
    candidate = payload("RACE")
    def create(_):
        return server.request("POST", "/api/people", data=candidate)[0]
    with ThreadPoolExecutor(max_workers=12) as pool:
        statuses = list(pool.map(create, range(12)))
    equal(statuses.count(201), 1, "concurrent duplicate: exactly one insert")
    equal(statuses.count(409), 11, "concurrent duplicate: all other inserts conflict")
    people, _ = server.expect("GET", "/api/people", 200)
    equal(sum(person["id"] == "RACE" for person in people), 1, "only one duplicate row persisted")


def test_auth(executable, directory):
    username, password = "test-admin", "temporary-test-password"
    encoded = base64.b64encode(f"{username}:{password}".encode("ascii")).decode("ascii")
    headers = {"Authorization": "Basic " + encoded}
    with Server(executable, directory, {"SCHOOL_ADMIN_USER": username, "SCHOOL_ADMIN_PASSWORD": password}) as server:
        server.expect("GET", "/api/health", 200)
        for path in ("/", "/app.js", "/api/people"):
            _, response_headers = server.expect("GET", path, 401)
            require("Basic" in response_headers.get("WWW-Authenticate", ""), "missing Basic auth challenge")
        server.expect("GET", "/api/people", 401, headers={"Authorization": "Basic invalid"})
        server.expect("GET", "/", 200, headers=headers)
        server.expect("GET", "/api/people", 200, headers=headers)
        server.expect("POST", "/api/people", 201, data=payload("AUTH"), headers=headers)
        server.expect("POST", "/api/people", 401, data=payload("UNAUTHORIZED"))


def expect_startup_failure(executable, directory, environment=None, corrupt=False):
    server = Server(executable, directory, environment)
    if corrupt:
        server.database.write_bytes(b"This is not a SQLite database.\n" * 64)
    try:
        server.spawn()
        try:
            exit_code = server.process.wait(timeout=10)
        except subprocess.TimeoutExpired as error:
            raise AssertionError("Invalid configuration unexpectedly left the server running") from error
        require(exit_code != 0, "Invalid configuration must return a nonzero exit code")
        require(bool(server.diagnostics().strip()), "Startup failure must explain the problem")
    finally:
        server.stop()


def test_bad_config(executable, directory):
    cases = [
        {"SCHOOL_PORT": "0"}, {"SCHOOL_PORT": "65536"}, {"SCHOOL_PORT": "8080oops"},
        {"SCHOOL_PORT": "not-a-port"},
        {"SCHOOL_ADMIN_USER": "admin"}, {"SCHOOL_ADMIN_PASSWORD": "test-password"},
        {"SCHOOL_ADMIN_USER": "invalid:user", "SCHOOL_ADMIN_PASSWORD": "test-password"},
        {"SCHOOL_HOST": "0.0.0.0"},
        {"SCHOOL_WEB_ROOT": str(Path(directory) / "missing-web")},
        {"SCHOOL_DB_PATH": str(Path(directory) / "missing-directory" / "test.db")},
    ]
    for index, environment in enumerate(cases):
        expect_startup_failure(executable, Path(directory) / str(index), environment)


def test_lock_recovery(executable, directory):
    with Server(executable, directory) as server:
        candidate = payload("LOCKED")
        connection = sqlite3.connect(server.database, timeout=10)
        try:
            connection.execute("BEGIN IMMEDIATE")
            server.expect("POST", "/api/people", 503, data=candidate, timeout=20)
        finally:
            connection.rollback()
            connection.close()
        server.expect("GET", "/api/health", 200)
        server.expect("POST", "/api/people", 201, data=candidate)
        stored, _ = server.expect("GET", "/api/people", 200)
        equal(stored, [candidate], "write after releasing external database lock")


def test_schema_failure(executable, directory):
    with Server(executable, directory) as server:
        connection = sqlite3.connect(server.database, timeout=10)
        try:
            connection.execute("DROP TABLE people")
            connection.commit()
        finally:
            connection.close()
        health, _ = server.expect("GET", "/api/health", 503)
        equal(health, {"status": "error", "service": "school-personnel", "database": "unavailable"}, "broken schema health")
        server.expect("GET", "/api/people", 503)


def test_invalid_stored_utf8(executable, directory):
    with Server(executable, directory) as server:
        person = payload("BAD_UTF8")
        server.expect("POST", "/api/people", 201, data=person)
        connection = sqlite3.connect(server.database, timeout=10)
        try:
            connection.execute("UPDATE people SET name=CAST(X'C0AF' AS TEXT) WHERE id=?", (person["id"],))
            connection.commit()
        finally:
            connection.close()
        server.expect("GET", "/api/people", 503)
        health, _ = server.expect("GET", "/api/health", 503)
        equal(health, {"status": "error", "service": "school-personnel", "database": "unavailable"}, "invalid stored UTF-8 health")
        person["name"] = "恢复教师"
        connection = sqlite3.connect(server.database, timeout=10)
        try:
            connection.execute("UPDATE people SET name=? WHERE id=?", (person["name"], person["id"]))
            connection.commit()
        finally:
            connection.close()
        server.expect("GET", "/api/health", 200)
        stored, _ = server.expect("GET", "/api/people", 200)
        equal(stored, [person], "valid UTF-8 repair restores the stored record")


def group(label, action):
    action()
    print(f"PASS {label}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", type=Path,
                        default=PROJECT_ROOT / "build" / ("server.exe" if os.name == "nt" else "server"),
                        help="Path to the already-built C++ server executable")
    args = parser.parse_args()
    executable = args.server.resolve()
    if not executable.is_file():
        parser.error(f"Server executable not found: {executable}. Build it first.")
    with tempfile.TemporaryDirectory(prefix="school-api-test-") as temporary:
        root = Path(temporary)
        primary = root / "primary"
        with Server(executable, primary) as server:
            group("health and static files", lambda: test_health_static(server))
            group("teacher/admin CRUD, conflicts, rename and type switch", lambda: test_crud(server))
            group("invalid JSON and field validation", lambda: test_invalid_requests(server))
            group("oversized request returns 413 without disrupting health", lambda: test_payload_limit(server))
            group("Unicode escapes and salary cents", lambda: test_unicode_money(server))
            group("concurrent duplicate IDs", lambda: test_concurrent_duplicate(server))
            expected_records, _ = server.expect("GET", "/api/people", 200)
        with Server(executable, primary) as restarted:
            def verify_persistence():
                actual, _ = restarted.expect("GET", "/api/people", 200)
                equal(actual, expected_records, "records across process restart")
            group("SQLite persistence across restart", verify_persistence)
        group("Basic authentication and public health", lambda: test_auth(executable, root / "auth"))
        group("invalid configuration rejects startup", lambda: test_bad_config(executable, root / "bad-config"))
        group("corrupt database rejects startup", lambda: expect_startup_failure(executable, root / "corrupt", corrupt=True))
        group("database lock returns 503 and recovers", lambda: test_lock_recovery(executable, root / "locked"))
        group("broken schema returns health/list 503", lambda: test_schema_failure(executable, root / "broken-schema"))
        group("invalid stored UTF-8 returns 503 and recovers after repair", lambda: test_invalid_stored_utf8(executable, root / "invalid-utf8"))
    print("All 13 groups passed. Only temporary test databases were used.")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, http.client.HTTPException, sqlite3.Error) as error:
        print(f"FAIL {error}", file=sys.stderr)
        sys.exit(1)
