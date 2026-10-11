"use strict";
const portalState={me:null,page:"",courses:[],courseId:"",editingCourse:false,busy:false,generation:0};
function showLogin(message="") {
  portalState.generation++; portalState.me=null; state.people=[]; state.loaded=false; state.signup=false; state.online=true;
  for(const id of ["pageHeader","pageMain","portalMain","portalNav"]) $(id).hidden=true;
  $("authView").hidden=false; $("loginForm").reset();
  $("loginError").textContent=message; $("loginError").hidden=!message;
  $("profileDetails").replaceChildren(); $("studyRows").replaceChildren(); $("gradeRows").replaceChildren(); $("accountRows").replaceChildren();
  $("passwordForm").reset(); $("cancelAccountForm").reset(); $("accountForm").reset(); render();
}
async function bootPortal(){try{await enterPortal(await request("/api/me",{},true));}catch(error){showLogin(error.status===401?"":error.message);}}
async function enterPortal(me){
  portalState.generation++; portalState.me=me; state.signup=false;
  setConnection("online");
  $("authView").hidden=true; $("pageHeader").hidden=false; $("portalNav").hidden=false;
  const person=me.person, student=person&&isStudent(person.type), teacher=person&&["teacher","graduate"].includes(person.type);
  $("currentUser").textContent=me.role==="admin"?(me.developmentMode?"本机管理员模式":"系统管理员"): `${person.name} · ${TYPES[person.type].label}`;
  for(const button of $("portalNav").querySelectorAll("[data-page]")){
    const page=button.dataset.page;
    button.hidden=me.role==="admin"?!["people","courses","accounts"].includes(page):!(page==="profile"||(student&&page==="study")||(teacher&&page==="teaching"));
  }
  $("btnExport").hidden=me.role!=="admin"; $("btnAdd").hidden=me.role!=="admin";
  await portalNavigate(me.role==="admin"?"people":"profile");
}
async function portalNavigate(page){
  if(!portalState.me)return;
  portalState.page=page; $("portalError").hidden=true;
  $("pageMain").hidden=page!=="people"; $("portalMain").hidden=page==="people";
  for(const button of $("portalNav").querySelectorAll("[data-page]"))button.classList.toggle("active",button.dataset.page===page);
  for(const id of ["profilePanel","studyPanel","coursesPanel","accountsPanel"])$(id).hidden=true;
  const labels={profile:["个人信息","查看本人档案、修改密码和办理销户"],study:["我的课程","查看各门课程的成绩和个人排名"],teaching:["授课课程","维护分配给自己的课程成绩"],courses:["课程与成绩","维护课程、授课人员、学生成绩与排名"],accounts:["账号管理","为已有档案开通或关闭个人登录账号"]};
  if(labels[page]){ $("portalTitle").textContent=labels[page][0]; $("portalSubtitle").textContent=labels[page][1]; }
  const generation=portalState.generation;
  try{
    if(page==="people"){await refresh();return;}
    if(page==="profile"){ $("profilePanel").hidden=false;renderProfile();return; }
    if(page==="study"){
      $("studyPanel").hidden=false;const courses=await request("/api/my/courses",{},true);if(generation!==portalState.generation)return;
      const rows=courses.map(course=>{const row=document.createElement("tr");row.append(cell(`${course.name} (${course.id})`),cell(course.credit),cell(course.teacherName||"—"),cell(course.score===null?"未录入":Number(course.score).toFixed(2)),cell(course.rank===null?"不参与排名":`${course.rank} / ${course.participants}`));return row;});
      $("studyRows").replaceChildren(...rows);if(!rows.length)emptyRow($("studyRows"),5,"暂无课程");return;
    }
    if(page==="accounts"){ $("accountsPanel").hidden=false;await loadAccounts();return; }
    $("coursesPanel").hidden=false;$("courseForm").hidden=portalState.me.role!=="admin";
    let courses=await request("/api/courses",{},true);if(generation!==portalState.generation)return;
    if(page==="teaching")courses=courses.filter(course=>course.teacherId===portalState.me.person.id);
    portalState.courses=courses;$("courseSelect").replaceChildren();
    for(const course of courses){const option=document.createElement("option");option.value=course.id;option.textContent=`${course.name} (${course.id})`;$("courseSelect").append(option);}
    if(!courses.length){const option=document.createElement("option");option.value="";option.textContent="暂无课程";$("courseSelect").append(option);}
    if(courses.some(course=>course.id===portalState.courseId))$("courseSelect").value=portalState.courseId;
    $("studentIds").replaceChildren(...state.people.filter(person=>isStudent(person.type)).map(person=>{const option=document.createElement("option");option.value=person.id;option.label=person.name;return option;}));
    await selectCourse();
  }catch(error){portalError(error);}
}
function emptyRow(body,columns,message){const row=document.createElement("tr"),td=cell(message);td.colSpan=columns;row.append(td);body.replaceChildren(row);}
function renderProfile(){
  const person=portalState.me.person;const values=[["人员类别",TYPES[person.type].label],["工号 / 学号",person.id],["姓名",person.name],["性别",person.gender],["年龄",person.age],["联系电话",person.phone||"—"],["院系 / 部门",person.department],[TYPES[person.type].roleLabel,person[TYPES[person.type].role]]];
  if(isStudent(person.type))values.push(["班级",person.className],["入学年份",person.admissionYear]);else values.push(["工资",formatMoney(person.salary)]);
  if(person.type==="graduate")values.push(["研究方向",person.researchDirection],["导师",person.supervisor]);
  $("profileDetails").replaceChildren(...values.map(([label,value])=>{const block=document.createElement("div"),dt=document.createElement("dt"),dd=document.createElement("dd");dt.textContent=label;dd.textContent=String(value);block.append(dt,dd);return block;}));
}
function portalError(error){$("portalError").textContent=error.message;$("portalError").hidden=false;if(error.status===401)showLogin(error.message);}
async function portalWrite(fn){
  if(portalState.busy)return;portalState.busy=true;$("portalError").hidden=true;
  const controls=[...$("portalMain").querySelectorAll("button,input,select"),$("btnLogout")];const previous=controls.map(control=>control.disabled);controls.forEach(control=>control.disabled=true);
  try{await fn();}catch(error){portalError(error);}finally{
    controls.forEach((control,index)=>control.disabled=previous[index]);portalState.busy=false;
    // 保存或删除课程后，以新页面的课程状态更新控件，避免恢复旧的禁用状态。
    if(portalState.page==="courses"||portalState.page==="teaching"){
      const hasCourse=portalState.courses.some(course=>course.id===portalState.courseId);
      for(const control of $("gradeForm").elements)control.disabled=!hasCourse;
      $("btnDeleteCourse").disabled=!hasCourse;
    }
  }
}
function confirmPortal(message){return new Promise(resolve=>{
  const dialog=$("portalConfirm");$("portalConfirmText").textContent=message;
  const done=value=>{dialog.close();$("portalConfirmYes").onclick=null;$("portalConfirmNo").onclick=null;dialog.oncancel=null;resolve(value);};
  $("portalConfirmYes").onclick=()=>done(true);$("portalConfirmNo").onclick=()=>done(false);dialog.oncancel=event=>{event.preventDefault();done(false);};dialog.showModal();
});}
async function selectCourse(){
  const id=$("courseSelect").value;portalState.courseId=id;const course=portalState.courses.find(value=>value.id===id);
  portalState.editingCourse=!!course;
  if(course){$("courseId").value=course.id;$("courseName").value=course.name;$("courseCredit").value=course.credit;$("courseTeacher").value=course.teacherId;$("courseSummary").textContent=`${course.credit} 学分 · ${course.teacherName||"未分配授课人员"}`;}
  else {$("courseForm").reset();$("courseSummary").textContent="请先新增课程或等待管理员分配课程";}
  $("courseId").readOnly=!!course;$("btnDeleteCourse").disabled=!course;
  for(const control of $("gradeForm").elements)control.disabled=!course;
  if(!course){emptyRow($("gradeRows"),5,"暂无课程成绩");return;}
  const generation=portalState.generation;try{const grades=await request(`/api/courses/${encodeURIComponent(id)}/grades`,{},true);if(generation!==portalState.generation||portalState.courseId!==id)return;
    const rows=grades.map(grade=>{const row=document.createElement("tr"),actions=cell(""),edit=document.createElement("button"),remove=document.createElement("button");edit.type=remove.type="button";edit.className=remove.className="btn btn-ghost";edit.textContent="编辑";remove.textContent="移除";
      edit.onclick=()=>{$("gradeStudent").value=grade.studentId;$("gradeScore").value=grade.score;$("gradeScore").focus();};
      remove.onclick=async()=>{if(await confirmPortal(`移除 ${grade.name}（${grade.studentId}）的此门课程成绩？`))await portalWrite(async()=>{await request(`/api/courses/${encodeURIComponent(id)}/grades/${encodeURIComponent(grade.studentId)}`,{method:"DELETE"});await selectCourse();toast("成绩已移除");});};
      actions.append(edit,remove);row.append(cell(grade.rank),cell(grade.studentId),cell(grade.name),cell(Number(grade.score).toFixed(2)),actions);return row;});$("gradeRows").replaceChildren(...rows);if(!rows.length)emptyRow($("gradeRows"),5,"此课程尚未录入成绩");
  }catch(error){portalError(error);}
}
async function loadAccounts(){
  const generation=portalState.generation,accounts=await request("/api/accounts",{},true);if(generation!==portalState.generation)return;
  const rows=accounts.map(account=>{const row=document.createElement("tr"),actions=cell(""),button=document.createElement("button");button.type="button";button.className="btn btn-ghost";button.textContent="关闭账号";
    button.onclick=async()=>{if(await confirmPortal(`关闭 ${account.name}（${account.personId}）的个人登录账号？人员档案会保留，当前会话将失效。`))await portalWrite(async()=>{await request(`/api/accounts/${encodeURIComponent(account.personId)}`,{method:"DELETE"});await loadAccounts();toast("登录账号已关闭");});};actions.append(button);row.append(cell(account.personId),cell(account.name),actions);return row;});$("accountRows").replaceChildren(...rows);if(!rows.length)emptyRow($("accountRows"),3,"尚未开通个人账号");
}
async function submitRegistration(person){
  const password=$("signupPassword").value;if([...password].length<8||[...password].length>128)return invalid("signupPassword","密码须为 8–128 个字符");
  state.busy=true;syncControls();try{const me=await request("/api/auth/register",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({person,password})},true);closeModal(true);$("signupPassword").value="";await enterPortal(me);toast("开户成功，已登录个人账号");}catch(error){mutationError(error,els.formError);}finally{state.busy=false;syncControls();}
}
document.addEventListener("DOMContentLoaded",()=>{
  $("loginForm").addEventListener("submit",async event=>{event.preventDefault();const button=event.submitter;if(button.disabled)return;button.disabled=true;$("loginError").hidden=true;
    try{const me=await request("/api/auth/login",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({username:$("loginUsername").value.trim(),password:$("loginPassword").value})},true);$("loginPassword").value="";await enterPortal(me);}catch(error){$("loginError").textContent=error.message;$("loginError").hidden=false;}finally{button.disabled=false;}
  });
  $("btnRegister").onclick=()=>{state.signup=true;state.filter="student";openModal();els.modalTitle.textContent="开通个人账号";};
  els.btnAdd.addEventListener("click",()=>{state.signup=false;$("signupPasswordField").hidden=true;$("signupPassword").required=false;});
  $("btnLogout").onclick=async()=>{try{await request("/api/auth/logout",{method:"POST",headers:{"Content-Type":"application/json"},body:"{}"});showLogin();}catch(error){toast(error.message,"error");}};
  $("portalNav").addEventListener("click",event=>{const button=event.target.closest("[data-page]");if(button&&!button.hidden&&!portalState.busy)portalNavigate(button.dataset.page);});
  $("btnPortalRefresh").onclick=async()=>{try{portalState.me=await request("/api/me",{},true);await portalNavigate(portalState.page);}catch(error){portalError(error);}};
  $("courseSelect").onchange=selectCourse;
  $("btnNewCourse").onclick=()=>{$("courseForm").reset();$("courseId").readOnly=false;portalState.editingCourse=false;$("courseId").focus();};
  $("courseForm").addEventListener("submit",event=>{event.preventDefault();portalWrite(async()=>{const course={id:$("courseId").value.trim(),name:$("courseName").value.trim(),credit:Number($("courseCredit").value),teacherId:$("courseTeacher").value.trim()};await request(portalState.editingCourse?`/api/courses/${encodeURIComponent(course.id)}`:"/api/courses",{method:portalState.editingCourse?"PUT":"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(course)});portalState.courseId=course.id;await portalNavigate("courses");toast("课程已保存");});});
  $("btnDeleteCourse").onclick=async()=>{const id=portalState.courseId;if(id&&await confirmPortal("删除当前课程及其全部成绩记录？"))await portalWrite(async()=>{await request(`/api/courses/${encodeURIComponent(id)}`,{method:"DELETE"});portalState.courseId="";await portalNavigate("courses");toast("课程已删除");});};
  $("gradeForm").addEventListener("submit",event=>{event.preventDefault();portalWrite(async()=>{await request(`/api/courses/${encodeURIComponent(portalState.courseId)}/grades`,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({studentId:$("gradeStudent").value.trim(),score:Number($("gradeScore").value)})});$("gradeForm").reset();await selectCourse();toast("成绩已保存，排名已更新");});});
  $("accountForm").addEventListener("submit",event=>{event.preventDefault();portalWrite(async()=>{await request("/api/accounts",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({personId:$("accountPerson").value.trim(),password:$("accountPassword").value})});$("accountForm").reset();await loadAccounts();toast("个人账号已开通");});});
  $("passwordForm").addEventListener("submit",event=>{event.preventDefault();portalWrite(async()=>{await request("/api/auth/password",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({oldPassword:$("oldPassword").value,newPassword:$("newPassword").value})});showLogin("密码已修改，请重新登录");});});
  $("cancelAccountForm").addEventListener("submit",async event=>{event.preventDefault();if(await confirmPortal("确认销户？这将删除本人的登录账号、人员档案和成绩记录，无法撤销。"))await portalWrite(async()=>{await request("/api/auth/cancel",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({password:$("cancelPassword").value})});showLogin("销户已完成");});});
  $("btnExport").onclick=()=>portalWrite(async()=>{const data=await request("/api/storage/export",{},true),url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)],{type:"application/json"})),link=document.createElement("a");link.href=url;link.download="school-personnel-backup.json";document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);toast("全部资料已导出到文件");});
});
