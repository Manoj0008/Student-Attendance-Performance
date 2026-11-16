#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

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
#define HASH_SIZE 101
#define BUF_SIZE 65536
#define DEFAULT_DATA_FILE "data/students.csv"

static char data_file[512] = DEFAULT_DATA_FILE;

typedef struct Student {
    int id;
    char name[80], department[60];
    int year, marks, present, total;
    struct Student *next;
} Student;

typedef struct BSTNode {
    int marks, id;
    struct BSTNode *left, *right;
} BSTNode;

typedef struct {
    Student *buckets[HASH_SIZE];
} HashTable;

static HashTable table;
static Student students[MAX_STUDENTS];
static int student_count = 0;

/* HASH TABLE */

static int hash_id(int id) {
    return (id < 0 ? -id : id) % HASH_SIZE;
}

static Student *find_student(int id) {
    Student *p = table.buckets[hash_id(id)];

    while (p) {
        if (p->id == id) return p;
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

/* FILE STORAGE */

static void save_data(void) {
    FILE *f = fopen(data_file, "w");

    if (!f) return;

    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i];

        fprintf(f, "%d|%s|%s|%d|%d|%d|%d\n",
                s->id, s->name, s->department, s->year,
                s->marks, s->present, s->total);
    }

    fclose(f);
}

static void load_data(void) {
    FILE *f = fopen(data_file, "r");

    if (!f) return;

    char line[512];

    while (fgets(line, sizeof(line), f) &&
           student_count < MAX_STUDENTS) {

        Student s = {0};
        char *parts[7], *p = line;
        int n = 0;

        while (n < 7 &&
               (parts[n] = strtok(n == 0 ? p : NULL, "|")) != NULL)
            n++;

        if (n == 7) {
            s.id = atoi(parts[0]);

            snprintf(s.name, sizeof(s.name), "%s", parts[1]);
            snprintf(s.department, sizeof(s.department), "%s", parts[2]);

            s.year = atoi(parts[3]);
            s.marks = atoi(parts[4]);
            s.present = atoi(parts[5]);
            s.total = atoi(parts[6]);

            s.name[strcspn(s.name, "\r\n")] = 0;
            s.department[strcspn(s.department, "\r\n")] = 0;

            if (s.id > 0 && !find_student(s.id))
                students[student_count++] = s;
        }
    }

    fclose(f);
    rebuild_hash();
}

/* BINARY SEARCH TREE */

static BSTNode *bst_insert(BSTNode *root, int marks, int id) {
    if (!root) {
        BSTNode *n = (BSTNode *)calloc(1, sizeof(BSTNode));

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

static void bst_free(BSTNode *r) {
    if (!r) return;

    bst_free(r->left);
    bst_free(r->right);
    free(r);
}

static void bst_collect_desc(
    BSTNode *r, int *ids, int *marks, int *count) {

    if (!r || *count >= MAX_STUDENTS) return;

    bst_collect_desc(r->right, ids, marks, count);

    ids[*count] = r->id;
    marks[*count] = r->marks;
    (*count)++;

    bst_collect_desc(r->left, ids, marks, count);
}

/* MAX HEAP */

static void heap_swap(Student **a, Student **b) {
    Student *t = *a;
    *a = *b;
    *b = t;
}

static void heap_push(Student **heap, int *size, Student *s) {
    int i = (*size)++;

    heap[i] = s;

    while (i > 0) {
        int p = (i - 1) / 2;

        if (heap[p]->marks >= heap[i]->marks)
            break;

        heap_swap(&heap[p], &heap[i]);
        i = p;
    }
}

static Student *heap_pop(Student **heap, int *size) {
    if (*size <= 0) return NULL;

    Student *top = heap[0];

    heap[0] = heap[--(*size)];

    int i = 0;

    for (;;) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        int largest = i;

        if (l < *size &&
            heap[l]->marks > heap[largest]->marks)
            largest = l;

        if (r < *size &&
            heap[r]->marks > heap[largest]->marks)
            largest = r;

        if (largest == i) break;

        heap_swap(&heap[i], &heap[largest]);
        i = largest;
    }

    return top;
}

/* JSON HELPERS */

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

    if (*p != '"') return;

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

/* HTTP RESPONSE */

static void send_response(
    socket_t c, int status,
    const char *type, const char *body) {

    char header[512];

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

/* STUDENT LIST API */

static void send_students(socket_t c) {
    char out[BUF_SIZE];
    size_t used = 0;

    used += (size_t)snprintf(
        out + used, sizeof(out) - used, "[");

    for (int i = 0; i < student_count; i++) {
        Student *s = &students[i];

        char name[180], dept[140];

        json_escape(s->name, name, sizeof(name));
        json_escape(s->department, dept, sizeof(dept));

        used += (size_t)snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\","
            "\"department\":\"%s\",\"year\":%d,"
            "\"marks\":%d,\"present\":%d,\"total\":%d,"
            "\"attendance\":%.1f}",
            i ? "," : "",
            s->id, name, dept, s->year,
            s->marks, s->present, s->total,
            s->total ? 100.0 * s->present / s->total : 0.0);

        if (used >= sizeof(out) - 300) break;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    send_response(c, 200, "application/json", out);
}

/* BST SORTED API */

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

        if (!s) continue;

        char name[180];

        json_escape(s->name, name, sizeof(name));

        used += (size_t)snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\",\"marks\":%d}",
            i ? "," : "", s->id, name, marks[i]);

        if (used >= sizeof(out) - 250) break;
    }

    snprintf(out + used, sizeof(out) - used, "]");

    bst_free(root);

    send_response(c, 200, "application/json", out);
}

/* TOP FIVE STUDENTS USING MAX HEAP */

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

        used += (size_t)snprintf(
            out + used, sizeof(out) - used,
            "%s{\"id\":%d,\"name\":\"%s\",\"marks\":%d}",
            i ? "," : "", s->id, name, s->marks);
    }

    snprintf(out + used, sizeof(out) - used, "]");

    send_response(c, 200, "application/json", out);
}

/* HTTP CONTENT LENGTH PARSER */

static int get_content_length(
    const char *request, int *content_length) {

    const char *headers_end = strstr(request, "\r\n\r\n");

    if (!headers_end) return 0;

    const char *line = request;

    while (line < headers_end) {
        const char *line_end = strstr(line, "\r\n");

        if (!line_end || line_end > headers_end)
            break;

        const char *colon = memchr(
            line, ':', (size_t)(line_end - line));

        if (colon) {
            size_t name_len = (size_t)(colon - line);

            if (name_len == 14) {
                const char expected[] = "content-length";
                int matches = 1;

                for (size_t i = 0; i < name_len; i++) {
                    if ((char)tolower((unsigned char)line[i])
                        != expected[i]) {
                        matches = 0;
                        break;
                    }
                }

                if (matches) {
                    const char *value = colon + 1;

                    while (value < line_end &&
                           isspace((unsigned char)*value))
                        value++;

                    char number[32];

                    size_t value_len =
                        (size_t)(line_end - value);

                    if (value_len == 0 ||
                        value_len >= sizeof(number))
                        return 0;

                    memcpy(number, value, value_len);
                    number[value_len] = '\0';

                    char *endptr = NULL;

                    long parsed = strtol(number, &endptr, 10);

                    while (endptr && *endptr &&
                           isspace((unsigned char)*endptr))
                        endptr++;

                    if (!endptr ||
                        endptr == number ||
                        *endptr != '\0' ||
                        parsed < 0 ||
                        parsed > BUF_SIZE)
                        return 0;

                    *content_length = (int)parsed;

                    return 1;
                }
            }
        }

        line = line_end + 2;
    }

    *content_length = 0;

    return 1;
}

/* HTTP CLIENT HANDLER */

static void handle_client(socket_t c) {
    char req[BUF_SIZE] = {0};

    int total = 0;
    int header_bytes = 0;
    int content_length = 0;
    int expected_bytes = 0;

    /* Receive complete HTTP request */

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

        if (header_bytes == 0) {
            char *separator = strstr(req, "\r\n\r\n");

            if (separator) {
                header_bytes = (int)(separator - req) + 4;

                if (!get_content_length(req, &content_length) ||
                    content_length >
                    BUF_SIZE - header_bytes - 1) {

                    send_response(
                        c, 400, "application/json",
                        "{\"error\":\"Invalid or oversized Content-Length.\"}");

                    CLOSE_SOCKET(c);
                    return;
                }

                expected_bytes = header_bytes + content_length;
            }
        }

        if (header_bytes > 0 && total >= expected_bytes)
            break;
    }

    if (header_bytes == 0 || total < expected_bytes) {
        send_response(
            c, 400, "application/json",
            "{\"error\":\"Incomplete HTTP request.\"}");

        CLOSE_SOCKET(c);
        return;
    }

    char method[12] = {0};
    char path[256] = {0};

    sscanf(req, "%11s %255s", method, path);

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

    /* ATTENDANCE */

    else if (strcmp(method, "POST") == 0 &&
             strcmp(path, "/api/attendance") == 0) {

        int id = get_int(body, "id", 0);

        char status[20];

        get_string(body, "status", status, sizeof(status));

        Student *s = find_student(id);

        if (!s) {
            send_response(
                c, 404, "application/json",
                "{\"error\":\"Student not found.\"}");
        }

        else if (strcmp(status, "Present") &&
                 strcmp(status, "Absent")) {

            send_response(
                c, 400, "application/json",
                "{\"error\":\"Status must be Present or Absent.\"}");
        }

        else {
            s->total++;

            if (!strcmp(status, "Present"))
                s->present++;

            save_data();

            send_response(
                c, 200, "application/json",
                "{\"message\":\"Attendance saved.\"}");
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
                "{\"message\":\"Marks updated.\"}");
        }
    }

    /* DELETE STUDENT */

    else if (strcmp(method, "DELETE") == 0 &&
             strncmp(path, "/api/students?id=", 17) == 0) {

        int id = atoi(path + 17);
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

            rebuild_hash();
            save_data();

            send_response(
                c, 200, "application/json",
                "{\"message\":\"Student deleted.\"}");
        }
    }

    /* FRONTEND HTML */

    else if (strcmp(method, "GET") == 0 &&
             (strcmp(path, "/") == 0 ||
              strcmp(path, "/index.html") == 0)) {

        FILE *f = fopen("../frontend/index.html", "rb");

        if (!f)
            f = fopen("frontend/index.html", "rb");

        if (!f) {
            send_response(
                c, 404, "text/plain",
                "Frontend file not found. Run from project root.");
        }

        else {
            char page[20000];

            size_t n = fread(page, 1, sizeof(page) - 1, f);

            page[n] = 0;

            fclose(f);

            send_response(
                c, 200, "text/html; charset=utf-8", page);
        }
    }

    /* FRONTEND CSS */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/style.css") == 0) {

        FILE *f = fopen("frontend/style.css", "rb");
        char page[20000];

        if (!f) {
            send_response(c, 404, "text/plain", "CSS not found.");
        }

        else {
            size_t n = fread(page, 1, sizeof(page) - 1, f);

            page[n] = 0;

            fclose(f);

            send_response(c, 200, "text/css", page);
        }
    }

    /* FRONTEND JAVASCRIPT */

    else if (strcmp(method, "GET") == 0 &&
             strcmp(path, "/script.js") == 0) {

        FILE *f = fopen("frontend/script.js", "rb");
        char page[20000];

        if (!f) {
            send_response(
                c, 404, "text/plain",
                "JavaScript not found.");
        }

        else {
            size_t n = fread(page, 1, sizeof(page) - 1, f);

            page[n] = 0;

            fclose(f);

            send_response(
                c, 200, "application/javascript", page);
        }
    }

    else {
        send_response(
            c, 404, "application/json",
            "{\"error\":\"Route not found.\"}");
    }

    CLOSE_SOCKET(c);
}

/* MAIN SERVER */

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

        CreateDirectoryA(data_dir, NULL);
#else
        snprintf(data_file, sizeof(data_file),
                 "%s/students.csv", data_dir);

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

    printf("\nStudent Attendance & Performance Management System\n");
    printf("Server listening on port %d\n", port);
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
