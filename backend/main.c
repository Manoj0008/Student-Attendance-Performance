#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <curl/curl.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#define SOCKET_ERROR_CODE SOCKET_ERROR
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR_CODE (-1)
#define CLOSE_SOCKET close
#endif

#define DEFAULT_PORT 8080
#define MAX_STUDENTS 1000
#define MAX_ATTENDANCE 100000
#define HASH_SIZE 101
#define BUF_SIZE 65536

#define DEFAULT_DATA_FILE "data/students.csv"
#define DEFAULT_ATTENDANCE_FILE "data/attendance.csv"

typedef struct Student {
    int id;
    char name[80];
    char department[60];
    int year;
    int marks;
    int present;
    int total;
    struct Student *next;
} Student;

typedef struct BSTNode {
    int marks;
    int id;
    struct BSTNode *left;
    struct BSTNode *right;
} BSTNode;

typedef struct {
    int id;
    char date[11];
    char status[8];
} AttendanceRecord;

typedef struct {
    Student *buckets[HASH_SIZE];
} HashTable;

static HashTable table;

static Student students[MAX_STUDENTS];
static int student_count = 0;

static AttendanceRecord attendance_records[MAX_ATTENDANCE];
static int attendance_count = 0;

static char data_file[512] = DEFAULT_DATA_FILE;
static char attendance_file[512] = DEFAULT_ATTENDANCE_FILE;

/* =====================================================
   HASH TABLE
===================================================== */

static int hash_id(int id) {
    return (id < 0 ? -id : id) % HASH_SIZE;
}

static Student *find_student(int id) {
    Student *p = table.buckets[hash_id(id)];

    while (p) {
        if (p->id == id)
            return p;

        p = p->next;
    }

    return NULL;
}

static void hash_insert(Student *s) {
    int h = hash_id(s->id);

    s->next = table.buckets[h];
    table.buckets[h] = s;
}

static void hash_remove(int id) {
    int h = hash_id(id);
    Student **p = &table.buckets[h];

    while (*p) {
        if ((*p)->id == id) {
            *p = (*p)->next;
            return;
        }

        p = &(*p)->next;
    }
}

static void rebuild_hash(void) {
    memset(&table, 0, sizeof(table));

    for (int i = 0; i < student_count; i++)
        hash_insert(&students[i]);
}

/* =====================================================
   FILE STORAGE
===================================================== */

static void save_data(void) {
    FILE *f = fopen(data_file, "w");

    if (!f) {
        printf("Unable to save student data.\n");
        return;
    }

    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i];

        fprintf(f, "%d|%s|%s|%d|%d|%d|%d\n",
                s->id,
                s->name,
                s->department,
                s->year,
                s->marks,
                s->present,
                s->total);
    }

    fclose(f);
}

static void load_data(void) {
    FILE *f = fopen(data_file, "r");

    if (!f)
        return;

    char line[512];

    while (fgets(line, sizeof(line), f) &&
           student_count < MAX_STUDENTS) {

        Student s = {0};

        char *parts[7];
        char *p = line;
        int n = 0;

        while (n < 7 &&
               (parts[n] = strtok(n == 0 ? p : NULL, "|")) != NULL)
            n++;

        if (n != 7)
            continue;

        s.id = atoi(parts[0]);

        snprintf(s.name, sizeof(s.name), "%s", parts[1]);
        snprintf(s.department, sizeof(s.department), "%s", parts[2]);

        s.year = atoi(parts[3]);
        s.marks = atoi(parts[4]);
        s.present = atoi(parts[5]);
        s.total = atoi(parts[6]);

        s.name[strcspn(s.name, "\r\n")] = 0;
        s.department[strcspn(s.department, "\r\n")] = 0;

        if (s.id > 0 &&
            s.year >= 1 && s.year <= 5 &&
            s.marks >= 0 && s.marks <= 100 &&
            s.present >= 0 && s.total >= s.present &&
            !find_student(s.id)) {

            students[student_count++] = s;
            hash_insert(&students[student_count - 1]);
        }
    }

    fclose(f);
}

/* =====================================================
   ATTENDANCE FILE STORAGE
===================================================== */

static void save_attendance(void) {
    FILE *f = fopen(attendance_file, "w");

    if (!f) {
        printf("Unable to save attendance data.\n");
        return;
    }

    for (int i = 0; i < attendance_count; i++) {
        AttendanceRecord *r = &attendance_records[i];

        fprintf(f, "%d|%s|%s\n",
                r->id, r->date, r->status);
    }

    fclose(f);
}

static int valid_date(const char *date) {
    if (!date || strlen(date) != 10)
        return 0;

    if (date[4] != '-' || date[7] != '-')
        return 0;

    for (int i = 0; i < 10; i++) {
        if (i != 4 && i != 7 &&
            !isdigit((unsigned char)date[i]))
            return 0;
    }

    int year = atoi(date);
    int month = (date[5] - '0') * 10 + date[6] - '0';
    int day = (date[8] - '0') * 10 + date[9] - '0';

    if (year < 2000 || year > 9999 ||
        month < 1 || month > 12)
        return 0;

    int days[] = {
        31,28,31,30,31,30,
        31,31,30,31,30,31
    };

    int leap = (year % 400 == 0) ||
               (year % 4 == 0 && year % 100 != 0);

    if (leap)
        days[1] = 29;

    return day >= 1 && day <= days[month - 1];
}

static void load_attendance(void) {
    FILE *f = fopen(attendance_file, "r");

    if (!f)
        return;

    char line[128];

    while (fgets(line, sizeof(line), f) &&
           attendance_count < MAX_ATTENDANCE) {

        AttendanceRecord r = {0};

        if (sscanf(line, "%d|%10[^|]|%7s",
                   &r.id, r.date, r.status) != 3)
            continue;

        if (!find_student(r.id) ||
            !valid_date(r.date))
            continue;

        if (strcmp(r.status, "Present") != 0 &&
            strcmp(r.status, "Absent") != 0)
            continue;

        int duplicate = 0;

        for (int i = 0; i < attendance_count; i++) {
            if (attendance_records[i].id == r.id &&
                strcmp(attendance_records[i].date, r.date) == 0) {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate)
            attendance_records[attendance_count++] = r;
    }

    fclose(f);
}

static int attendance_exists(int id, const char *date) {
    for (int i = 0; i < attendance_count; i++) {
        if (attendance_records[i].id == id &&
            strcmp(attendance_records[i].date, date) == 0)
            return 1;
    }

    return 0;
}

static int month_matches(const char *date, const char *month) {
    return !month[0] || strncmp(date, month, 7) == 0;
}

/* =====================================================
   BINARY SEARCH TREE
===================================================== */

static BSTNode *bst_insert(BSTNode *root, int marks, int id) {
    if (!root) {
        BSTNode *n = calloc(1, sizeof(BSTNode));

        if (n) {
            n->marks = marks;
            n->id = id;
        }

        return n;
    }

    if (marks < root->marks ||
        (marks == root->marks && id < root->id))
        root->left = bst_insert(root->left, marks, id);
    else
        root->right = bst_insert(root->right, marks, id);

    return root;
}

static void bst_free(BSTNode *root) {
    if (!root)
        return;

    bst_free(root->left);
    bst_free(root->right);
    free(root);
}

static void bst_collect_desc(
    BSTNode *root, int *ids, int *marks, int *count) {

    if (!root || *count >= MAX_STUDENTS)
        return;

    bst_collect_desc(root->right, ids, marks, count);

    ids[*count] = root->id;
    marks[*count] = root->marks;
    (*count)++;

    bst_collect_desc(root->left, ids, marks, count);
}

/* =====================================================
   MAX HEAP
===================================================== */

static void heap_swap(Student **a, Student **b) {
    Student *t = *a;
    *a = *b;
    *b = t;
}

static void heap_push(Student **heap, int *size, Student *s) {
    int i = (*size)++;

    heap[i] = s;

    while (i > 0) {
        int parent = (i - 1) / 2;

        if (heap[parent]->marks >= heap[i]->marks)
            break;

        heap_swap(&heap[parent], &heap[i]);
        i = parent;
    }
}

static Student *heap_pop(Student **heap, int *size) {
    if (*size <= 0)
        return NULL;

    Student *top = heap[0];

    heap[0] = heap[--(*size)];

    int i = 0;

    for (;;) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int largest = i;

        if (left < *size &&
            heap[left]->marks > heap[largest]->marks)
            largest = left;

        if (right < *size &&
            heap[right]->marks > heap[largest]->marks)
            largest = right;

        if (largest == i)
            break;

        heap_swap(&heap[i], &heap[largest]);
        i = largest;
    }

    return top;
}

/* =====================================================
   JSON HELPERS
===================================================== */

static void json_escape(
    const char *src, char *dst, size_t cap) {

    size_t j = 0;

    for (size_t i = 0; src[i] && j + 2 < cap; i++) {
        unsigned char c = (unsigned char)src[i];

        if (c == '"' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        }
        else if (c == '\n') {
            dst[j++] = '\\';
            dst[j++] = 'n';
        }
        else if (c == '\r') {
            dst[j++] = '\\';
            dst[j++] = 'r';
        }
        else if (c == '\t') {
            dst[j++] = '\\';
            dst[j++] = 't';
        }
        else if (c >= 32) {
            dst[j++] = (char)c;
        }
    }

    dst[j] = 0;
}

static int get_int(
    const char *json, const char *key, int fallback) {

    char pattern[80];

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);

    if (!p || !(p = strchr(p, ':')))
        return fallback;

    p++;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == '-')
        return atoi(p);

    if (!isdigit((unsigned char)*p))
        return fallback;

    return atoi(p);
}

static void get_string(
    const char *json, const char *key,
    char *out, size_t cap) {

    char pattern[80];

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *p = strstr(json, pattern);

    out[0] = 0;

    if (!p || !(p = strchr(p, ':')))
        return;

    p++;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != '"')
        return;

    p++;

    size_t j = 0;

    while (*p && *p != '"' && j + 1 < cap) {
        if (*p == '\\' && p[1])
            p++;

        out[j++] = *p++;
    }

    out[j] = 0;

    for (size_t i = 0; out[i]; i++) {
        if (out[i] == '|' ||
            out[i] == '\r' ||
            out[i] == '\n')
            out[i] = ' ';
    }
}

/* =====================================================
   URL QUERY HELPERS
===================================================== */

static int get_query_int(
    const char *query, const char *key, int fallback) {

    char pattern[80];

    snprintf(pattern, sizeof(pattern), "%s=", key);

    const char *p = strstr(query, pattern);

    if (!p)
        return fallback;

    if (p != query && p[-1] != '&')
        return fallback;

    p += strlen(pattern);

    if (!isdigit((unsigned char)*p))
        return fallback;

    return atoi(p);
}

static void get_query_month(
    const char *query, char *month, size_t cap) {

    month[0] = 0;

    const char *p = strstr(query, "month=");

    if (!p)
        return;

    if (p != query && p[-1] != '&')
        return;

    p += 6;

    char value[8] = {0};

    if (strlen(p) < 7)
        return;

    memcpy(value, p, 7);

    if (value[4] != '-')
        return;

    for (int i = 0; i < 7; i++) {
        if (i == 4)
            continue;

        if (!isdigit((unsigned char)value[i]))
            return;
    }

    int month_number =
        (value[5] - '0') * 10 + value[6] - '0';

    if (month_number < 1 || month_number > 12)
        return;

    snprintf(month, cap, "%s", value);
}

/* =====================================================
   HTTP RESPONSE
===================================================== */

static void send_response(
    socket_t c, int status,
    const char *type, const char *body) {

    char header[1024];

    const char *reason =
        status == 200 ? "OK" :
        status == 201 ? "Created" :
        status == 400 ? "Bad Request" :
        status == 404 ? "Not Found" :
        status == 409 ? "Conflict" :
        "Internal Server Error";

    int len = (int)strlen(body);

    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n\r\n",
        status, reason, type, len);

    send(c, header, (int)strlen(header), 0);
    send(c, body, len, 0);
}

/* =====================================================
   STUDENT API
===================================================== */

static void send_students(socket_t c) {
    char out[BUF_SIZE];
    size_t used = 0;

    used += (size_t)snprintf(
        out + used, sizeof(out) - used, "[");

    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i];

        char name[180];
        char dept[140];

        json_escape(s->name, name, sizeof(name));
        json_escape(s->department, dept, sizeof(dept));

        int n = snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\","
            "\"department\":\"%s\",\"year\":%d,"
            "\"marks\":%d,\"present\":%d,\"total\":%d,"
            "\"attendance\":%.1f}",
            i ? "," : "",
            s->id, name, dept, s->year,
            s->marks, s->present, s->total,
            s->total ? 100.0 * s->present / s->total : 0.0);

        if (n < 0 || (size_t)n >= sizeof(out) - used)
            break;

        used += (size_t)n;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    send_response(c, 200, "application/json", out);
}

/* =====================================================
   BST SORTED STUDENTS API
===================================================== */

static void send_sorted(socket_t c) {
    BSTNode *root = NULL;

    for (int i = 0; i < student_count; i++)
        root = bst_insert(root, students[i].marks, students[i].id);

    int ids[MAX_STUDENTS];
    int marks[MAX_STUDENTS];
    int count = 0;

    bst_collect_desc(root, ids, marks, &count);

    char out[BUF_SIZE];
    size_t used = 0;

    used += (size_t)snprintf(
        out + used, sizeof(out) - used, "[");

    for (int i = 0; i < count; i++) {
        Student *s = find_student(ids[i]);

        if (!s)
            continue;

        char name[180];

        json_escape(s->name, name, sizeof(name));

        int n = snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\",\"marks\":%d}",
            i ? "," : "",
            s->id, name, marks[i]);

        if (n < 0 || (size_t)n >= sizeof(out) - used)
            break;

        used += (size_t)n;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    bst_free(root);

    send_response(c, 200, "application/json", out);
}

/* =====================================================
   MAX HEAP TOP FIVE API
===================================================== */

static void send_top(socket_t c) {
    Student *heap[MAX_STUDENTS];
    int size = 0;

    for (int i = 0; i < student_count; i++)
        heap_push(heap, &size, &students[i]);

    char out[8192];
    size_t used = 0;

    used += (size_t)snprintf(
        out + used, sizeof(out) - used, "[");

    int limit = size < 5 ? size : 5;

    for (int i = 0; i < limit; i++) {
        Student *s = heap_pop(heap, &size);

        char name[180];

        json_escape(s->name, name, sizeof(name));

        int n = snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\",\"marks\":%d}",
            i ? "," : "",
            s->id, name, s->marks);

        if (n < 0 || (size_t)n >= sizeof(out) - used)
            break;

        used += (size_t)n;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    send_response(c, 200, "application/json", out);
}

/* =====================================================
   DATE-WISE ATTENDANCE HISTORY
===================================================== */

static void send_attendance_history(
    socket_t c, int student_id, const char *month) {

    char out[BUF_SIZE];
    size_t used = 0;

    used += (size_t)snprintf(
        out + used, sizeof(out) - used, "[");

    int first = 1;

    for (int i = 0; i < attendance_count; i++) {
        AttendanceRecord *r = &attendance_records[i];

        if (student_id > 0 && r->id != student_id)
            continue;

        if (!month_matches(r->date, month))
            continue;

        Student *s = find_student(r->id);

        if (!s)
            continue;

        char name[180];

        json_escape(s->name, name, sizeof(name));

        int n = snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\","
            "\"date\":\"%s\",\"status\":\"%s\"}",
            first ? "" : ",",
            r->id, name, r->date, r->status);

        if (n < 0 || (size_t)n >= sizeof(out) - used)
            break;

        used += (size_t)n;
        first = 0;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    send_response(c, 200, "application/json", out);
}

/* =====================================================
   OVERALL AND MONTHLY ATTENDANCE SUMMARY
===================================================== */

static void send_attendance_summary(
    socket_t c, const char *month) {

    char out[BUF_SIZE];
    size_t used = 0;

    used += (size_t)snprintf(
        out + used, sizeof(out) - used, "[");

    int first = 1;

    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i];

        int dated_present = 0;
        int dated_absent = 0;

        for (int j = 0; j < attendance_count; j++) {
            AttendanceRecord *r = &attendance_records[j];

            if (r->id != s->id ||
                !month_matches(r->date, month))
                continue;

            if (strcmp(r->status, "Present") == 0)
                dated_present++;
            else
                dated_absent++;
        }

        int present = dated_present;
        int absent = dated_absent;
        int legacy = 0;

        if (!month[0]) {
            legacy = s->total - dated_present - dated_absent;

            if (legacy < 0)
                legacy = 0;

            int legacy_present = s->present - dated_present;

            if (legacy_present < 0)
                legacy_present = 0;

            if (legacy_present > legacy)
                legacy_present = legacy;

            present += legacy_present;
            absent += legacy - legacy_present;
        }

        int total = present + absent;

        double percentage = total
            ? 100.0 * present / total
            : 0.0;

        char name[180];

        json_escape(s->name, name, sizeof(name));

        int n = snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\","
            "\"present_days\":%d,"
            "\"absent_days\":%d,"
            "\"total_sessions\":%d,"
            "\"attendance\":%.1f,"
            "\"legacy_sessions\":%d}",
            first ? "" : ",",
            s->id, name,
            present, absent, total,
            percentage, legacy);

        if (n < 0 || (size_t)n >= sizeof(out) - used)
            break;

        used += (size_t)n;
        first = 0;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    send_response(c, 200, "application/json", out);
}

/* =====================================================
   HTTP CONTENT LENGTH
===================================================== */

static int get_content_length(
    const char *request, int *content_length) {

    const char *headers_end = strstr(request, "\r\n\r\n");

    if (!headers_end)
        return 0;

    const char *line = request;

    while (line < headers_end) {
        const char *line_end = strstr(line, "\r\n");

        if (!line_end || line_end > headers_end)
            break;

        const char *colon = memchr(
            line, ':', (size_t)(line_end - line));

        if (colon) {
            size_t name_len = (size_t)(colon - line);

            if (name_len == 14 &&
                strncasecmp(line, "Content-Length", 14) == 0) {

                const char *value = colon + 1;

                while (value < line_end &&
                       isspace((unsigned char)*value))
                    value++;

                char number[32];

                size_t len = (size_t)(line_end - value);

                if (!len || len >= sizeof(number))
                    return 0;

                memcpy(number, value, len);
                number[len] = '\0';

                char *endptr = NULL;
                long parsed = strtol(number, &endptr, 10);

                if (endptr == number || *endptr ||
                    parsed < 0 || parsed > BUF_SIZE)
                    return 0;

                *content_length = (int)parsed;
                return 1;
            }
        }

        line = line_end + 2;
    }

    *content_length = 0;
    return 1;
}

/* =====================================================
   FRONTEND FILE SERVING
===================================================== */

static void send_file(
    socket_t c, const char *filename, const char *type) {

    FILE *f = fopen(filename, "rb");

    if (!f) {
        send_response(c, 404, "text/plain",
                      "Frontend file not found.");
        return;
    }

    char buffer[BUF_SIZE];

    size_t n = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);

    buffer[n] = '\0';

    send_response(c, 200, type, buffer);
}

/* =====================================================
   HTTP CLIENT HANDLER
===================================================== */

static void handle_client(socket_t c) {
    char req[BUF_SIZE] = {0};

    int total = 0;
    int header_bytes = 0;
    int content_length = 0;
    int expected_bytes = 0;

    while (total < BUF_SIZE - 1) {
        int got = recv(
            c,
            req + total,
            (int)(BUF_SIZE - 1 - total),
            0);

        if (got <= 0) {
            CLOSE_SOCKET(c);
            return;
        }

        total += got;
        req[total] = '\0';

        if (!header_bytes) {
            char *separator = strstr(req, "\r\n\r\n");

            if (separator) {
                header_bytes = (int)(separator - req) + 4;

                if (!get_content_length(req, &content_length) ||
                    content_length > BUF_SIZE - header_bytes - 1) {

                    send_response(
                        c, 400, "application/json",
                        "{\"error\":\"Invalid or oversized request.\"}");

                    CLOSE_SOCKET(c);
                    return;
                }

                expected_bytes = header_bytes + content_length;
            }
        }

        if (header_bytes > 0 && total >= expected_bytes)
            break;
    }

    if (!header_bytes || total < expected_bytes) {
        send_response(
            c, 400, "application/json",
            "{\"error\":\"Incomplete HTTP request.\"}");

        CLOSE_SOCKET(c);
        return;
    }

    char method[12] = {0};
    char path[256] = {0};
    char query[256] = {0};

    sscanf(req, "%11s %255s", method, path);

    char *question = strchr(path, '?');

    if (question) {
        snprintf(query, sizeof(query), "%s", question + 1);
        *question = '\0';
    }

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(c, 200, "text/plain", "");
        CLOSE_SOCKET(c);
        return;
    }

    char *body = strstr(req, "\r\n\r\n");

    if (body)
        body += 4;
    else
        body = "";

    /* GET STUDENTS */

    if (strcmp(method, "GET") == 0 &&
        strcmp(path, "/api/students") == 0) {

        send_students(c);
    }

    /* GET TOP STUDENTS */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/api/top") == 0) {

        send_top(c);
    }

    /* GET BST SORTED STUDENTS */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/api/sorted") == 0) {

        send_sorted(c);
    }

    /* GET ATTENDANCE HISTORY */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/api/attendance") == 0) {

        int id = get_query_int(query, "student_id", 0);

        char month[8] = {0};
        get_query_month(query, month, sizeof(month));

        send_attendance_history(c, id, month);
    }

    /* GET ATTENDANCE SUMMARY */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/api/attendance/summary") == 0) {

        char month[8] = {0};

        get_query_month(query, month, sizeof(month));

        send_attendance_summary(c, month);
    }

    /* REGISTER STUDENT */

    else if (strcmp(method, "POST") == 0 &&
             strcmp(path, "/api/students") == 0) {

        int id = get_int(body, "id", 0);
        int year = get_int(body, "year", 1);

        char name[80], dept[60];

        get_string(body, "name", name, sizeof(name));
        get_string(body, "department", dept, sizeof(dept));

        if (id <= 0 || !name[0] || !dept[0] ||
            year < 1 || year > 5) {

            send_response(
                c, 400, "application/json",
                "{\"error\":\"Enter a valid ID, name, department and year (1-5).\"}");
        }

        else if (find_student(id)) {
            send_response(
                c, 409, "application/json",
                "{\"error\":\"Student ID already exists.\"}");
        }

        else if (student_count >= MAX_STUDENTS) {
            send_response(
                c, 400, "application/json",
                "{\"error\":\"Student limit reached.\"}");
        }

        else {
            Student *s = &students[student_count++];

            memset(s, 0, sizeof(*s));

            s->id = id;
            s->year = year;

            strcpy(s->name, name);
            strcpy(s->department, dept);

            hash_insert(s);
            save_data();

            send_response(
                c, 201, "application/json",
                "{\"message\":\"Student registered successfully.\"}");
        }
    }

    /* DATE-WISE ATTENDANCE */

    else if (strcmp(method, "POST") == 0 &&
             strcmp(path, "/api/attendance") == 0) {

        int id = get_int(body, "id", 0);

        char status[20] = {0};
        char date[16] = {0};

        get_string(body, "status", status, sizeof(status));
        get_string(body, "date", date, sizeof(date));

        Student *s = find_student(id);

        if (!s) {
            send_response(
                c, 404, "application/json",
                "{\"error\":\"Student not found.\"}");
        }

        else if (!valid_date(date)) {
            send_response(
                c, 400, "application/json",
                "{\"error\":\"Enter a valid date in YYYY-MM-DD format.\"}");
        }

        else if (strcmp(status, "Present") &&
                 strcmp(status, "Absent")) {

            send_response(
                c, 400, "application/json",
                "{\"error\":\"Status must be Present or Absent.\"}");
        }

        else if (attendance_exists(id, date)) {

            send_response(
                c, 409, "application/json",
                "{\"error\":\"Attendance already recorded for this student on this date.\"}");
        }

        else if (attendance_count >= MAX_ATTENDANCE) {

            send_response(
                c, 400, "application/json",
                "{\"error\":\"Attendance record limit reached.\"}");
        }

        else {
            AttendanceRecord *r =
                &attendance_records[attendance_count++];

            r->id = id;

            snprintf(r->date, sizeof(r->date), "%s", date);
            snprintf(r->status, sizeof(r->status), "%s", status);

            s->total++;

            if (strcmp(status, "Present") == 0)
                s->present++;

            save_attendance();
            save_data();

            send_response(
                c, 201, "application/json",
                "{\"message\":\"Attendance saved successfully.\"}");
        }
    }

    /* UPDATE MARKS */

    else if (strcmp(method, "POST") == 0 &&
             strcmp(path, "/api/marks") == 0) {

        int id = get_int(body, "id", 0);
        int marks = get_int(body, "marks", -1);

        Student *s = find_student(id);

        if (!s) {
            send_response(
                c, 404, "application/json",
                "{\"error\":\"Student not found.\"}");
        }

        else if (marks < 0 || marks > 100) {
            send_response(
                c, 400, "application/json",
                "{\"error\":\"Marks must be between 0 and 100.\"}");
        }

        else {
            s->marks = marks;
            save_data();

            send_response(
                c, 200, "application/json",
                "{\"message\":\"Marks updated successfully.\"}");
        }
    }

    /* DELETE STUDENT */

    else if (strcmp(method, "DELETE") == 0 &&
             strcmp(path, "/api/students") == 0) {

        int id = get_query_int(query, "id", 0);
        int idx = -1;

        for (int i = 0; i < student_count; i++) {
            if (students[i].id == id) {
                idx = i;
                break;
            }
        }

        if (idx < 0) {
            send_response(
                c, 404, "application/json",
                "{\"error\":\"Student not found.\"}");
        }

        else {
            hash_remove(id);

            for (int i = idx; i < student_count - 1; i++)
                students[i] = students[i + 1];

            student_count--;

            /* Remove this student's attendance records */

            for (int i = 0; i < attendance_count;) {
                if (attendance_records[i].id == id) {

                    for (int j = i; j < attendance_count - 1; j++)
                        attendance_records[j] =
                            attendance_records[j + 1];

                    attendance_count--;
                }
                else {
                    i++;
                }
            }

            rebuild_hash();

            save_attendance();
            save_data();

            send_response(
                c, 200, "application/json",
                "{\"message\":\"Student deleted successfully.\"}");
        }
    }

    /* FRONTEND HTML */

    else if (strcmp(method, "GET") == 0 &&
             (strcmp(path, "/") == 0 ||
              strcmp(path, "/index.html") == 0)) {

        FILE *f = fopen("frontend/index.html", "rb");

        if (!f)
            f = fopen("../frontend/index.html", "rb");

        if (!f) {
            send_response(
                c, 404, "text/plain",
                "Frontend HTML not found.");
        }
        else {
            char page[BUF_SIZE];

            size_t n = fread(page, 1, sizeof(page) - 1, f);

            page[n] = '\0';

            fclose(f);

            send_response(
                c, 200, "text/html; charset=utf-8", page);
        }
    }

    /* FRONTEND CSS */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/style.css") == 0) {

        send_file(c, "frontend/style.css", "text/css");
    }

    /* FRONTEND JAVASCRIPT */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/script.js") == 0) {

        send_file(
            c, "frontend/script.js",
            "application/javascript");
    }

    else {
        send_response(
            c, 404, "application/json",
            "{\"error\":\"Route not found.\"}");
    }

    CLOSE_SOCKET(c);
}

/* =====================================================
   MAIN SERVER
===================================================== */

int main(void) {
    int port = DEFAULT_PORT;

    const char *port_env = getenv("PORT");
    const char *data_dir = getenv("DATA_DIR");

    if (port_env && *port_env) {
        int value = atoi(port_env);

        if (value > 0 && value <= 65535)
            port = value;
    }

    if (data_dir && *data_dir) {

#ifdef _WIN32
        snprintf(data_file, sizeof(data_file),
                 "%s\\students.csv", data_dir);

        snprintf(attendance_file, sizeof(attendance_file),
                 "%s\\attendance.csv", data_dir);

        CreateDirectoryA(data_dir, NULL);

#else
        snprintf(data_file, sizeof(data_file),
                 "%s/students.csv", data_dir);

        snprintf(attendance_file, sizeof(attendance_file),
                 "%s/attendance.csv", data_dir);

        mkdir(data_dir, 0755);
#endif

    }
    else {

#ifdef _WIN32
        CreateDirectoryA("data", NULL);
#else
        mkdir("data", 0755);
#endif
    }

#ifdef _WIN32
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock startup failed.\n");
        return 1;
    }
#endif

    load_data();
    load_attendance();

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);

    if (server == INVALID_SOCKET) {
        printf("Socket creation failed.\n");

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    int opt = 1;

    setsockopt(
        server, SOL_SOCKET, SO_REUSEADDR,
        (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(server, (struct sockaddr *)&addr,
             sizeof(addr)) == SOCKET_ERROR_CODE ||
        listen(server, 10) == SOCKET_ERROR_CODE) {

        printf("Could not start server on port %d.\n", port);

        CLOSE_SOCKET(server);

#ifdef _WIN32
        WSACleanup();
#endif

        return 1;
    }

    printf("\n");
    printf("============================================\n");
    printf(" CAMPUSFLOW STUDENT MANAGEMENT SYSTEM\n");
    printf("============================================\n");
    printf("Server listening on port %d\n", port);
    printf("Students loaded: %d\n", student_count);
    printf("Attendance records loaded: %d\n", attendance_count);
    printf("Hash Table: Active\n");
    printf("Binary Search Tree: Active\n");
    printf("Max Heap: Active\n");
    printf("Attendance History: Active\n");
    printf("Press Ctrl+C to stop.\n\n");

    for (;;) {
        socket_t client = accept(server, NULL, NULL);

        if (client != INVALID_SOCKET)
            handle_client(client);
    }

    CLOSE_SOCKET(server);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
