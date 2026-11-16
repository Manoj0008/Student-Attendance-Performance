let students = [];

async function api(url, options = {}) {
  const response = await fetch(url, {
    headers: { "Content-Type": "application/json" },
    ...options
  });
  const data = await response.json();
  if (!response.ok) throw new Error(data.error || "Request failed");
  return data;
}

function toast(message) {
  const el = document.getElementById("toast");
  el.textContent = message;
  el.classList.add("show");
  setTimeout(() => el.classList.remove("show"), 2600);
}
function openModal() { document.getElementById("modal").classList.add("show"); }
function closeModal() { document.getElementById("modal").classList.remove("show"); }

async function refresh() {
  try {
    students = await api("/api/students");
    renderStudents();
    renderSelects();
    updateStats();
    renderTop(await api("/api/top"));
  } catch (e) { toast(e.message); }
}

function renderStudents() {
  const q = document.getElementById("search").value.toLowerCase();
  const list = students.filter(s =>
    String(s.id).includes(q) || s.name.toLowerCase().includes(q) ||
    s.department.toLowerCase().includes(q)
  );
  document.getElementById("studentCount").textContent = `${list.length} student${list.length === 1 ? "" : "s"}`;
  const tbody = document.getElementById("studentRows");
  if (!list.length) {
    tbody.innerHTML = `<tr><td colspan="6" class="empty">${students.length ? "No matching students." : "No students yet. Add your first student."}</td></tr>`;
    return;
  }
  tbody.innerHTML = list.map(s => {
    const initials = s.name.split(/\s+/).map(x => x[0]).slice(0,2).join("").toUpperCase();
    return `<tr>
      <td><div class="student-cell"><div class="student-avatar">${escapeHtml(initials)}</div><div><b>${escapeHtml(s.name)}</b><small>ID: ${s.id}</small></div></div></td>
      <td>${escapeHtml(s.department)}</td><td>Year ${s.year}</td>
      <td>${s.attendance.toFixed(1)}%</td><td><span class="mark">${s.marks}/100</span></td>
      <td><button class="delete" onclick="deleteStudent(${s.id})" title="Delete student">Delete</button></td>
    </tr>`;
  }).join("");
}
function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function renderSelects() {
  const options = `<option value="">Select student</option>` + students.map(s =>
    `<option value="${s.id}">${escapeHtml(s.name)} (ID: ${s.id})</option>`).join("");
  document.getElementById("attendanceStudent").innerHTML = options;
  document.getElementById("marksStudent").innerHTML = options;
}
function updateStats() {
  const count = students.length;
  const avgAttendance = count ? students.reduce((a,s)=>a+s.attendance,0)/count : 0;
  const avgMarks = count ? students.reduce((a,s)=>a+s.marks,0)/count : 0;
  const top = [...students].sort((a,b)=>b.marks-a.marks)[0];
  document.getElementById("totalStudents").textContent = count;
  document.getElementById("avgAttendance").textContent = `${avgAttendance.toFixed(1)}%`;
  document.getElementById("avgMarks").textContent = avgMarks.toFixed(1);
  document.getElementById("topScore").textContent = top ? `${top.marks}/100` : "—";
  document.getElementById("topName").textContent = top ? top.name : "No marks recorded";
}
function renderTop(top) {
  const el = document.getElementById("topList");
  el.innerHTML = top.length ? top.map((s,i)=>`
    <div class="top-item"><div class="rank">${i+1}</div>
    <div class="top-info"><b>${escapeHtml(s.name)}</b><small>Student ID: ${s.id}</small></div>
    <div class="score">${s.marks}<small style="color:#9aa3af;font:10px 'DM Sans'"> /100</small></div></div>`).join("")
    : `<div class="empty">No ranking data yet.</div>`;
}

document.getElementById("studentForm").addEventListener("submit", async e => {
  e.preventDefault();
  const payload = {
    id: Number(document.getElementById("studentId").value),
    name: document.getElementById("studentName").value.trim(),
    department: document.getElementById("studentDepartment").value.trim(),
    year: Number(document.getElementById("studentYear").value)
  };
  try {
    const result = await api("/api/students", {method:"POST", body:JSON.stringify(payload)});
    toast(result.message); e.target.reset(); closeModal(); await refresh();
  } catch (err) { toast(err.message); }
});
document.getElementById("attendanceForm").addEventListener("submit", async e => {
  e.preventDefault();
  try {
    const result = await api("/api/attendance", {method:"POST",body:JSON.stringify({
      id:Number(document.getElementById("attendanceStudent").value),
      status:document.getElementById("attendanceStatus").value
    })});
    toast(result.message); e.target.reset(); await refresh();
  } catch (err) { toast(err.message); }
});
document.getElementById("marksForm").addEventListener("submit", async e => {
  e.preventDefault();
  try {
    const result = await api("/api/marks", {method:"POST",body:JSON.stringify({
      id:Number(document.getElementById("marksStudent").value),
      marks:Number(document.getElementById("marksValue").value)
    })});
    toast(result.message); e.target.reset(); await refresh();
  } catch (err) { toast(err.message); }
});
async function deleteStudent(id) {
  if (!confirm("Delete this student record?")) return;
  try { const result = await api(`/api/students?id=${id}`, {method:"DELETE"}); toast(result.message); await refresh(); }
  catch (err) { toast(err.message); }
}
refresh();
