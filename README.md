# Student Attendance & Performance Management System (C + Web)

A Windows-based academic mini-project. The backend is written in C and serves a modern HTML/CSS/JavaScript dashboard over HTTP.

## Data structures implemented

1. **Hash Table** — student ID lookup and duplicate checking.
2. **Binary Search Tree (BST)** — marks can be inserted and traversed in descending order. The BST functions are implemented in `backend/main.c` as `bst_insert` and `bst_collect_desc`.
3. **Array** — student records and attendance counters are maintained in arrays/struct fields.
4. **Max Heap** — extracts the five highest-scoring students for the dashboard.

## Requirements

- Windows 10 or Windows 11
- Visual Studio Code
- GCC compiler (recommended: MinGW-w64 / MSYS2)
- VS Code C/C++ extension

## Setup

### 1. Install GCC

Install MSYS2 from https://www.msys2.org/ and open the **MSYS2 UCRT64** terminal. Run:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

Add this folder to Windows PATH:

```text
C:\msys64\ucrt64\bin
```

Close and reopen VS Code. In the VS Code terminal, verify:

```powershell
gcc --version
```

### 2. Open the project

Extract the ZIP file. In VS Code choose **File > Open Folder** and select `Student_Attendance_Performance_C`.

### 3. Compile

Open the VS Code terminal (PowerShell), make sure the terminal is in the project root, and run:

```powershell
gcc backend\main.c -o student_system.exe -lws2_32
```

### 4. Run

```powershell
.\student_system.exe
```

You should see:

```text
Student Attendance & Performance Management System
Server running at http://localhost:8080
Press Ctrl+C to stop.
```

Open this address in Chrome or Edge:

```text
http://localhost:8080
```

Keep the terminal open while using the website. Press **Ctrl+C** in the terminal to stop the server.

## How to use

1. Click **Add student** and register a student ID, name, department and year.
2. Use **Mark attendance** to record Present or Absent for a student.
3. Use **Update marks** to enter a score from 0 to 100.
4. The dashboard recalculates total students, average attendance, average marks and top score.
5. The Top Performers panel displays the five highest scores.
6. Search students by ID, name or department. Delete removes a student record.

## Data storage

Student information is saved in `data/students.csv`. It is loaded automatically when the server starts. Back up this file before deleting or modifying it.

## Notes / project limitations

- This is a learning/demo HTTP server. It is single-threaded and intended for localhost demonstration, not public internet deployment.
- Attendance is stored as total present and total sessions per student. Each submission adds one session; avoid submitting twice for the same class/day.
- Marks store one current score per student (0–100), not a history of multiple exams.
- The BST implementation is included for marks ordering; the current dashboard's top-five display uses the Max Heap.
- The small JSON reader is designed for the simple forms in this project, not arbitrary untrusted JSON.
- The Google Fonts import requires an internet connection; the page uses fallback fonts if unavailable.

## API endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/students` | List all students |
| GET | `/api/top` | Get top five students using Max Heap |
| GET | `/api/sorted` | Get students sorted by marks using BST |
| POST | `/api/students` | Register student |
| POST | `/api/attendance` | Add attendance session |
| POST | `/api/marks` | Update marks |
| DELETE | `/api/students?id=1001` | Delete student |

## Suggested viva explanation

- **Why Hash Table?** Average-case O(1) lookup by student ID.
- **Why BST?** Maintains marks in ordered form; in-order traversal gives sorted marks.
- **Why Array?** Direct indexing and compact storage for the fixed-capacity student collection and attendance counts.
- **Why Max Heap?** Efficiently retrieves the highest mark; insertion/removal are O(log n), maximum lookup is O(1).


## Deploy online using Render (Docker)

1. Push this entire folder to a GitHub repository. `Dockerfile` and `render.yaml` must be in the repository root.
2. Sign in to Render and choose **New + → Blueprint**.
3. Connect the GitHub repository, review the service and persistent disk configuration, then deploy.
4. Open the `onrender.com` URL shown in the Render dashboard.

The Dockerfile compiles the C server for Linux. The C server reads Render's `PORT` variable automatically. `render.yaml` mounts persistent storage at `/var/data`; the application saves student data to `/var/data/students.csv` using `DATA_DIR`.

The Blueprint sets the service to Render's `starter` paid compute plan because Render persistent disks are not supported on Free web services. Check current pricing before deploying. If you prefer a Free web service for a temporary demo, remove the `disk` section and use the Free plan, but student records saved to the container filesystem can be lost when the service restarts or redeploys.

For manual setup, create **New + → Web Service**, connect the repository, choose **Docker**, set `DATA_DIR=/var/data`, and attach a persistent disk mounted at `/var/data` if supported by your plan.

Local Windows run from the project root remains:
```powershell
gcc backend\main.c -o student_system.exe -lws2_32
.\student_system.exe
```
Then open `http://localhost:8080`.

This is an educational demo server, not a production student information system. Do not use real sensitive student records on a public deployment without authentication, access controls, security hardening and appropriate privacy safeguards.
