// MacheteSVG - Application Version & Build Information
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace machete::build
{

#ifndef MACHETE_APP_VERSION
#define MACHETE_APP_VERSION "1.3.0"
#endif

#ifndef MACHETE_BUILD_NUMBER
#define MACHETE_BUILD_NUMBER "59"
#endif

#ifndef MACHETE_GIT_COMMIT
#define MACHETE_GIT_COMMIT "ad37465"
#endif

#ifndef MACHETE_BUILD_DATE
#define MACHETE_BUILD_DATE __DATE__
#endif

#ifndef MACHETE_BUILD_TIME
#define MACHETE_BUILD_TIME __TIME__
#endif

constexpr const char *kAppVersion = MACHETE_APP_VERSION;
constexpr const char *kBuildNumber = MACHETE_BUILD_NUMBER;
constexpr const char *kGitCommit = MACHETE_GIT_COMMIT;
constexpr const char *kBuildDate = MACHETE_BUILD_DATE;
constexpr const char *kBuildTime = MACHETE_BUILD_TIME;

inline constexpr const char *version_string() noexcept
{
    return kAppVersion;
}

inline constexpr const char *build_number() noexcept
{
    return kBuildNumber;
}

inline constexpr const char *git_commit() noexcept
{
    return kGitCommit;
}

inline constexpr const char *build_date() noexcept
{
    return kBuildDate;
}

inline constexpr const char *build_time() noexcept
{
    return kBuildTime;
}

} // namespace machete::build
