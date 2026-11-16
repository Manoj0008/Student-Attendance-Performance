
FROM gcc:14-bookworm AS build

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

COPY backend/main.c ./main.c

RUN gcc -O2 -Wall -Wextra -std=c11 main.c -o student_system -lcurl

FROM debian:bookworm-slim

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /app/student_system ./student_system
COPY frontend ./frontend

RUN mkdir -p /var/data

ENV DATA_DIR=/var/data

EXPOSE 8080

CMD ["./student_system"]
