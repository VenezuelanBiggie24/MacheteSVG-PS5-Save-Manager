// MacheteSVG - Internationalization (i18n) Engine
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>

namespace machete::i18n
{

enum class Language
{
    English = 0,
    Spanish = 1,
};

enum class StringId
{
    // Application Header
    AppTitle,
    AppSubtitle,
    JailbreakActiveRoot,
    SandboxRestricted,
    WebServerActive,
    WebServerPrompt,

    // Games List & Details
    GamesCount,
    ScanningGames,
    NoGamesFound,
    StatusPrefix,
    LastActivityPrefix,
    KeystoneStatusHeader,
    KeystoneStatusOk,
    KeystoneStatusPending,
    StorageLocationsHeader,
    SaveActionsHeader,

    // Storage Details & Buttons
    LocalStorageInfo,
    UsbStorageInfo,
    WebServerStorageInfo,
    BtnBackup,
    BtnRestore,
    BtnWebServer,

    // Footer Legend & DualSense Hints
    LegendSelect,
    LegendBack,
    LegendWebServer,
    LegendBackup,
    LegendLanguage,
    HintTheme,
    HintNavigate,
    HintRefresh,
    HintLanguage,

    // Storage Devices & Hotplug
    StorageInternal,
    StorageM2,
    StorageUsbFormat,
    StorageWebServer,
    UsbConnectedFormat,
    UsbDisconnectedFormat,

    // Standardized Storage Location Tags
    LocInternal,
    LocM2,
    LocUsb,
    LocWebServer,

    // Action Modal Dialog
    DialogActionTitle,
    DialogBackupUsb,
    DialogRestoreUsb,
    DialogSyncWebServer,
    DialogCancel,
    DialogBackupHeading,
    DialogRestoreHeading,
    DialogStructureSubtitle,

    // Web Server Dialog
    WebServerDialogTitle,
    WebServerDialogSubtitle,
    WebServerStatusRunning,
    WebServerStatusStopped,
    WebServerUrlLabel,
    WebServerInstruction,
    WebServerBtnStart,
    WebServerBtnStop,
    WebServerClose,

    // Notifications and Toasts
    ToastJailbreakGranted,
    ToastBackupSuccess,
    ToastBackupFailed,
    ToastRestoreSuccess,
    ToastRestoreFailed,
    ToastWebServerStarted,
    ToastWebServerStopped,
    ToastScanUpdated,
    ToastLanguageSwitched,

    // Count marker
    Count
};

// Initializes language from PS5 system settings (sceSystemServiceParamGetInt) or env.
// Non-Spanish systems automatically default to English.
void init();

// Returns the current active language
Language current_language();

// Sets language explicitly
void set_language(Language lang);

// Toggles between English and Spanish
void toggle_language();

// Returns localized string for the specified ID
const char *get(StringId id);

// Convenience inline function
inline const char *tr(StringId id)
{
    return get(id);
}

} // namespace machete::i18n
