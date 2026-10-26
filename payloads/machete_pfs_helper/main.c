// MacheteSVG - Standalone PFS Mount Worker Daemon for PS5
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define HELPER_PORT 9033
#define LOG_PATH "/data/machete_pfs_helper.log"
#define READY_PATH "/download0/machete_pfs_ready"
#define READY_DATA_PATH "/data/machete_pfs_ready"
#define CMD_PATH "/download0/machete_pfs_cmd"
#define CMD_DATA_PATH "/data/machete_pfs_cmd"
#define RES_PATH "/download0/machete_pfs_res"
#define RES_DATA_PATH "/data/machete_pfs_res"

struct SaveMountOpt {
    uint8_t reserved[8];
    const char *budgetid;
    uint8_t pad[48];
};

extern int sceFsInitMountSaveDataOpt(struct SaveMountOpt *opt);
extern int sceFsMountSaveData(struct SaveMountOpt *opt, const char *image, const char *mountpoint, const void *key);
extern int sceFsInitUmountSaveDataOpt(struct SaveMountOpt *opt);
extern int sceFsUmountSaveData(struct SaveMountOpt *opt, const char *mountpoint, void *key, int flags);

static FILE *s_log_file = NULL;

static void log_msg(const char *fmt, ...) {
    if (!s_log_file) {
        s_log_file = fopen(LOG_PATH, "a");
    }
    if (s_log_file) {
        time_t now = time(NULL);
        struct tm tm_buf;
        localtime_r(&now, &tm_buf);
        char tstr[32];
        strftime(tstr, sizeof(tstr), "%Y-%m-%d %H:%M:%S", &tm_buf);
        fprintf(s_log_file, "[%s] [PFS_HELPER] ", tstr);

        va_list args;
        va_start(args, fmt);
        vfprintf(s_log_file, fmt, args);
        va_end(args);
        fprintf(s_log_file, "\n");
        fflush(s_log_file);
    }
}

static uint8_t hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

static bool parse_hex_key(const char *hex, uint8_t out_key[32]) {
    if (!hex || strlen(hex) < 64) return false;
    for (int i = 0; i < 32; ++i) {
        out_key[i] = (hex_char_to_val(hex[i * 2]) << 4) | hex_char_to_val(hex[i * 2 + 1]);
    }
    return true;
}

static int do_mount(const char *image_path, const char *mountpoint, const uint8_t *key) {
    if (!image_path || !mountpoint) return -1;
    mkdir(mountpoint, 0777);

    struct SaveMountOpt opt;
    memset(&opt, 0, sizeof(opt));
    sceFsInitMountSaveDataOpt(&opt);
    opt.budgetid = "system";

    log_msg("Invoking sceFsMountSaveData(image='%s', mount='%s', key=%s)",
            image_path, mountpoint, key ? "provided" : "null");

    int rc = sceFsMountSaveData(&opt, image_path, mountpoint, key);
    log_msg("sceFsMountSaveData returned rc=%d (0x%08x)", rc, (unsigned int)rc);
    return rc;
}

static int do_umount(const char *mountpoint) {
    if (!mountpoint) return -1;

    struct SaveMountOpt opt;
    memset(&opt, 0, sizeof(opt));
    sceFsInitUmountSaveDataOpt(&opt);

    log_msg("Invoking sceFsUmountSaveData(mount='%s')", mountpoint);
    int rc = sceFsUmountSaveData(&opt, mountpoint, NULL, 0);
    log_msg("sceFsUmountSaveData returned rc=%d (0x%08x)", rc, (unsigned int)rc);
    return rc;
}

static void publish_ready(int port) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"status\": \"ready\", \"pid\": %d, \"port\": %d}\n",
             (int)getpid(), port);

    FILE *f1 = fopen(READY_PATH, "w");
    if (f1) { fputs(buf, f1); fclose(f1); chmod(READY_PATH, 0666); }

    FILE *f2 = fopen(READY_DATA_PATH, "w");
    if (f2) { fputs(buf, f2); fclose(f2); chmod(READY_DATA_PATH, 0666); }

    log_msg("Published ready marker: PID %d on port %d", (int)getpid(), port);
}

static void handle_command_str(const char *cmd_line, char *out_resp, size_t out_max) {
    if (!cmd_line || !out_resp) return;

    if (strncmp(cmd_line, "PING", 4) == 0) {
        snprintf(out_resp, out_max, "PONG\n");
        return;
    }

    if (strncmp(cmd_line, "MOUNT ", 6) == 0) {
        char image[512] = {0};
        char mountpoint[512] = {0};
        char hex_key[128] = {0};
        int parts = sscanf(cmd_line + 6, "%511s %511s %127s", image, mountpoint, hex_key);

        uint8_t key_buf[32];
        const uint8_t *key_ptr = NULL;
        if (parts >= 3 && strlen(hex_key) >= 64) {
            if (parse_hex_key(hex_key, key_buf)) {
                key_ptr = key_buf;
            }
        }

        int rc = do_mount(image, mountpoint, key_ptr);
        snprintf(out_resp, out_max, "{\"status\": \"%s\", \"rc\": %d}\n",
                 (rc >= 0) ? "mounted" : "error", rc);
        return;
    }

    if (strncmp(cmd_line, "UMOUNT ", 7) == 0) {
        char mountpoint[512] = {0};
        (void)sscanf(cmd_line + 7, "%511s", mountpoint);
        int rc = do_umount(mountpoint);
        snprintf(out_resp, out_max, "{\"status\": \"%s\", \"rc\": %d}\n",
                 (rc == 0) ? "unmounted" : "error", rc);
        return;
    }

    if (strncmp(cmd_line, "STATUS", 6) == 0) {
        snprintf(out_resp, out_max, "{\"status\": \"running\", \"pid\": %d}\n", (int)getpid());
        return;
    }

    snprintf(out_resp, out_max, "{\"status\": \"unknown_command\"}\n");
}

static void process_file_ipc(void) {
    const char *cmd_files[] = { CMD_PATH, CMD_DATA_PATH };
    const char *res_files[] = { RES_PATH, RES_DATA_PATH };

    for (int i = 0; i < 2; ++i) {
        if (access(cmd_files[i], R_OK) == 0) {
            FILE *fc = fopen(cmd_files[i], "r");
            if (fc) {
                char line[1024] = {0};
                if (fgets(line, sizeof(line), fc)) {
                    fclose(fc);
                    unlink(cmd_files[i]);

                    // Trim newline
                    size_t len = strlen(line);
                    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
                        line[len - 1] = '\0';
                        len--;
                    }

                    log_msg("Processing file IPC command: %s", line);
                    char resp[512];
                    handle_command_str(line, resp, sizeof(resp));

                    FILE *fr = fopen(res_files[i], "w");
                    if (fr) {
                        fputs(resp, fr);
                        fclose(fr);
                        chmod(res_files[i], 0666);
                    }
                    log_msg("File IPC result written: %s", resp);
                } else {
                    fclose(fc);
                    unlink(cmd_files[i]);
                }
            }
        }
    }
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    log_msg("=================================================");
    log_msg("Machete PFS Helper Daemon starting (PID %d)", (int)getpid());
    log_msg("=================================================");

    // Socket server setup
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd >= 0) {
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(HELPER_PORT);

        if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0 &&
            listen(server_fd, 5) == 0) {
            // Set non-blocking
            int flags = fcntl(server_fd, F_GETFL, 0);
            fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
            log_msg("Listening on 127.0.0.1:%d", HELPER_PORT);
        } else {
            log_msg("Warning: failed to bind socket on port %d: %s", HELPER_PORT, strerror(errno));
            close(server_fd);
            server_fd = -1;
        }
    }

    publish_ready(HELPER_PORT);

    bool running = true;
    while (running) {
        // 1. Check file IPC
        process_file_ipc();

        // 2. Check socket connections
        if (server_fd >= 0) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd >= 0) {
                char req_buf[1024] = {0};
                ssize_t bytes = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
                if (bytes > 0) {
                    req_buf[bytes] = '\0';
                    size_t len = strlen(req_buf);
                    while (len > 0 && (req_buf[len - 1] == '\r' || req_buf[len - 1] == '\n')) {
                        req_buf[len - 1] = '\0';
                        len--;
                    }

                    if (strncmp(req_buf, "QUIT", 4) == 0) {
                        running = false;
                        send(client_fd, "BYE\n", 4, 0);
                    } else {
                        char resp[512];
                        handle_command_str(req_buf, resp, sizeof(resp));
                        send(client_fd, resp, strlen(resp), 0);
                    }
                }
                close(client_fd);
            }
        }

        usleep(15000); // 15ms sleep
    }

    log_msg("Machete PFS Helper Daemon stopping");
    if (server_fd >= 0) close(server_fd);
    unlink(READY_PATH);
    unlink(READY_DATA_PATH);
    return 0;
}
