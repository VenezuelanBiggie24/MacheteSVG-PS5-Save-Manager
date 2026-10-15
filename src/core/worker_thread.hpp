// MacheteSVG - Robust Worker Thread with Guaranteed 2MB Stack Size
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <pthread.h>
#include <utility>

namespace machete::threading
{

class WorkerThread
{
public:
    WorkerThread() noexcept = default;

    template <typename F, typename... Args>
    explicit WorkerThread(F &&f, Args &&...args)
    {
        start(std::forward<F>(f), std::forward<Args>(args)...);
    }

    ~WorkerThread()
    {
        join();
    }

    WorkerThread(const WorkerThread &) = delete;
    WorkerThread &operator=(const WorkerThread &) = delete;

    WorkerThread(WorkerThread &&other) noexcept
        : thread_(other.thread_), joinable_(other.joinable_)
    {
        other.thread_ = 0;
        other.joinable_ = false;
    }

    WorkerThread &operator=(WorkerThread &&other) noexcept
    {
        if (this != &other)
        {
            join();
            thread_ = other.thread_;
            joinable_ = other.joinable_;
            other.thread_ = 0;
            other.joinable_ = false;
        }
        return *this;
    }

    template <typename F, typename... Args>
    void start(F &&f, Args &&...args)
    {
        join();

        auto task = [func = std::forward<F>(f),
                     ... captured_args = std::forward<Args>(args)]() mutable {
            func(captured_args...);
        };

        using TaskType = decltype(task);
        auto *task_ptr = new TaskType(std::move(task));

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        // Guaranteed 2 MB stack to prevent thread stack exhaustion on Prospero OS
        constexpr std::size_t kWorkerStackSize = 2 * 1024 * 1024;
        pthread_attr_setstacksize(&attr, kWorkerStackSize);

        int rc = pthread_create(
            &thread_, &attr,
            [](void *arg) -> void * {
                auto *t = static_cast<TaskType *>(arg);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
                try
                {
                    (*t)();
                }
                catch (...)
                {
                }
#else
                (*t)();
#endif
                delete t;
                return nullptr;
            },
            task_ptr);

        pthread_attr_destroy(&attr);

        if (rc == 0)
        {
            joinable_ = true;
        }
        else
        {
            delete task_ptr;
            joinable_ = false;
        }
    }

    bool joinable() const noexcept
    {
        return joinable_;
    }

    void join()
    {
        if (joinable_)
        {
            pthread_join(thread_, nullptr);
            joinable_ = false;
            thread_ = 0;
        }
    }

private:
    pthread_t thread_{0};
    bool joinable_{false};
};

} // namespace machete::threading
