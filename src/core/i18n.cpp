// MacheteSVG - Internationalization (i18n) Engine Implementation
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/i18n.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
extern "C" {
    // sceSystemServiceParamGetInt is part of libSceSystemService
    int sceSystemServiceParamGetInt(int paramId, int *value);
}
#endif

namespace machete::i18n
{

namespace
{

std::atomic<Language> s_current_lang{Language::English};

// [Language][StringId] table
const char *const s_strings[2][static_cast<std::size_t>(StringId::Count)] = {
    // English (Language::English = 0) - Primary & Default Fallback
    {
        /* AppTitle */                  "MacheteSVG",
        /* AppSubtitle */               "Next-Gen PS5 Save Manager | Direct USB & Google Drive Cloud Backup",
        /* JailbreakActiveRoot */       "Jailbreak Active (Root)",
        /* SandboxRestricted */         "Restricted Sandbox",
        /* CloudConnected */            "Google Drive Connected",
        /* CloudConnectPrompt */        "Connect Google Drive (\xCE\x94)",

        /* GamesCount */                "%zu Games",
        /* ScanningGames */             "Scanning installed games and save data...",
        /* NoGamesFound */              "No games or save data found. Install games or plug USB.",
        /* StatusPrefix */              "Status: %s",
        /* LastActivityPrefix */        "Last Activity: %s",
        /* KeystoneStatusHeader */      "DIGITAL INTEGRITY & SIGNATURE STATUS",
        /* KeystoneStatusOk */          "Keystone Verified - Protected against CE-108255-1 error",
        /* KeystoneStatusPending */     "Keystone pending (will be generated during backup)",
        /* StorageLocationsHeader */    "STANDARDIZED STORAGE LOCATIONS (/MacheteSVG/)",
        /* SaveActionsHeader */         "SAVE DATA ACTIONS",

        /* LocalStorageInfo */          "Local Storage: Internal console SSD",
        /* UsbStorageInfo */            "USB Storage: /mnt/usb0/MacheteSVG/",
        /* CloudStorageInfo */          "Cloud Storage: Google Drive",
        /* BtnBackup */                 "Backup Save Data",
        /* BtnRestore */                "Restore Save Data",
        /* BtnCloudSync */              "Sync with Google Drive",

        /* LegendSelect */              "Select",
        /* LegendBack */                "Back",
        /* LegendCloud */               "Google Drive",
        /* LegendBackup */              "Backup",
        /* LegendLanguage */            "Lang: EN (L3)",
        /* HintTheme */                 "Theme",
        /* HintNavigate */              "Navigate",
        /* HintRefresh */               "Refresh",
        /* HintLanguage */              "Language (ES)",

        /* StorageInternal */           "Internal Console SSD",
        /* StorageM2 */                 "M.2 Expansion SSD",
        /* StorageUsbFormat */          "USB Drive %d (%s)",
        /* StorageCloud */              "Google Drive (Cloud)",
        /* UsbConnectedFormat */        "USB drive connected at /mnt/usb%d",
        /* UsbDisconnectedFormat */      "USB drive disconnected from /mnt/usb%d",

        /* LocInternal */               "Internal: /data/MacheteSVG/",
        /* LocM2 */                     "SSD: /mnt/ext1/MacheteSVG/",
        /* LocUsb */                    "USB: /mnt/usb0/MacheteSVG/",
        /* LocCloud */                  "Cloud: Google Drive",

        /* DialogActionTitle */         "Save Management: %s",
        /* DialogBackupUsb */           "Create Backup on USB Drive (/mnt/usb0/)",
        /* DialogRestoreUsb */          "Restore Save from USB Drive (/mnt/usb0/)",
        /* DialogSyncCloud */           "Upload Save to Google Drive",
        /* DialogCancel */              "Cancel",
        /* DialogBackupHeading */       "Select Backup Destination",
        /* DialogRestoreHeading */      "Select Save Data Source",
        /* DialogStructureSubtitle */   "Saves organized under /MacheteSVG/[TitleID - Title]/",

        /* DriveDialogTitle */          "Google Drive Integration",
        /* DriveDialogSubtitle */       "Follow these steps on your phone or PC to authorize:",
        /* DriveDialogLinked */         "Your console is linked with Google Drive (/MacheteSVG/)",
        /* DriveStep1 */                "Step 1: Open in phone or browser:",
        /* DriveStep2 */                "Step 2: Enter authorization code:",
        /* DriveWaiting */              "Waiting for device authorization...",
        /* DriveRequestingCode */       "REQUESTING...",
        /* DriveSyncCurrent */          "Upload Current Save to Cloud",
        /* DriveRestoreCurrent */       "Download Current Save from Cloud",
        /* DriveSyncAll */              "Sync All Saves to Cloud",
        /* DriveSignOut */              "Disconnect Google Account",
        /* DriveClose */                "Close",

        /* ToastJailbreakGranted */     "Jailbreak Granted! Root Privileges Active",
        /* ToastBackupSuccess */        "Backup created successfully on USB",
        /* ToastBackupFailed */         "Backup failed: Unable to write to destination",
        /* ToastRestoreSuccess */       "Save data restored successfully",
        /* ToastRestoreFailed */        "Restore failed: Save files corrupted or missing",
        /* ToastCloudSuccess */         "Save successfully synced with Google Drive",
        /* ToastCloudFailed */          "Google Drive sync failed",
        /* ToastScanUpdated */          "Games and saves updated (%zu found)",
        /* ToastLanguageSwitched */     "Language switched to English",
        /* ToastUploadingToCloud */     "Uploading %s to Google Drive...",
        /* ToastStartingCloudSync */    "Starting full cloud backup...",
        /* ToastCloudLoggedOut */       "Google Drive session closed",
        /* ToastDownloadingFromCloud */ "Downloading from Google Drive /MacheteSVG/...",
        /* ToastUploadingGeneric */     "Uploading to Google Drive /MacheteSVG/...",
    },

    // Spanish (Language::Spanish = 1) - Para consolas en Espanol
    {
        /* AppTitle */                  "MacheteSVG",
        /* AppSubtitle */               "Gestor de Partidas Next-Gen para PS5 | Respaldo Directo en USB y Google Drive",
        /* JailbreakActiveRoot */       "Jailbreak Activo (Root)",
        /* SandboxRestricted */         "Sandbox Restringido",
        /* CloudConnected */            "Google Drive Conectado",
        /* CloudConnectPrompt */        "Conectar Google Drive (\xCE\x94)",

        /* GamesCount */                "%zu Juegos",
        /* ScanningGames */             "Buscando partidas y juegos instalados...",
        /* NoGamesFound */              "No se encontraron juegos o partidas. Instala juegos o conecta un USB.",
        /* StatusPrefix */              "Estado: %s",
        /* LastActivityPrefix */        "Ultima actividad: %s",
        /* KeystoneStatusHeader */      "ESTADO DE INTEGRIDAD Y FIRMA DIGITAL",
        /* KeystoneStatusOk */          "Keystone Verificado - Protegido contra error CE-108255-1",
        /* KeystoneStatusPending */     "Keystone pendiente (se generara durante el respaldo)",
        /* StorageLocationsHeader */    "UBICACIONES ESTANDARIZADAS (/MacheteSVG/)",
        /* SaveActionsHeader */         "ACCIONES DE GUARDADO",

        /* LocalStorageInfo */          "Almacenamiento Local: SSD interno de consola",
        /* UsbStorageInfo */            "Almacenamiento USB: /mnt/usb0/MacheteSVG/",
        /* CloudStorageInfo */          "Almacenamiento Nube: Google Drive",
        /* BtnBackup */                 "Crear Copia de Seguridad",
        /* BtnRestore */                "Restaurar Partida",
        /* BtnCloudSync */              "Sincronizar con Google Drive",

        /* LegendSelect */              "Seleccionar",
        /* LegendBack */                "Atras",
        /* LegendCloud */               "Google Drive",
        /* LegendBackup */              "Respaldar",
        /* LegendLanguage */            "Idioma: ES (L3)",
        /* HintTheme */                 "Tema",
        /* HintNavigate */              "Navegar",
        /* HintRefresh */               "Recargar",
        /* HintLanguage */              "Idioma (EN)",

        /* StorageInternal */           "Almacenamiento Interno SSD",
        /* StorageM2 */                 "SSD Expansi\xC3\xB3n M.2",
        /* StorageUsbFormat */          "Unidad USB %d (%s)",
        /* StorageCloud */              "Google Drive (Nube)",
        /* UsbConnectedFormat */        "Unidad USB conectada en /mnt/usb%d",
        /* UsbDisconnectedFormat */      "Unidad USB desconectada de /mnt/usb%d",

        /* LocInternal */               "Interno: /data/MacheteSVG/",
        /* LocM2 */                     "SSD: /mnt/ext1/MacheteSVG/",
        /* LocUsb */                    "USB: /mnt/usb0/MacheteSVG/",
        /* LocCloud */                  "Nube: Google Drive",

        /* DialogActionTitle */         "Gestion de Partida: %s",
        /* DialogBackupUsb */           "Crear Copia en USB (/mnt/usb0/)",
        /* DialogRestoreUsb */          "Restaurar desde USB (/mnt/usb0/)",
        /* DialogSyncCloud */           "Subir Partida a Google Drive",
        /* DialogCancel */              "Cancelar",
        /* DialogBackupHeading */       "Selecciona el Destino del Respaldo",
        /* DialogRestoreHeading */      "Selecciona el Origen de la Partida",
        /* DialogStructureSubtitle */   "Se creara la estructura ordenada /MacheteSVG/[TitleID - Nombre]/",

        /* DriveDialogTitle */          "Integracion de Google Drive",
        /* DriveDialogSubtitle */       "Sigue estos pasos en tu movil o PC para autorizar:",
        /* DriveDialogLinked */         "Tu consola esta vinculada con Google Drive (/MacheteSVG/)",
        /* DriveStep1 */                "Paso 1: Abre en tu movil o PC:",
        /* DriveStep2 */                "Paso 2: Introduce el codigo:",
        /* DriveWaiting */              "Esperando autorizacion del dispositivo...",
        /* DriveRequestingCode */       "SOLICITANDO...",
        /* DriveSyncCurrent */          "Subir Partida Actual a la Nube",
        /* DriveRestoreCurrent */       "Descargar Partida Actual desde la Nube",
        /* DriveSyncAll */              "Sincronizar Todas las Partidas a la Nube",
        /* DriveSignOut */              "Cerrar Sesion de Google Drive",
        /* DriveClose */                "Cerrar",

        /* ToastJailbreakGranted */     "\xC2\xA1Jailbreak Concedido! Privilegios Root Activos",
        /* ToastBackupSuccess */        "Copia de seguridad creada con exito en USB",
        /* ToastBackupFailed */         "Error al crear copia: Destino inaccesible",
        /* ToastRestoreSuccess */       "Partida restaurada con exito",
        /* ToastRestoreFailed */        "Error al restaurar: Archivos corruptos o faltantes",
        /* ToastCloudSuccess */         "Partida sincronizada con Google Drive",
        /* ToastCloudFailed */          "Error de sincronizacion con Google Drive",
        /* ToastScanUpdated */          "Juegos y partidas actualizados (%zu encontrados)",
        /* ToastLanguageSwitched */     "Idioma cambiado a Espanol",
        /* ToastUploadingToCloud */     "Subiendo %s a Google Drive...",
        /* ToastStartingCloudSync */    "Iniciando sincronizacion total en la nube...",
        /* ToastCloudLoggedOut */       "Sesion de Google Drive cerrada",
        /* ToastDownloadingFromCloud */ "Descargando de Google Drive /MacheteSVG/...",
        /* ToastUploadingGeneric */     "Subiendo a Google Drive /MacheteSVG/...",
    }
};

} // namespace

void init()
{
#if defined(__PROSPERO__) || defined(__Prospero__) || defined(__PS5__)
    int lang = -1;
    // Parameter ID 1 corresponds to SCE_SYSTEM_SERVICE_PARAM_ID_LANG
    if (sceSystemServiceParamGetInt(1, &lang) == 0)
    {
        // 3 = Spanish (Spain), 20 = Spanish (Latin America)
        if (lang == 3 || lang == 20)
        {
            s_current_lang = Language::Spanish;
            return;
        }
    }
#endif

    // Fallback: Check environment variables
    const char *env_lang = std::getenv("LANG");
    if (!env_lang)
        env_lang = std::getenv("LC_ALL");

    if (env_lang)
    {
        std::string s(env_lang);
        if (s.rfind("es", 0) == 0 || s.find("Spanish") != std::string::npos)
        {
            s_current_lang = Language::Spanish;
            return;
        }
    }

    // Default to English for all other languages (Japanese, French, German, Italian, etc.)
    s_current_lang = Language::English;
}

Language current_language()
{
    return s_current_lang.load(std::memory_order_relaxed);
}

void set_language(Language lang)
{
    s_current_lang.store(lang, std::memory_order_relaxed);
}

void toggle_language()
{
    Language expected = s_current_lang.load(std::memory_order_relaxed);
    Language desired = (expected == Language::English) ? Language::Spanish : Language::English;
    while (!s_current_lang.compare_exchange_weak(expected, desired, std::memory_order_relaxed))
    {
        desired = (expected == Language::English) ? Language::Spanish : Language::English;
    }
}

const char *get(StringId id)
{
    const auto idx = static_cast<std::size_t>(id);
    if (idx >= static_cast<std::size_t>(StringId::Count))
        return "";

    const auto lang_idx = static_cast<std::size_t>(s_current_lang.load(std::memory_order_relaxed));
    return s_strings[lang_idx][idx];
}

} // namespace machete::i18n
