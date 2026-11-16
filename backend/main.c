
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

    /* Download the persistent CSV files before loading application data. */
    if (getenv("GITHUB_TOKEN") && *getenv("GITHUB_TOKEN")) {
        if (!github_download_file("students.csv", data_file) ||
            !github_download_file("attendance.csv", attendance_file)) {
            printf("Could not download CampusFlow data from GitHub. Check token permissions and repository files.\n");
            return 1;
        }
        printf("CampusFlow data downloaded from GitHub.\n");
    }

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
