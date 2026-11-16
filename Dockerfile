FROM gcc:14-bookworm AS build
WORKDIR /app
COPY backend/main.c ./main.c
RUN gcc -O2 -Wall -Wextra -std=c11 main.c -o student_system

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /app/student_system ./student_system
COPY frontend ./frontend
RUN mkdir -p /var/data
ENV DATA_DIR=/var/data
EXPOSE 8080
CMD ["./student_system"]
