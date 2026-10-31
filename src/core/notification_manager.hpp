// MacheteSVG - Native PS5 Notifications Architecture
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/logger.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__) || defined(__FreeBSD__)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace machete::notification
{

#pragma pack(push, 1)
struct SceNotificationRequest
{
    int type{0};
    int req_id{-1};
    int priority{0};
    int msg_id{0};
    int target_id{-1};
    int user_id{-1};
    int unk1{0};
    int unk2{0};
    int app_id{0};
    int error_num{0};
    int unk3{0};
    unsigned char use_icon_image_uri{0};
    char message[1024]{};
    char icon_uri[1024]{};
    char unk[1024]{};
};
#pragma pack(pop)

using SceKernelSendNotificationRequestFn = int (*)(int device, const void *req, std::size_t size, int blocking);
using SceNotificationSendFn = int (*)(int user_id, bool is_logged, const char *payload);

inline SceKernelSendNotificationRequestFn resolve_send_notification_request()
{
    static SceKernelSendNotificationRequestFn s_fn = nullptr;
    static bool s_probed = false;
    if (s_probed)
        return s_fn;
    s_probed = true;

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__) || defined(__FreeBSD__)
#if defined(RTLD_DEFAULT)
    void *sym = dlsym(RTLD_DEFAULT, "sceKernelSendNotificationRequest");
    if (sym)
    {
        s_fn = reinterpret_cast<SceKernelSendNotificationRequestFn>(sym);
        return s_fn;
    }
#endif
    static const char *kLibs[] = {
        "libkernel.sprx",
        "libkernel_sys.sprx",
        "libkernel_web.sprx",
        nullptr
    };
    for (int i = 0; kLibs[i]; ++i)
    {
        void *h = dlopen(kLibs[i], RTLD_LAZY | RTLD_GLOBAL);
        if (h)
        {
            sym = dlsym(h, "sceKernelSendNotificationRequest");
            if (sym)
            {
                s_fn = reinterpret_cast<SceKernelSendNotificationRequestFn>(sym);
                return s_fn;
            }
            dlclose(h);
        }
    }
#endif
    return nullptr;
}

inline SceNotificationSendFn resolve_sce_notification_send()
{
    static SceNotificationSendFn s_fn = nullptr;
    static bool s_probed = false;
    if (s_probed)
        return s_fn;
    s_probed = true;

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__) || defined(__FreeBSD__)
#if defined(RTLD_DEFAULT)
    void *sym = dlsym(RTLD_DEFAULT, "sceNotificationSend");
    if (sym)
    {
        s_fn = reinterpret_cast<SceNotificationSendFn>(sym);
        return s_fn;
    }
#endif
    static const char *kLibs[] = {
        "libSceNotification.sprx",
        "/system/common/lib/libSceNotification.sprx",
        nullptr
    };
    for (int i = 0; kLibs[i]; ++i)
    {
        void *h = dlopen(kLibs[i], RTLD_LAZY | RTLD_GLOBAL);
        if (h)
        {
            sym = dlsym(h, "sceNotificationSend");
            if (sym)
            {
                s_fn = reinterpret_cast<SceNotificationSendFn>(sym);
                return s_fn;
            }
            dlclose(h);
        }
    }
#endif
    return nullptr;
}

inline std::string format_notification_text(const std::string &message, const std::string &title = "MacheteSVG")
{
    if (title.empty() || title == message)
        return message;
    return title + ": " + message;
}

inline std::string build_rich_notification_payload(const std::string &message,
                                                   const std::string &title = "MacheteSVG",
                                                   const std::string &icon_path = "/user/data/MacheteSVG/notification.png")
{
    std::string escaped_msg;
    escaped_msg.reserve(message.size() * 2);
    for (unsigned char c : message)
    {
        if (c == '"')
            escaped_msg += "\\\"";
        else if (c == '\\')
            escaped_msg += "\\\\";
        else if (c == '\n')
            escaped_msg += "\\n";
        else if (c == '\r')
            escaped_msg += "\\r";
        else if (c == '\t')
            escaped_msg += "\\t";
        else if (c < 0x20)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            escaped_msg += buf;
        }
        else
        {
            escaped_msg += static_cast<char>(c);
        }
    }

    std::string escaped_title;
    escaped_title.reserve(title.size() * 2);
    for (unsigned char c : title)
    {
        if (c == '"')
            escaped_title += "\\\"";
        else if (c == '\\')
            escaped_title += "\\\\";
        else if (c == '\n')
            escaped_title += "\\n";
        else if (c == '\r')
            escaped_title += "\\r";
        else if (c == '\t')
            escaped_title += "\\t";
        else if (c < 0x20)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            escaped_title += buf;
        }
        else
        {
            escaped_title += static_cast<char>(c);
        }
    }

    std::string escaped_icon;
    escaped_icon.reserve(icon_path.size() * 2);
    for (unsigned char c : icon_path)
    {
        if (c == '"')
            escaped_icon += "\\\"";
        else if (c == '\\')
            escaped_icon += "\\\\";
        else
            escaped_icon += static_cast<char>(c);
    }

    char ts_buf[64] = "2026-10-31T02:00:00.000Z";
    std::time_t now = std::time(nullptr);
    {
        static std::mutex s_gmtime_mutex;
        std::lock_guard<std::mutex> lock(s_gmtime_mutex);
        const std::tm *p_tm = std::gmtime(&now);
        if (p_tm)
        {
            std::strftime(ts_buf, sizeof(ts_buf), "%Y-%m-%dT%H:%M:%S.000Z", p_tm);
        }
    }

    static std::atomic<unsigned int> s_serial{0};
    std::string notif_id = "machete-" + std::to_string(now) + "-" + std::to_string(++s_serial);

    std::string json;
    json.reserve(2048);
    json += "{\"rawData\":{";
    json += "\"viewTemplateType\":\"ToastTemplateB\",";
    json += "\"channelType\":\"SystemFeedback\",";
    json += "\"bundleName\":\"" + escaped_title + "\",";
    json += "\"useCaseId\":\"IDC\",";
    json += "\"soundEffect\":\"none\",";
    json += "\"toastOverwriteType\":\"No\",";
    json += "\"isImmediate\":true,";
    json += "\"priority\":100,";
    json += "\"viewData\":{";
    if (!escaped_icon.empty())
    {
        json += "\"icon\":{\"type\":\"Url\",\"parameters\":{\"url\":\"" + escaped_icon + "\"}},";
    }
    json += "\"message\":{\"body\":\"" + escaped_msg + "\"},";
    json += "\"subMessage\":{\"body\":\"" + escaped_title + "\"}";
    json += "}},";
    json += "\"createdDateTime\":\"" + std::string(ts_buf) + "\",";
    json += "\"localNotificationId\":\"" + notif_id + "\"}";

    return json;
}

struct NotificationTelemetry
{
    std::string last_message;
    std::string last_title;
    std::string last_formatted;
    std::size_t total_count = 0;
};

inline NotificationTelemetry &get_telemetry()
{
    static NotificationTelemetry s_telem;
    return s_telem;
}

inline std::mutex &get_notification_mutex()
{
    static std::mutex s_mutex;
    return s_mutex;
}

inline void notify(const std::string &message, const std::string &title = "MacheteSVG")
{
    std::lock_guard<std::mutex> lock(get_notification_mutex());
    auto &telem = get_telemetry();
    telem.last_message = message;
    telem.last_title = title;
    telem.last_formatted = format_notification_text(message, title);
    telem.total_count++;

    // Safe fallback to application log
    MACHETE_LOG("[NOTIFY]", "[%s] %s", title.c_str(), message.c_str());

    // 1. Try modern rich PS5 shell notification
    auto sce_notif_send = resolve_sce_notification_send();
    if (sce_notif_send)
    {
        std::string payload = build_rich_notification_payload(message, title);
        int rc = sce_notif_send(0xfe, true, payload.c_str());
        if (rc == 0)
            return;
    }

    // 2. Try legacy libkernel notification request
    auto send_notif_req = resolve_send_notification_request();
    if (send_notif_req)
    {
        SceNotificationRequest req{};
        req.type = 0;
        req.target_id = -1;
        req.user_id = -1;
        std::string formatted = format_notification_text(message, title);
        std::size_t copy_len = (formatted.size() < sizeof(req.message) - 1) ? formatted.size() : (sizeof(req.message) - 1);
        std::memcpy(req.message, formatted.data(), copy_len);
        req.message[copy_len] = '\0';
        send_notif_req(0, &req, sizeof(req), 0);
    }
}

inline std::string get_last_notification_message()
{
    std::lock_guard<std::mutex> lock(get_notification_mutex());
    return get_telemetry().last_message;
}

inline std::string get_last_notification_title()
{
    std::lock_guard<std::mutex> lock(get_notification_mutex());
    return get_telemetry().last_title;
}

inline std::string get_last_notification_formatted()
{
    std::lock_guard<std::mutex> lock(get_notification_mutex());
    return get_telemetry().last_formatted;
}

inline std::size_t get_notification_count()
{
    std::lock_guard<std::mutex> lock(get_notification_mutex());
    return get_telemetry().total_count;
}

inline void clear_notification_history()
{
    std::lock_guard<std::mutex> lock(get_notification_mutex());
    auto &telem = get_telemetry();
    telem.last_message.clear();
    telem.last_title.clear();
    telem.last_formatted.clear();
    telem.total_count = 0;
}

} // namespace machete::notification

namespace machete
{
using notification::notify;
}
