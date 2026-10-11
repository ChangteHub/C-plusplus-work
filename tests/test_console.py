#!/usr/bin/env python3
"""Exercise console student/worker entry, save/load, edit, and salary statistics."""
import argparse
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def run(executable, directory, lines):
    result = subprocess.run([str(executable)], input=("\n".join(lines) + "\n").encode("utf-8"),
                            cwd=directory, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15,
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    if result.returncode:
        raise AssertionError(result.stderr.decode("utf-8", errors="replace"))
    return result.stdout.decode("utf-8", errors="replace")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--console", type=Path,
                        default=ROOT / "build" / ("school_system.exe" if os.name == "nt" else "school_system"))
    executable = parser.parse_args().console.resolve()
    if not executable.is_file():
        parser.error("Build the console executable first")
    with tempfile.TemporaryDirectory(prefix="school-console-test-") as temporary:
        directory = Path(temporary)
        run(executable, directory, ["1", "3", "S001", "学生验证", "男", "17", "13800138000",
                                   "计算机学院", "软件工程", "软件2026级1班", "2026",
                                   "1", "4", "W001", "工作者验证", "女", "45", "13800138001",
                                   "后勤中心", "保洁", "4200.55", "9", "0"])
        data = (directory / "school_data.txt").read_text(encoding="utf-8")
        assert "Student|S001|" in data and "Worker|W001|" in data, "new categories must save distinct record types"
        output = run(executable, directory, ["10", "5", "6", "3", "S001", "S002", "学生验证", "男", "18",
                                            "13800138000", "计算机学院", "软件工程", "软件2026级2班", "2026",
                                            "3", "W001", "W002", "工作者验证", "女", "46", "13800138001",
                                            "后勤中心", "维修", "4800.65", "9", "0"])
        assert "学生人数: 1" in output and "普通工作者人数: 1" in output, "new categories must load and count correctly"
        assert "教职工平均工资: 4200.55" in output, "students must not lower the employee average salary"
        data = (directory / "school_data.txt").read_text(encoding="utf-8")
        assert "Student|S002|" in data and "软件2026级2班" in data, "editing a student must preserve its type and fields"
        assert "Worker|W002|" in data and "维修|4800.65" in data, "editing a worker must preserve its type and job"
        output = run(executable, directory, ["10", "4", "3", "3", "4", "3", "4", "2", "S002", "y", "6", "9", "0"])
        assert "人员类别: 本科生" in output and "人员类别: 普通工作者" in output, "category search must find both new types"
        assert "学生人数: 0" in output and "普通工作者人数: 1" in output, "deleting one category must preserve the other"
        assert "教职工平均工资: 4800.65" in output, "worker salary must survive save/load"
        output=run(executable,directory,["10","1","5","G001","研究生验证","男","23","13800138002","计算机学院","软件工程","研一1班","2026","智能控制","李老师","6","9","0"])
        assert "研究生人数" in output or "学生人数: 1" in output
        data=(directory/"school_data.txt").read_text(encoding="utf-8")
        assert "Graduate|G001|" in data and "智能控制|李老师" in data
        output=run(executable,directory,["10","4","1","G001","6","0"])
        assert "研究方向: 智能控制" in output and "导师: 李老师" in output
        assert "教职工平均工资: 4800.65" in output
    print("PASS console student/worker entry, edits, category search, save/load, deletion and salary statistics")


if __name__ == "__main__":
    main()
