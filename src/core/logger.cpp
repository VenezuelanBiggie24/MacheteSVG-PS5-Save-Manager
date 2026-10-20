// MacheteSVG - Event Logging, Crash Diagnostics & Breadcrumb Tracing Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/logger.hpp"
#include "core/build_info.hpp"

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <unistd.h>

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
extern "C"
{
    int sceKernelDebugOutText(int channel, const char *text) __attribute__((weak));
    int sceKernelUsleep(unsigned int microseconds) __attribute__((weak));
    int sceSystemServiceLoadExec(const char *path, const char **arguments) __attribute__((weak));
}
#else
extern "C"
{
    __attribute__((weak)) int sceKernelDebugOutText(int, const char *) { return 0; }
    __attribute__((weak)) int sceKernelUsleep(unsigned int) { return 0; }
    __attribute__((weak)) int sceSystemServiceLoadExec(const char *, const char **) { return 0; }
}
#endif

namespace machete::log
{

namespace
{

constexpr const char *kPrimaryLogPath   = "/data/homebrew/PPSA99001/machete.log";
constexpr const char *kSandboxLogPath   = "/download0/machete.log";
constexpr const char *kDataLogPath      = "/data/machete.log";
constexpr const char *kUserAppLogPath   = "/user/app/PPSA99001/machete.log";

// Descriptores de archivo directos POSIX (seguros contra señales asíncronas)
int s_fd_primary = -1;
int s_fd_sandbox = -1;
int s_fd_data    = -1;
int s_fd_userapp = -1;

std::recursive_mutex s_log_mutex;
char s_current_breadcrumb[256] = "initialization";

// Función de escritura asíncrona segura para manejador de señales
void raw_signal_safe_write(int fd, const char *str)
{
    if (fd < 0 || !str)
        return;
    const std::size_t len = std::strlen(str);
    const auto written = ::write(fd, str, len);
    (void)written;
    ::fsync(fd);
}

void try_open_fd(int &fd_ref, const char *path)
{
    if (fd_ref >= 0)
        return;
    fd_ref = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
}

// Manejador de señales fatales (SIGSEGV, SIGBUS, etc.)
void fatal_signal_handler(int sig, siginfo_t *info, void *ucontext)
{
    const char *sig_name = "UNKNOWN";
    switch (sig)
    {
    case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault / Invalid Memory Access)"; break;
    case SIGBUS:  sig_name = "SIGBUS (Bus Error / Invalid Alignment)"; break;
    case SIGABRT: sig_name = "SIGABRT (Abort / Assertion Failed)"; break;
    case SIGILL:  sig_name = "SIGILL (Illegal Instruction)"; break;
    case SIGFPE:  sig_name = "SIGFPE (Floating Point Exception / Divide by Zero)"; break;
    case SIGSYS:  sig_name = "SIGSYS (Unsupported System Call / Bad Syscall)"; break;
    case SIGTRAP: sig_name = "SIGTRAP (Debugger Trap / Breakpoint)"; break;
    }

    std::uint64_t rip = 0, rsp = 0, rbp = 0, trapno = 0, err = 0;
    std::uint64_t rax = 0, rbx = 0, rcx = 0, rdx = 0, rsi = 0, rdi = 0;
#if (defined(__FreeBSD__) || defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)) && (defined(__x86_64__) || defined(__amd64__))
#if defined(__FreeBSD__) || defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
    if (ucontext)
    {
        const auto *uc = reinterpret_cast<const ucontext_t *>(ucontext);
        rip = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rip);
        rsp = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rsp);
        rbp = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rbp);
        trapno = static_cast<std::uint64_t>(uc->uc_mcontext.mc_trapno);
        err = static_cast<std::uint64_t>(uc->uc_mcontext.mc_err);
        rax = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rax);
        rbx = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rbx);
        rcx = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rcx);
        rdx = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rdx);
        rsi = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rsi);
        rdi = static_cast<std::uint64_t>(uc->uc_mcontext.mc_rdi);
    }
#else
    (void)ucontext;
#endif
#else
    (void)ucontext;
#endif

    char crash_msg[1280];
    std::snprintf(crash_msg, sizeof(crash_msg),
                  "\n"
                  "====================================================\n"
                  "[FATAL CRASH DETECTED]\n"
                  "Application: MacheteSVG v%s (Build #%s, Commit %s)\n"
                  "Built: %s %s\n"
                  "Signal: %d - %s\n"
                  "Faulting Memory Address (si_addr): %p\n"
                  "Signal Code (si_code): %d\n"
                  "CPU Registers:\n"
                  "  RIP: 0x%016llx  RSP: 0x%016llx  RBP: 0x%016llx\n"
                  "  RAX: 0x%016llx  RBX: 0x%016llx  RCX: 0x%016llx\n"
                  "  RDX: 0x%016llx  RSI: 0x%016llx  RDI: 0x%016llx\n"
                  "  Trap: %llu  Err: %llu\n"
                  "Last Recorded Breadcrumb: [%s]\n"
                  "PID: %d | UID: %d | GID: %d\n"
                  "====================================================\n\n",
                  ::machete::build::kAppVersion,
                  ::machete::build::kBuildNumber,
                  ::machete::build::kGitCommit,
                  ::machete::build::kBuildDate,
                  ::machete::build::kBuildTime,
                  sig, sig_name, info ? info->si_addr : nullptr, info ? info->si_code : 0,
                  static_cast<unsigned long long>(rip),
                  static_cast<unsigned long long>(rsp),
                  static_cast<unsigned long long>(rbp),
                  static_cast<unsigned long long>(rax),
                  static_cast<unsigned long long>(rbx),
                  static_cast<unsigned long long>(rcx),
                  static_cast<unsigned long long>(rdx),
                  static_cast<unsigned long long>(rsi),
                  static_cast<unsigned long long>(rdi),
                  static_cast<unsigned long long>(trapno),
                  static_cast<unsigned long long>(err),
                  s_current_breadcrumb, static_cast<int>(getpid()),
                  static_cast<int>(getuid()), static_cast<int>(getgid()));

    // Write directly without mutex locks or malloc (Async-Signal-Safe)
    ::write(STDERR_FILENO, crash_msg, std::strlen(crash_msg));
    raw_signal_safe_write(s_fd_primary, crash_msg);
    raw_signal_safe_write(s_fd_sandbox, crash_msg);
    raw_signal_safe_write(s_fd_data, crash_msg);
    raw_signal_safe_write(s_fd_userapp, crash_msg);

    if (sceKernelDebugOutText)
        sceKernelDebugOutText(0, crash_msg);

    // Brief pause to allow disk buffers to settle on FreeBSD/Prospero before exit
    if (sceKernelUsleep)
        sceKernelUsleep(150000);

    if (sceSystemServiceLoadExec)
    {
        sceSystemServiceLoadExec("exit", nullptr);
        for (;;)
        {
            if (sceKernelUsleep)
                sceKernelUsleep(100000);
        }
    }

    _exit(128 + sig);
}

} // namespace

void init()
{
    // 0. Dedicated alternate signal stack to guarantee crash handling during stack overflow
    alignas(16) static char s_alt_signal_stack[65536];
    stack_t ss{};
    ss.ss_sp = s_alt_signal_stack;
    ss.ss_size = sizeof(s_alt_signal_stack);
    ss.ss_flags = 0;
    (void)sigaltstack(&ss, nullptr);

    // 1. Install signal handlers for forensic crash capture
    struct sigaction sa{};
    sa.sa_sigaction = fatal_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESETHAND | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGSYS,  &sa, nullptr);
    sigaction(SIGTRAP, &sa, nullptr);

    // 2. Open active log paths
    reopen_paths();

    // 3. Log session startup header with complete date and time
    char time_str[64];
    std::time_t now = std::time(nullptr);
    std::tm *tm_info = std::localtime(&now);
    if (tm_info)
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    else
        std::snprintf(time_str, sizeof(time_str), "unknown");

    write("[SYSTEM]", "====================================================");
    write("[SYSTEM]", "MacheteSVG v%s (Build #%s, Commit %s) - Event Log & System Diagnostics",
          ::machete::build::kAppVersion, ::machete::build::kBuildNumber, ::machete::build::kGitCommit);
    write("[SYSTEM]", "App Version: %s | Compilation Build: #%s (%s %s)",
          ::machete::build::kAppVersion, ::machete::build::kBuildNumber,
          ::machete::build::kBuildDate, ::machete::build::kBuildTime);
    write("[SYSTEM]", "Date & Time: %s", time_str);
    write("[SYSTEM]", "PID: %d | UID: %d | EUID: %d | GID: %d",
          static_cast<int>(getpid()), static_cast<int>(getuid()),
          static_cast<int>(geteuid()), static_cast<int>(getgid()));
    write("[SYSTEM]", "Log Descriptors: primary=%d, sandbox=%d, data=%d, userapp=%d",
          (s_fd_primary >= 0) ? 1 : 0, (s_fd_sandbox >= 0) ? 1 : 0,
          (s_fd_data >= 0) ? 1 : 0, (s_fd_userapp >= 0) ? 1 : 0);
    write("[SYSTEM]", "====================================================");
}

void set_breadcrumb(const char *step)
{
    if (!step)
        return;
    std::lock_guard<std::recursive_mutex> lock(s_log_mutex);
    std::strncpy(s_current_breadcrumb, step, sizeof(s_current_breadcrumb) - 1);
    s_current_breadcrumb[sizeof(s_current_breadcrumb) - 1] = '\0';
}

const char *get_breadcrumb()
{
    std::lock_guard<std::recursive_mutex> lock(s_log_mutex);
    return s_current_breadcrumb;
}

void reopen_paths()
{
    std::lock_guard<std::recursive_mutex> lock(s_log_mutex);
    try_open_fd(s_fd_sandbox, kSandboxLogPath);
    try_open_fd(s_fd_primary, kPrimaryLogPath);
    try_open_fd(s_fd_data,    kDataLogPath);
    try_open_fd(s_fd_userapp, kUserAppLogPath);
    write("[SYSTEM]", "Log paths refreshed after root jailbreak (App v%s, Build #%s, Commit %s)",
          ::machete::build::kAppVersion, ::machete::build::kBuildNumber, ::machete::build::kGitCommit);
}

void reset_fresh()
{
    std::lock_guard<std::recursive_mutex> lock(s_log_mutex);
    if (s_fd_primary >= 0)
    {
        ::close(s_fd_primary);
        s_fd_primary = -1;
    }
    if (s_fd_data >= 0)
    {
        ::close(s_fd_data);
        s_fd_data = -1;
    }
    if (s_fd_userapp >= 0)
    {
        ::close(s_fd_userapp);
        s_fd_userapp = -1;
    }

    // Truncar archivos en disco
    unlink(kPrimaryLogPath);
    unlink(kDataLogPath);
    unlink(kUserAppLogPath);

    try_open_fd(s_fd_primary, kPrimaryLogPath);
    try_open_fd(s_fd_data,    kDataLogPath);
    try_open_fd(s_fd_userapp, kUserAppLogPath);

    char time_str[64];
    std::time_t now = std::time(nullptr);
    std::tm *tm_info = std::localtime(&now);
    if (tm_info)
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    else
        std::snprintf(time_str, sizeof(time_str), "unknown");

    write("[SYSTEM]", "====================================================");
    write("[SYSTEM]", "MacheteSVG - Registro Limpio de Sesion (Post-Actualizacion OTA)");
    write("[SYSTEM]", "Version: %s | Build: #%s | Commit: %s",
          ::machete::build::kAppVersion, ::machete::build::kBuildNumber, ::machete::build::kGitCommit);
    write("[SYSTEM]", "Fecha y Hora: %s", time_str);
    write("[SYSTEM]", "Historial previo depurado. Sesion fresca iniciada correctamente.");
    write("[SYSTEM]", "====================================================");
}

void vwrite(const char *tag, const char *fmt, va_list args)
{
    char body[1024];
    vsnprintf(body, sizeof(body), fmt, args);

    char time_str[64];
    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
    bool tm_ok = false;
    {
        std::lock_guard<std::recursive_mutex> lock(s_log_mutex);
        std::tm *tm_info = std::localtime(&now);
        if (tm_info)
        {
            tm_buf = *tm_info;
            tm_ok = true;
        }
    }
    if (tm_ok)
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);
    else
        std::snprintf(time_str, sizeof(time_str), "1970-01-01 00:00:00");

    char line[1536];
    const int len = std::snprintf(line, sizeof(line), "[%s] %-12s %s\n",
                                  time_str, tag ? tag : "[INFO]", body);
    if (len <= 0)
        return;

    std::lock_guard<std::recursive_mutex> lock(s_log_mutex);

    // Standard output
    std::fputs(line, stdout);
    std::fflush(stdout);

    // Prospero kernel debug output
    if (sceKernelDebugOutText)
        sceKernelDebugOutText(0, line);

    // Check if message is error level to enforce disk sync
    const bool is_error = (tag != nullptr) &&
        (std::strstr(tag, "ERR") != nullptr || std::strstr(tag, "CRIT") != nullptr ||
         std::strstr(tag, "FATAL") != nullptr || std::strstr(tag, "FAIL") != nullptr);

    const auto write_log_fd = [is_error](int fd, const char *text, std::size_t text_len)
    {
        if (fd >= 0)
        {
            auto w = ::write(fd, text, text_len);
            (void)w;
            if (is_error)
            {
                ::fsync(fd);
            }
        }
    };

    write_log_fd(s_fd_primary, line, static_cast<std::size_t>(len));
    write_log_fd(s_fd_sandbox, line, static_cast<std::size_t>(len));
    write_log_fd(s_fd_data,    line, static_cast<std::size_t>(len));
    write_log_fd(s_fd_userapp, line, static_cast<std::size_t>(len));
}

void write(const char *tag, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vwrite(tag, fmt, args);
    va_end(args);
}

void flush()
{
    std::lock_guard<std::recursive_mutex> lock(s_log_mutex);
    std::fflush(stdout);
    std::fflush(stderr);
    if (s_fd_primary >= 0) ::fsync(s_fd_primary);
    if (s_fd_sandbox >= 0) ::fsync(s_fd_sandbox);
    if (s_fd_data >= 0)    ::fsync(s_fd_data);
    if (s_fd_userapp >= 0) ::fsync(s_fd_userapp);
}

} // namespace machete::log
