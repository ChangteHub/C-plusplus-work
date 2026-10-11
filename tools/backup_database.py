#!/usr/bin/env python3
"""Create a consistent SQLite snapshot, including committed WAL records."""
import argparse
from pathlib import Path
import sqlite3
import sys
import time


def main():
    parser = argparse.ArgumentParser(description="备份学校人员数据库（不会覆盖已有备份）")
    parser.add_argument("--source", default="school.db", help="源数据库路径")
    parser.add_argument("--output", required=True, help="新备份文件路径")
    args = parser.parse_args()
    source = Path(args.source).resolve()
    output = Path(args.output).resolve()
    if not source.is_file():
        parser.error("源数据库不存在")
    if source == output or output.exists():
        parser.error("备份目标已存在，请指定新文件名")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation prevents accidental overwrite, including a racing writer.
    with output.open("xb"):
        pass
    started = time.monotonic()

    def progress(_status, _remaining, _total):
        if time.monotonic() - started > 30:
            raise TimeoutError("数据库繁忙，备份超过 30 秒，请稍后重试")

    try:
        with sqlite3.connect(source.as_uri() + "?mode=ro", uri=True) as incoming:
            with sqlite3.connect(output) as outgoing:
                incoming.backup(outgoing, pages=128, progress=progress, sleep=0.05)
                check = outgoing.execute("PRAGMA integrity_check").fetchone()
                if check != ("ok",):
                    raise RuntimeError("备份数据库完整性检查未通过")
                count = outgoing.execute("SELECT COUNT(*) FROM people").fetchone()[0]
            outgoing.close()
        incoming.close()
    except Exception as error:
        # Windows also needs handles closed before deleting our incomplete file.
        if "outgoing" in locals():
            outgoing.close()
        if "incoming" in locals():
            incoming.close()
        output.unlink(missing_ok=True)
        print(f"备份失败：{error}", file=sys.stderr)
        return 1
    print(f"备份成功：{output}（{count} 条人员记录）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
