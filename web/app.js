/* Aurora · 学校人员管理。服务器是唯一的数据来源。 */
"use strict";

const state = {
  people: [], view: [], filter: "all", query: "", sort: "default",
  online: false, loaded: false, loading: false, busy: false,
  editingId: null, formType: "teacher", pendingDeleteId: null,
  returnFocus: null, lastUpdated: null,
};
const $ = (id) => document.getElementById(id);
const els = Object.fromEntries([
  "tableBody", "emptyState", "emptyTitle", "emptySub", "tableSummary",
  "searchInput", "sortSelect", "filters", "statTotal", "statTeacher",
  "statAdmin", "statSalary", "connBadge", "connText", "connectionNotice",
  "modalOverlay", "modalTitle", "confirmOverlay", "confirmText", "toastStack",
  "personForm", "formError", "deleteError", "btnAdd", "btnRetry", "btnSave",
  "btnCloseModal", "btnCancel", "btnConfirmOk", "btnConfirmCancel",
  "btnFormRetry", "btnDeleteRetry",
].map((id) => [id, $(id)]));

const ICONS = {
  success: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><path d="M20 6 9 17l-5-5"/></svg>',
  error: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><path d="M18 6 6 18M6 6l12 12"/></svg>',
  info: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><path d="M12 16v-4M12 8h.01"/></svg>',
  edit: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.12 2.12 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>',
  delete: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>',
};

class ApiError extends Error {
  constructor(message, status = 0) { super(message); this.status = status; }
}

async function request(path, options = {}, requireJson = false) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 15000);
  try {
    const response = await fetch(path, {
      credentials: "same-origin", cache: "no-store", ...options, signal: controller.signal,
      headers: { Accept: "application/json", ...options.headers },
    });
    // HTTP 2xx 已确认写入成功；不要因响应正文损坏而引导用户重复写入。
    if (response.ok && !requireJson) return null;
    let data = null;
    try { data = await response.json(); } catch (_) { /* 下方按状态处理 */ }
    if (!response.ok) {
      const serverMessage = data && (typeof data.error === "string" ? data.error : typeof data.message === "string" ? data.message : "");
      const defaults = {
        401: "身份认证失败，请使用服务器配置的账号重新登录",
        403: "没有执行此操作的权限", 404: "人员记录不存在，请刷新列表",
        409: "工号已存在，请更换", 413: "提交的信息过长",
        429: "请求过于频繁，请稍后再试",
      };
      throw new ApiError(serverMessage || defaults[response.status] || `服务器请求失败（HTTP ${response.status}）`, response.status);
    }
    if (data === null) throw new ApiError("服务器返回了无法读取的数据，请重试");
    return data;
  } catch (error) {
    if (error instanceof ApiError) throw error;
    throw new ApiError(error.name === "AbortError" ? "服务器响应超时" : "无法连接服务器，请检查网络或服务器状态");
  } finally { clearTimeout(timeout); }
}

function toast(message, type = "success") {
  const item = document.createElement("div");
  item.className = `toast glass ${type}`;
  const icon = document.createElement("span");
  icon.className = "t-icon";
  icon.innerHTML = ICONS[type] || ICONS.info; // 仅插入固定 SVG，数据均以文本呈现。
  icon.setAttribute("aria-hidden", "true");
  const text = document.createElement("span");
  text.textContent = message;
  item.append(icon, text);
  els.toastStack.append(item);
  setTimeout(() => item.remove(), type === "error" ? 7000 : 4500);
}

function setConnection(mode, message = "") {
  state.online = mode === "online";
  els.connBadge.classList.toggle("online", state.online);
  els.connBadge.classList.toggle("offline", mode === "offline");
  els.connText.textContent = mode === "online" ? "服务已连接" : mode === "offline" ? "连接异常" : "连接中…";
  els.connBadge.title = mode === "online" ? "人员档案已从服务器加载" : "恢复连接前暂停添加、编辑和删除";
  els.connectionNotice.hidden = mode === "online";
  els.connectionNotice.classList.toggle("notice-error", mode === "offline");
  els.connectionNotice.textContent = message || "正在连接服务器，加载人员信息…";
  syncControls();
}

function syncControls() {
  const cannotWrite = !state.online || state.busy || state.loading;
  els.btnAdd.disabled = cannotWrite;
  els.btnRetry.disabled = state.loading || state.busy;
  els.btnRetry.textContent = state.loading ? "正在加载…" : state.online ? "刷新列表" : "重试连接";
  els.btnSave.disabled = cannotWrite;
  els.btnSave.textContent = state.busy && !els.modalOverlay.hidden ? "保存中…" : "保存";
  els.btnConfirmOk.disabled = cannotWrite;
  els.btnConfirmOk.textContent = state.busy && !els.confirmOverlay.hidden ? "删除中…" : "删除";
  for (const button of els.tableBody.querySelectorAll("button")) button.disabled = cannotWrite;
  for (const control of els.personForm.querySelectorAll("input, select, button")) control.disabled = state.busy;
  for (const button of [els.btnCloseModal, els.btnCancel, els.btnConfirmCancel]) button.disabled = state.busy;
  for (const [button, overlay] of [[els.btnFormRetry, els.modalOverlay], [els.btnDeleteRetry, els.confirmOverlay]]) {
    button.hidden = state.online || overlay.hidden;
    button.disabled = state.loading || state.busy;
  }
  els.personForm.setAttribute("aria-busy", String(state.busy));
}

function validList(data) {
  if (!Array.isArray(data) || data.some((person) => !person || typeof person !== "object" ||
      !["teacher", "admin"].includes(person.type) || typeof person.id !== "string" ||
      typeof person.name !== "string" || !Number.isFinite(person.age) || !Number.isFinite(person.salary))) {
    throw new ApiError("服务器返回的人员列表格式有误，请重试");
  }
  return data;
}

async function refresh(successNotice = "") {
  if (state.loading) return false;
  state.loading = true;
  setConnection("connecting");
  els.tableBody.setAttribute("aria-busy", "true");
  render();
  try {
    state.people = validList(await request("/api/people", {}, true));
    state.loaded = true;
    state.lastUpdated = new Date();
    setConnection("online");
    return true;
  } catch (error) {
    const snapshot = state.loaded ? "以下显示上次加载的结果。" : "尚未加载人员数据。";
    setConnection("offline", `${successNotice}${error.message}。${snapshot}请点击「重试连接」，恢复后即可继续管理。`);
    return false;
  } finally {
    state.loading = false;
    els.tableBody.setAttribute("aria-busy", "false");
    render();
    syncControls();
  }
}

function formatMoney(amount, average = false) {
  return "¥" + Number(amount).toLocaleString("zh-CN", {
    minimumFractionDigits: 2, maximumFractionDigits: average ? 2 : 15,
  });
}

function render() {
  let list = state.people.slice();
  if (state.filter !== "all") list = list.filter((person) => person.type === state.filter);
  if (state.query) {
    const query = state.query.toLocaleLowerCase();
    list = list.filter((person) => person.name.toLocaleLowerCase().includes(query) || person.id.toLocaleLowerCase().includes(query));
  }
  const sorts = {
    "age-asc": (a, b) => a.age - b.age, "age-desc": (a, b) => b.age - a.age,
    "salary-asc": (a, b) => a.salary - b.salary, "salary-desc": (a, b) => b.salary - a.salary,
  };
  if (sorts[state.sort]) list.sort(sorts[state.sort]);
  state.view = list;
  renderStats();
  renderTable();
}

function renderStats() {
  const list = state.people;
  els.statTotal.textContent = state.loaded ? String(list.length) : "—";
  els.statTeacher.textContent = state.loaded ? String(list.filter((person) => person.type === "teacher").length) : "—";
  els.statAdmin.textContent = state.loaded ? String(list.filter((person) => person.type === "admin").length) : "—";
  els.statSalary.textContent = state.loaded ? formatMoney(list.length ? list.reduce((sum, person) => sum + person.salary, 0) / list.length : 0, true) : "—";
}

function cell(text, className = "") {
  const element = document.createElement("td");
  element.textContent = String(text == null ? "—" : text);
  if (className) element.className = className;
  return element;
}

function rowButton(kind, person) {
  const button = document.createElement("button");
  const label = kind === "edit" ? "编辑" : "删除";
  button.className = `icon-btn ${kind === "edit" ? "edit" : "del"}`;
  button.type = "button";
  button.dataset.action = kind;
  button.dataset.id = person.id;
  button.title = label;
  button.setAttribute("aria-label", `${label}${person.name}（${person.id}）`);
  button.innerHTML = ICONS[kind];
  button.disabled = !state.online || state.busy || state.loading;
  return button;
}

function renderTable() {
  const fragment = document.createDocumentFragment();
  state.view.forEach((person, index) => {
    const row = document.createElement("tr");
    row.style.setProperty("--d", `${Math.min(index * 30, 240)}ms`);
    const typeCell = document.createElement("td");
    const badge = document.createElement("span");
    badge.className = `badge ${person.type}`;
    badge.textContent = person.type === "teacher" ? "教师" : "行政";
    typeCell.append(badge);
    row.append(typeCell, cell(person.id, "cell-id"), cell(person.name, "cell-name"),
      cell(person.gender), cell(person.age, "ta-c"), cell(person.phone || "—"),
      cell(person.department || "—"), cell((person.type === "teacher" ? person.title : person.position) || "—"),
      cell(formatMoney(person.salary), "ta-r"));
    const actionsCell = cell("", "ta-c");
    const actions = document.createElement("div");
    actions.className = "row-actions";
    actions.append(rowButton("edit", person), rowButton("delete", person));
    actionsCell.append(actions);
    row.append(actionsCell);
    fragment.append(row);
  });
  els.tableBody.replaceChildren(fragment);
  els.emptyState.hidden = state.view.length > 0;
  if (!state.loaded) {
    els.emptyTitle.textContent = state.loading ? "正在加载人员档案" : "人员数据尚未加载";
    els.emptySub.textContent = state.loading ? "请稍候，正在连接服务器" : "点击「重试连接」加载服务器中的人员信息";
  } else if (!state.people.length) {
    els.emptyTitle.textContent = "暂无人员数据";
    els.emptySub.textContent = state.online ? "点击右上角「添加人员」录入第一份档案" : "恢复连接后可录入人员信息";
  } else {
    els.emptyTitle.textContent = "没有匹配的结果";
    els.emptySub.textContent = "试着更换关键词或人员类别";
  }
  els.tableSummary.textContent = state.loaded
    ? `显示 ${state.view.length} / ${state.people.length} 人 · 统计涵盖全部人员 · ${state.online ? "更新于" : "上次加载于"} ${state.lastUpdated.toLocaleTimeString("zh-CN", { hour12: false })}`
    : "人员信息加载完成后将在此显示";
}

function activateDialog(overlay, focusTarget) {
  state.returnFocus = document.activeElement;
  overlay.hidden = false;
  for (const id of ["pageHeader", "pageMain"]) $(id).inert = true;
  document.body.classList.add("dialog-open");
  focusTarget.focus();
}

function hideDialog(overlay, force = false) {
  if (state.busy && !force) return false;
  overlay.hidden = true;
  for (const id of ["pageHeader", "pageMain"]) $(id).inert = false;
  document.body.classList.remove("dialog-open");
  const previous = state.returnFocus;
  state.returnFocus = null;
  if (previous && previous.isConnected && !previous.disabled) previous.focus();
  else if (!els.btnAdd.disabled) els.btnAdd.focus();
  else els.btnRetry.focus();
  return true;
}

function clearErrors() {
  els.formError.hidden = true;
  for (const control of els.personForm.querySelectorAll("[aria-invalid]")) {
    control.removeAttribute("aria-invalid");
    control.classList.remove("invalid");
  }
}

function openModal(person = null) {
  if (!state.online || state.busy || state.loading) return;
  state.editingId = person ? person.id : null;
  setType(person ? person.type : "teacher");
  els.personForm.reset();
  els.modalTitle.textContent = person ? "编辑人员" : "添加人员";
  $("fieldId").value = person ? person.id : "";
  $("fieldName").value = person ? person.name : "";
  $("fieldGender").value = person ? person.gender : "男";
  $("fieldAge").value = person ? String(person.age) : "";
  $("fieldPhone").value = person ? person.phone || "" : "";
  $("fieldDept").value = person ? person.department || "" : "";
  $("fieldRole").value = person ? (person.type === "teacher" ? person.title : person.position) || "" : "";
  // 直接使用服务器数值，显示金额格式不参与编辑和保存。
  $("fieldSalary").value = person ? String(person.salary) : "";
  clearErrors();
  activateDialog(els.modalOverlay, $("fieldId"));
}

function closeModal(force = false) {
  if (hideDialog(els.modalOverlay, force)) state.editingId = null;
}

function setType(type) {
  if (!["teacher", "admin"].includes(type) || state.busy) return;
  state.formType = type;
  for (const button of $("typeSwitch").querySelectorAll("button")) {
    const active = button.dataset.type === type;
    button.classList.toggle("active", active);
    button.setAttribute("aria-pressed", String(active));
  }
  $("labelDept").textContent = type === "teacher" ? "所属院系 *" : "所属部门 *";
  $("labelRole").textContent = type === "teacher" ? "职称 *" : "职务 *";
  $("fieldDept").placeholder = type === "teacher" ? "如 计算机学院" : "如 财务处";
  $("fieldRole").placeholder = type === "teacher" ? "如 教授" : "如 会计";
}

function invalid(field, message) {
  const control = $(field);
  control.classList.add("invalid");
  control.setAttribute("aria-invalid", "true");
  els.formError.textContent = message;
  els.formError.hidden = false;
  control.focus();
  return null;
}

function readForm() {
  clearErrors();
  const value = (id) => $(id).value.trim();
  const id = value("fieldId"), name = value("fieldName"), gender = value("fieldGender");
  const department = value("fieldDept"), role = value("fieldRole"), phone = value("fieldPhone");
  const ageText = value("fieldAge"), salaryText = value("fieldSalary");
  const age = Number(ageText), salary = Number(salaryText);
  if (!/^[A-Za-z0-9_-]{1,32}$/.test(id)) return invalid("fieldId", "工号须为 1–32 位字母、数字、下划线或连字符");
  if (!name || [...name].length > 64) return invalid("fieldName", "请输入姓名，最多 64 个字符");
  if (!["男", "女", "其他"].includes(gender)) return invalid("fieldGender", "请选择有效的性别");
  if (!ageText || !Number.isInteger(age) || age < 18 || age > 100) return invalid("fieldAge", "年龄须为 18–100 的整数");
  if (!validPhone(phone)) return invalid("fieldPhone", "联系电话须包含 6–20 位数字，最多 32 个字符；可使用开头的 +、配对括号、空格、连字符或小数点");
  if (!department || [...department].length > 128) return invalid("fieldDept", "请输入院系或部门，最多 128 个字符");
  if (!role || [...role].length > 128) return invalid("fieldRole", "请输入职称或职务，最多 128 个字符");
  if (!salaryText || !Number.isFinite(salary) || salary < 0 || salary > 100000000) return invalid("fieldSalary", "工资须为 0–100,000,000 元之间的有效数字");
  if (state.people.some((person) => person.id === id && person.id !== state.editingId)) return invalid("fieldId", "工号已存在，请更换");
  return {
    type: state.formType, id, name, gender, age, phone, department, salary,
    [state.formType === "teacher" ? "title" : "position"]: role,
  };
}

function validPhone(phone) {
  if (!phone) return true;
  if (phone.length > 32 || !/^\+?[0-9() .-]+$/.test(phone)) return false;
  const digits = phone.replace(/\D/g, "").length;
  if (digits < 6 || digits > 20) return false;
  let parentheses = 0;
  for (const character of phone) {
    if (character === "(") { if (parentheses !== 0) return false; parentheses++; }
    if (character === ")") { if (parentheses !== 1) return false; parentheses--; }
  }
  return parentheses === 0;
}

function mutationError(error, target) {
  target.textContent = error.message;
  target.hidden = false;
  if (error.status === 0) {
    const message = `${error.message}。未收到服务器确认，请先重试加载列表核对操作结果，再继续管理。`;
    setConnection("offline", message);
    target.textContent = message;
  } else if ([401, 403].includes(error.status) || error.status >= 500) {
    setConnection("offline", `${error.message}。恢复连接前已暂停写入，请点击「重试连接」。`);
  }
}

async function savePerson() {
  if (!state.online || state.busy || state.loading) return;
  const person = readForm();
  if (!person) return;
  const originalId = state.editingId;
  const editing = originalId !== null;
  state.busy = true;
  syncControls();
  try {
    await request(editing ? `/api/people/${encodeURIComponent(originalId)}` : "/api/people", {
      method: editing ? "PUT" : "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify(person),
    });
  } catch (error) {
    mutationError(error, els.formError);
    return;
  } finally {
    state.busy = false;
    syncControls();
  }
  // 写入成功后关闭表单，再单独刷新。刷新失败不会重新提交或误报保存失败。
  closeModal();
  toast(editing ? "人员信息已修改" : "人员档案已添加");
  const updated = await refresh("本次保存已成功，但列表未能刷新。 ");
  if (!updated) toast("保存已成功，请重试加载列表确认最新信息", "info");
}

function confirmDelete(id) {
  if (!state.online || state.busy || state.loading) return;
  const person = state.people.find((item) => item.id === id);
  if (!person) return;
  state.pendingDeleteId = id;
  els.confirmText.textContent = `确定要删除「${person.name}」（${id}）的档案吗？此操作不可撤销。`;
  els.deleteError.hidden = true;
  activateDialog(els.confirmOverlay, els.btnConfirmCancel);
}

function closeConfirm(force = false) {
  if (hideDialog(els.confirmOverlay, force)) state.pendingDeleteId = null;
}

async function deletePerson() {
  const id = state.pendingDeleteId;
  if (id === null || !state.online || state.busy || state.loading) return;
  state.busy = true;
  els.deleteError.hidden = true;
  syncControls();
  try { await request(`/api/people/${encodeURIComponent(id)}`, { method: "DELETE" }); }
  catch (error) { mutationError(error, els.deleteError); return; }
  finally { state.busy = false; syncControls(); }
  closeConfirm();
  toast("人员档案已删除");
  const updated = await refresh("本次删除已成功，但列表未能刷新。 ");
  if (!updated) toast("删除已成功，请重试加载列表确认最新信息", "info");
}

function bindEvents() {
  els.btnAdd.addEventListener("click", () => openModal());
  const reconnect = async () => {
    if (state.busy) return;
    if (!els.modalOverlay.hidden) closeModal();
    if (!els.confirmOverlay.hidden) closeConfirm();
    if (await refresh()) toast("人员列表已更新");
  };
  for (const button of [els.btnRetry, els.btnFormRetry, els.btnDeleteRetry]) button.addEventListener("click", reconnect);
  els.btnCloseModal.addEventListener("click", () => closeModal());
  els.btnCancel.addEventListener("click", () => closeModal());
  els.personForm.addEventListener("submit", (event) => { event.preventDefault(); savePerson(); });
  els.personForm.addEventListener("input", (event) => {
    event.target.classList.remove("invalid"); event.target.removeAttribute("aria-invalid");
  });
  $("typeSwitch").addEventListener("click", (event) => {
    const button = event.target.closest("button[data-type]");
    if (button) setType(button.dataset.type);
  });
  els.btnConfirmCancel.addEventListener("click", () => closeConfirm());
  els.btnConfirmOk.addEventListener("click", deletePerson);
  for (const [overlay, close] of [[els.modalOverlay, closeModal], [els.confirmOverlay, closeConfirm]]) {
    overlay.addEventListener("click", (event) => { if (event.target === overlay) close(); });
  }
  els.tableBody.addEventListener("click", (event) => {
    const button = event.target.closest("button[data-action]");
    if (!button || button.disabled) return;
    if (button.dataset.action === "edit") openModal(state.people.find((person) => person.id === button.dataset.id));
    else confirmDelete(button.dataset.id);
  });
  els.searchInput.addEventListener("input", () => { state.query = els.searchInput.value.trim(); render(); });
  els.sortSelect.addEventListener("change", () => { state.sort = els.sortSelect.value; render(); });
  els.filters.addEventListener("click", (event) => {
    const chip = event.target.closest("button[data-filter]");
    if (!chip) return;
    state.filter = chip.dataset.filter;
    for (const button of els.filters.querySelectorAll("button")) {
      const active = button === chip;
      button.classList.toggle("active", active); button.setAttribute("aria-pressed", String(active));
    }
    render();
  });
  document.addEventListener("keydown", (event) => {
    const overlay = !els.modalOverlay.hidden ? els.modalOverlay : !els.confirmOverlay.hidden ? els.confirmOverlay : null;
    if (!overlay) return;
    if (event.key === "Escape") {
      event.preventDefault();
      if (overlay === els.modalOverlay) closeModal(); else closeConfirm();
    } else if (event.key === "Tab") {
      const controls = Array.from(overlay.querySelectorAll('button:not(:disabled), input:not(:disabled), select:not(:disabled), [tabindex="0"]'))
        .filter((control) => !control.hidden && control.getClientRects().length > 0);
      const first = controls[0], last = controls[controls.length - 1];
      if (!first) { event.preventDefault(); overlay.querySelector("[role]").focus(); }
      else if (event.shiftKey && (document.activeElement === first || !controls.includes(document.activeElement))) {
        event.preventDefault(); last.focus();
      } else if (!event.shiftKey && (document.activeElement === last || !controls.includes(document.activeElement))) {
        event.preventDefault(); first.focus();
      }
    }
  });
}

document.addEventListener("DOMContentLoaded", () => { bindEvents(); refresh(); });
