
const API = "/api";

const $ = (id) => document.getElementById(id);

let students = [];
let toastTimer;

// -------------------- COMMON HELPERS --------------------

async function api(url, options = {}) {
  const response = await fetch(API + url, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });

  const text = await response.text();
  let data;

  try {
    data = text ? JSON.parse(text) : {};
  } catch {
    throw new Error("Invalid server response.");
  }

  if (!response.ok) {
    throw new Error(data.error || "Request failed.");
  }

  return data;
}

function showToast(message, success = true) {
  const toast = $("toast");
  toast.textContent = message;
  toast.className = "toast show " + (success ? "success" : "error");

  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    toast.className = "toast";
  }, 3500);
}

function escapeHTML(value) {
  return String(value ?? "").replace(/[&<>"']/g, char => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#039;"
  })[char]);
}

function formatDate(date) {
  if (!date) return "—";
  return new Date(date + "T00:00:00").toLocaleDateString(
    "en-IN",
    { day: "2-digit", month: "short", year: "numeric" }
  );
}

function today() {
  const d = new Date();
  const offset = d.getTimezoneOffset();
  return new Date(d.getTime() - offset * 60000)
    .toISOString().slice(0, 10);
}

function attendancePercentage(present, total) {
  return total > 0
    ? ((present / total) * 100).toFixed(1)
    : "0.0";
}

// -------------------- LOAD DASHBOARD --------------------

async function loadDashboard() {
  try {
    await loadStudents();
    await Promise.all([
      loadTopPerformers(),
      loadSortedMarks(),
      loadAttendanceSummary()
    ]);
  } catch (error) {
    showToast(error.message, false);
  }
}

// -------------------- STUDENTS --------------------

async function loadStudents() {
  students = await api("/students");

  renderStudents();
  updateStudentDropdowns();
  updateDashboardStats();
}

function renderStudents() {
  const query = $("search").value.trim().toLowerCase();

  const filtered = students.filter(s =>
    String(s.id).includes(query) ||
    s.name.toLowerCase().includes(query) ||
    s.department.toLowerCase().includes(query)
  );

  $("studentCount").textContent =
    `${filtered.length} students`;

  $("studentRows").innerHTML = filtered.length
    ? filtered.map(s => {
        const present = Number(s.present || 0);
        const total = Number(s.total || 0);
        const percent = Number(s.attendance || 0);

        return `
          <tr>
            <td>
              <div class="student-cell">
                <div class="student-avatar">
                  ${escapeHTML(s.name.charAt(0).toUpperCase())}
                </div>
                <div>
                  <b>${escapeHTML(s.name)}</b>
                  <small>ID: ${s.id}</small>
                </div>
              </div>
            </td>
            <td>${escapeHTML(s.department)}</td>
            <td>Year ${s.year}</td>
            <td>${present} / ${total}</td>
            <td>${percent.toFixed(1)}%</td>
            <td><b>${s.marks}/100</b></td>
            <td>
              <button class="delete-btn"
                onclick="deleteStudent(${s.id})">
                Delete
              </button>
            </td>
          </tr>`;
      }).join("")
    : `<tr><td colspan="7" class="empty">
        No students found.
       </td></tr>`;
}

function updateStudentDropdowns() {
  const selects = [
    $("attendanceStudent"),
    $("marksStudent"),
    $("historyStudent")
  ];

  selects.forEach(select => {
    const previous = select.value;
    const isHistory = select.id === "historyStudent";

    select.innerHTML =
      `<option value="">${isHistory ? "All students" : "Select student"}</option>`;

    students.forEach(s => {
      const option = document.createElement("option");
      option.value = s.id;
      option.textContent = `${s.name} (ID: ${s.id})`;
      select.appendChild(option);
    });

    if (students.some(s => String(s.id) === previous)) {
      select.value = previous;
    }
  });
}

function updateDashboardStats() {
  $("totalStudents").textContent = students.length;

  const totalPresent = students.reduce(
    (sum, s) => sum + Number(s.present || 0), 0
  );

  const totalSessions = students.reduce(
    (sum, s) => sum + Number(s.total || 0), 0
  );

  const averageAttendance = totalSessions
    ? (totalPresent / totalSessions) * 100
    : 0;

  $("avgAttendance").textContent =
    averageAttendance.toFixed(1) + "%";

  const averageMarks = students.length
    ? students.reduce((sum, s) => sum + Number(s.marks), 0)
      / students.length
    : 0;

  $("avgMarks").textContent = averageMarks.toFixed(1);

  const top = [...students].sort(
    (a, b) => b.marks - a.marks
  )[0];

  $("topScore").textContent = top
    ? `${top.marks}/100`
    : "—";

  $("topName").textContent = top
    ? top.name
    : "No marks recorded";
}

// -------------------- STUDENT REGISTRATION --------------------

function openModal() {
  $("modal").classList.add("show");
}

function closeModal() {
  $("modal").classList.remove("show");
  $("studentForm").reset();
}

$("studentForm").addEventListener("submit", async event => {
  event.preventDefault();

  const student = {
    id: Number($("studentId").value),
    name: $("studentName").value.trim(),
    department: $("studentDepartment").value.trim(),
    year: Number($("studentYear").value)
  };

  try {
    const result = await api("/students", {
      method: "POST",
      body: JSON.stringify(student)
    });

    closeModal();
    await loadDashboard();
    showToast(result.message || "Student registered successfully!");
  } catch (error) {
    showToast(error.message, false);
  }
});

// -------------------- DELETE STUDENT --------------------

async function deleteStudent(id) {
  const student = students.find(s => s.id === id);

  if (!student) return;

  if (!confirm(`Delete ${student.name} (ID: ${id})?`)) {
    return;
  }

  try {
    const result = await api(`/students?id=${id}`, {
      method: "DELETE"
    });

    await loadDashboard();
    await loadAttendanceHistory();

    showToast(result.message || "Student deleted successfully!");
  } catch (error) {
    showToast(error.message, false);
  }
}

// -------------------- ATTENDANCE --------------------

$("attendanceDate").value = today();

$("attendanceForm").addEventListener("submit", async event => {
  event.preventDefault();

  const id = Number($("attendanceStudent").value);
  const date = $("attendanceDate").value;
  const status = $("attendanceStatus").value;

  if (!id || !date) {
    showToast("Select a student and attendance date.", false);
    return;
  }

  try {
    const result = await api("/attendance", {
      method: "POST",
      body: JSON.stringify({ id, date, status })
    });

    showToast(result.message || "Attendance saved successfully!");

    await loadDashboard();
    await loadAttendanceHistory();
  } catch (error) {
    showToast(error.message, false);
  }
});

// -------------------- MARKS --------------------

$("marksForm").addEventListener("submit", async event => {
  event.preventDefault();

  const id = Number($("marksStudent").value);
  const marks = Number($("marksValue").value);

  if (!id || marks < 0 || marks > 100) {
    showToast("Select a student and enter marks from 0 to 100.", false);
    return;
  }

  try {
    const result = await api("/marks", {
      method: "POST",
      body: JSON.stringify({ id, marks })
    });

    $("marksForm").reset();

    await loadDashboard();

    showToast(result.message || "Marks updated successfully!");
  } catch (error) {
    showToast(error.message, false);
  }
});

// -------------------- MAX HEAP RANKING --------------------

async function loadTopPerformers() {
  const ranking = await api("/top");

  $("topList").innerHTML = ranking.length
    ? ranking.map((s, index) => `
        <div class="rank-item">
          <div class="rank-number">${index + 1}</div>
          <div class="rank-info">
            <b>${escapeHTML(s.name)}</b>
            <small>Student ID: ${s.id}</small>
          </div>
          <strong>${s.marks}<small>/100</small></strong>
        </div>
      `).join("")
    : `<div class="empty">No ranking data yet.</div>`;
}

// -------------------- BST MARKS SORTING --------------------

async function loadSortedMarks() {
  const sorted = await api("/sorted");

  $("sortedRows").innerHTML = sorted.length
    ? sorted.map((s, index) => `
        <tr>
          <td>${index + 1}</td>
          <td>${escapeHTML(s.name)}</td>
          <td>${s.id}</td>
          <td><b>${s.marks}/100</b></td>
        </tr>
      `).join("")
    : `<tr><td colspan="4" class="empty">
        No marks available.
       </td></tr>`;
}

// -------------------- OVERALL ATTENDANCE REPORT --------------------

async function loadAttendanceSummary() {
  const month = $("reportMonth").value;

  const url = month
    ? `/attendance/summary?month=${encodeURIComponent(month)}`
    : "/attendance/summary";

  const report = await api(url);

  $("attendanceSummaryRows").innerHTML = report.length
    ? report.map(s => `
        <tr>
          <td>
            <b>${escapeHTML(s.name)}</b>
            <small>ID: ${s.id}</small>
          </td>
          <td>${s.present_days}</td>
          <td>${s.absent_days}</td>
          <td>${s.total_sessions}</td>
          <td>
            <b>${Number(s.attendance).toFixed(1)}%</b>
          </td>
          <td>${s.legacy_sessions || 0}</td>
        </tr>
      `).join("")
    : `<tr><td colspan="6" class="empty">
        No attendance records available.
       </td></tr>`;
}

$("refreshReports").addEventListener(
  "click", loadAttendanceSummary
);

$("allTimeReport").addEventListener("click", () => {
  $("reportMonth").value = "";
  loadAttendanceSummary();
});

// -------------------- INDIVIDUAL ATTENDANCE HISTORY --------------------

async function loadAttendanceHistory() {
  const id = $("historyStudent").value;
  const month = $("historyMonth").value;

  const params = new URLSearchParams();

  if (id) params.set("student_id", id);
  if (month) params.set("month", month);

  const query = params.toString();
  const url = "/attendance" + (query ? "?" + query : "");

  try {
    const records = await api(url);

    const present = records.filter(
      r => r.status === "Present"
    ).length;

    const absent = records.filter(
      r => r.status === "Absent"
    ).length;

    const total = records.length;

    $("historyPresent").textContent = present;
    $("historyAbsent").textContent = absent;
    $("historyTotal").textContent = total;

    $("historyPercentage").textContent =
      attendancePercentage(present, total) + "%";

    $("historyRows").innerHTML = records.length
      ? records.map(r => `
          <tr>
            <td>${formatDate(r.date)}</td>
            <td>${escapeHTML(r.name)}</td>
            <td>${r.id}</td>
            <td>
              <span class="attendance-badge ${
                r.status === "Present" ? "present" : "absent"
              }">
                ${escapeHTML(r.status)}
              </span>
            </td>
          </tr>
        `).join("")
      : `<tr><td colspan="4" class="empty">
          No dated attendance records found.
         </td></tr>`;
  } catch (error) {
    showToast("History: " + error.message, false);
  }
}

$("loadHistory").addEventListener(
  "click", loadAttendanceHistory
);

// -------------------- INITIALIZATION --------------------

document.addEventListener("DOMContentLoaded", () => {
  loadDashboard();
  loadAttendanceHistory();
});
