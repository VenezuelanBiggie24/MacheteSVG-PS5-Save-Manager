// MacheteSVG - Embedded HTTP Web Server for PS5 Save Management
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/game_scanner.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <pthread.h>
#include <string>
#include <vector>

namespace machete::server
{

inline constexpr std::uint16_t kDefaultPort = 8383;

class WebServer
{
public:
    static constexpr std::uint16_t kDefaultPort = machete::server::kDefaultPort;

    static WebServer &instance();

    // Starts the embedded web server on the specified port (default: 8383)
    bool start(std::uint16_t port = kDefaultPort);

    // Stops the web server and terminates the listener thread
    void stop();

    // Returns whether the web server is actively listening
    bool is_running() const { return running_.load(); }

    // Returns the port the server is listening on
    std::uint16_t port() const { return port_; }

    // Returns the LAN IP address of the PS5 console (e.g. "192.168.1.50")
    std::string local_ip() const;

    // Returns full formatted URL (e.g. "http://192.168.1.50:8383")
    std::string url() const;

    // Updates the game library list served by the Web API
    void update_games(const std::vector<scanner::ScannedGame> &games);

    // Retrieves current scanned games (thread-safe copy)
    std::vector<scanner::ScannedGame> get_games() const;

    // Finds a scanned game by title ID (thread-safe copy)
    bool find_game(const std::string &title_id, scanner::ScannedGame &out) const;

private:
    WebServer();
    ~WebServer();

    WebServer(const WebServer &) = delete;
    WebServer &operator=(const WebServer &) = delete;

    void server_thread_main();
    void handle_client(int client_fd);

    void send_response(int client_fd, int status_code, const std::string &status_text,
                       const std::string &content_type, const void *body_data, std::size_t body_len,
                       const std::string &extra_headers = "");

    void send_string_response(int client_fd, int status_code, const std::string &status_text,
                              const std::string &content_type, const std::string &body_str,
                              const std::string &extra_headers = "");

    void send_file_response(int client_fd, const std::string &file_path, const std::string &filename,
                            const std::string &content_type = "application/octet-stream");

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::uint16_t port_{kDefaultPort};
    int server_sock_{-1};
    pthread_t thread_{0};

    mutable std::mutex games_mutex_;
    std::vector<scanner::ScannedGame> games_;
    mutable std::mutex ip_mutex_;
    mutable std::string cached_ip_;
};

} // namespace machete::server
