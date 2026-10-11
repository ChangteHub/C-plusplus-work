#!/usr/bin/env python3
"""Assignment acceptance checks. Uses isolated databases, never production data."""
import argparse
from contextlib import closing
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import tempfile
from test_api import Server, payload, student_payload, worker_payload, equal, require, group

PASSWORD="TemporaryStudent123!"
ADMIN={"SCHOOL_ADMIN_USER":"admin","SCHOOL_ADMIN_PASSWORD":"TemporaryAdmin123!"}
ROOT=Path(__file__).resolve().parent.parent

def cookie(headers):
    value=headers.get("Set-Cookie","")
    require("HttpOnly" in value and "SameSite=Strict" in value,"session cookie flags")
    return {"Cookie":value.split(";",1)[0]}

def login(server,id,password=PASSWORD):
    profile,headers=server.expect("POST","/api/auth/login",200,data={"username":id,"password":password})
    return profile,cookie(headers)

def graduate(id="G1"):
    value=student_payload(id)
    value.update(type="graduate",age=23,researchDirection="智能控制",supervisor="李老师")
    return value

def acceptance(server):
    server.expect("GET","/",200)
    server.expect("GET","/api/people",401)
    profile,admin=login(server,"admin",ADMIN["SCHOOL_ADMIN_PASSWORD"])
    equal(profile["role"],"admin","admin identity")
    server.expect("POST","/api/auth/cancel",403,data={"password":ADMIN["SCHOOL_ADMIN_PASSWORD"]},headers=admin)
    accounts={}
    for value in (student_payload("S1"),student_payload("S2"),graduate()):
        info,headers=server.expect("POST","/api/auth/register",201,data={"person":value,"password":PASSWORD})
        equal(info["person"],value,"registered personal profile")
        accounts[value["id"]]=cookie(headers)
    duplicate=student_payload("S1");duplicate["name"]="不能冒领已有学号"
    server.expect("POST","/api/auth/register",409,data={"person":duplicate,"password":PASSWORD})
    invalid=graduate("BAD_G");invalid["researchDirection"]=""
    server.expect("POST","/api/auth/register",400,data={"person":invalid,"password":PASSWORD})
    invalid=graduate("BAD_G");invalid["age"]=17
    server.expect("POST","/api/auth/register",400,data={"person":invalid,"password":PASSWORD})
    server.expect("POST","/api/auth/register",400,data={"person":student_payload("BAD_P"),"password":"short"})
    for id,kind in (("T1","teacher"),("T2","teacher"),("W1","worker")):
        value=worker_payload(id) if kind=="worker" else payload(id)
        server.expect("POST","/api/people",201,data=value,headers=admin)
        server.expect("POST","/api/accounts",201,data={"personId":id,"password":PASSWORD},headers=admin)
        _,accounts[id]=login(server,id)
    group("individual registration, provisioning and graduate fields",lambda:None)
    for id,headers in accounts.items():
        me,_=server.expect("GET","/api/me",200,headers=headers)
        equal(me["person"]["id"],id,"own profile")
        server.expect("GET","/api/people",403,headers=headers)
        server.expect("GET","/api/accounts",403,headers=headers)
        server.expect("GET","/api/storage/export",403,headers=headers)
        server.expect("POST","/api/people",403,data=payload("ILLEGAL"),headers=headers)
    server.expect("POST","/api/courses",403,data={"id":"BAD","name":"越权","credit":3},headers=accounts["T1"])
    server.expect("POST","/api/auth/login",401,data={"username":"S1","password":"WrongPassword123!"})
    server.expect("GET","/api/me",401,headers={"Cookie":"school_session="+"0"*64})
    server.expect("POST","/api/courses",403,data={"id":"BAD","name":"CSRF","credit":3},headers={**admin,"Origin":"https://untrusted.example"})
    group("personal access, administrator permissions and origin checks",lambda:None)
    course={"id":"CPP","name":"C++ 程序设计","credit":3,"teacherId":"T1"}
    server.expect("POST","/api/courses",201,data=course,headers=admin)
    server.expect("POST","/api/courses",409,data=course,headers=admin)
    for bad in ({"id":"BAD","name":"bad","credit":True},{"id":"BAD","name":"bad","credit":31},{"id":"BAD","name":"bad","credit":3,"teacherId":"S1"}):
        server.expect("POST","/api/courses",400,data=bad,headers=admin)
    for id,score in (("G1",90.5),("S1",90.5),("S2",80)):
        server.expect("POST","/api/courses/CPP/grades",200,data={"studentId":id,"score":score},headers=accounts["T1"])
    ranks,_=server.expect("GET","/api/courses/CPP/grades",200,headers=admin)
    equal([row["rank"] for row in ranks],[1,1,3],"competition ranks")
    equal([row["studentId"] for row in ranks],["G1","S1","S2"],"stable tie order")
    server.expect("GET","/api/courses/CPP/grades",403,headers=accounts["S1"])
    server.expect("POST","/api/courses/CPP/grades",403,data={"studentId":"S1","score":100},headers=accounts["S1"])
    server.expect("GET","/api/courses/CPP/grades",403,headers=accounts["T2"])
    for bad in ({"studentId":"S1","score":101},{"studentId":"S1","score":-1},{"studentId":"S1","score":90.001},{"studentId":"S1","score":True},{"studentId":"T1","score":90}):
        server.expect("POST","/api/courses/CPP/grades",400,data=bad,headers=admin)
    for id in ("S1","S2","G1"):
        courses,_=server.expect("GET","/api/my/courses",200,headers=accounts[id])
        equal(courses[0]["score"],80 if id=="S2" else 90.5,"own course score")
        equal(courses[0]["rank"],3 if id=="S2" else 1,"own course rank")
        require("studentId" not in json.dumps(courses),"student must not receive other students' rows")
    server.expect("POST","/api/courses",201,data={"id":"EMPTY","name":"未录入课程","credit":2},headers=admin)
    courses,_=server.expect("GET","/api/my/courses",200,headers=accounts["S1"])
    empty=next(c for c in courses if c["id"]=="EMPTY")
    require(empty["score"] is None and empty["rank"] is None and empty["participants"]==0,"unrecorded scores")
    group("courses, assigned teacher access, exact scores and tied rankings",lambda:None)
    renamed=student_payload("S1NEW")
    server.expect("PUT","/api/people/S1",200,data=renamed,headers=admin)
    me,_=server.expect("GET","/api/me",200,headers=accounts["S1"])
    equal(me["person"]["id"],"S1NEW","session ID cascade")
    server.expect("POST","/api/auth/login",401,data={"username":"S1","password":PASSWORD})
    _,accounts["S1NEW"]=login(server,"S1NEW")
    me,_=server.expect("GET","/api/my/courses",200,headers=accounts["S1NEW"])
    equal(next(c for c in me if c["id"]=="CPP")["score"],90.5,"grade ID cascade")
    server.expect("POST","/api/courses",201,data={"id":"GRAD","name":"研究生助教课程","credit":1,"teacherId":"G1"},headers=admin)
    server.expect("PUT","/api/people/G1",200,data=worker_payload("G1"),headers=admin)
    server.expect("GET","/api/courses/GRAD/grades",403,headers=accounts["G1"])
    ranks,_=server.expect("GET","/api/courses/CPP/grades",200,headers=admin)
    require(all(row["studentId"]!="G1" for row in ranks),"retired student grade must clear")
    group("identifier cascades and category changes preserve valid associations",lambda:None)
    export,_=server.expect("GET","/api/storage/export",200,headers=admin)
    equal(export["format"],"school-personnel-backup","complete export format")
    require(all(key in export for key in ("people","accounts","courses","grades")),"complete file export")
    require(PASSWORD not in json.dumps(export),"passwords must not be exported in plain text")
    saved=json.loads(Path(str(server.database)+".export.json").read_text(encoding="utf-8"))
    equal(saved,export,"file stream write and read verification")
    account=next(row for row in export["accounts"] if row["person_id"]=="S1NEW")
    expected=hashlib.pbkdf2_hmac("sha256",PASSWORD.encode(),bytes.fromhex(account["salt"]),account["iterations"]).hex()
    equal(account["password_hash"],expected,"platform PBKDF2 implementation")
    group("complete file persistence and password hash interoperability",lambda:None)
    server.expect("POST","/api/auth/password",400,data={"oldPassword":"WrongPassword123!","newPassword":"NewPassword123!"},headers=accounts["S1NEW"])
    server.expect("POST","/api/auth/password",200,data={"oldPassword":PASSWORD,"newPassword":"NewPassword123!"},headers=accounts["S1NEW"])
    server.expect("GET","/api/me",401,headers=accounts["S1NEW"])
    server.expect("GET","/api/me",401,headers=accounts["S1"])
    _,accounts["S1NEW"]=login(server,"S1NEW","NewPassword123!")
    _,extra=login(server,"W1")
    server.expect("POST","/api/auth/logout",200,data={},headers=extra)
    server.expect("GET","/api/me",401,headers=extra)
    server.expect("DELETE","/api/accounts/T2",200,headers=admin)
    server.expect("GET","/api/me",401,headers=accounts["T2"])
    people,_=server.expect("GET","/api/people",200,headers=admin)
    require(any(person["id"]=="T2" for person in people),"closing account preserves personnel")
    server.expect("POST","/api/auth/cancel",401,data={"password":"WrongPassword123!"},headers=accounts["S2"])
    server.expect("POST","/api/auth/cancel",200,data={"password":PASSWORD},headers=accounts["S2"])
    server.expect("GET","/api/me",401,headers=accounts["S2"])
    ranks,_=server.expect("GET","/api/courses/CPP/grades",200,headers=admin)
    equal([row["studentId"] for row in ranks],["S1NEW"],"cancel cascades grades")
    group("password changes, logout, account closure and cancellation",lambda:None)
    return accounts["S1NEW"],admin

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server",type=Path,default=ROOT/"build"/("server.exe" if os.name=="nt" else "server"))
    executable=parser.parse_args().server.resolve()
    with tempfile.TemporaryDirectory(prefix="school-assignment-test-") as temporary:
        folder=Path(temporary)
        with Server(executable,folder,ADMIN) as server: personal,old_admin=acceptance(server)
        with Server(executable,folder,ADMIN) as server:
            me,_=server.expect("GET","/api/me",200,headers=personal)
            equal(me["person"]["id"],"S1NEW","personal session restart")
            courses,_=server.expect("GET","/api/my/courses",200,headers=personal)
            equal(next(c for c in courses if c["id"]=="CPP")["score"],90.5,"grade restart")
            server.expect("GET","/api/me",401,headers=old_admin)
            _,admin=login(server,"admin",ADMIN["SCHOOL_ADMIN_PASSWORD"])
            with closing(sqlite3.connect(server.database)) as db:
                db.execute("UPDATE sessions SET expires=0 WHERE is_admin=0")
                db.commit()
            server.expect("GET","/api/me",401,headers=personal)
            server.expect("DELETE","/api/courses/CPP",200,headers=admin)
            with closing(sqlite3.connect(server.database)) as db:equal(db.execute("SELECT count(*) FROM grades WHERE course_id='CPP'").fetchone()[0],0,"course deletion cascade")
            group("process restart, session expiry and course removal",lambda:None)
    print("All 7 assignment acceptance groups passed; only temporary data was used.")

if __name__=="__main__":main()
