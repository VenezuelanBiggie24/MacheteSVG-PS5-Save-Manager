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
        /* AppSubtitle */               "Next-Gen PS5 Save Manager & Web Station | Direct USB & Local Browser Access",
        /* JailbreakActiveRoot */       "Jailbreak Active (Root)",
        /* SandboxRestricted */         "Restricted Sandbox",
        /* WebServerActive */           "Web Station Active",
        /* WebServerPrompt */           "Web Station (\xCE\x94)",

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
        /* WebServerStorageInfo */      "Web Station: Local Browser Downloads",
        /* BtnBackup */                 "Backup Save Data",
        /* BtnRestore */                "Restore Save Data",
        /* BtnWebServer */              "Web Station Server",

        /* LegendSelect */              "Select",
        /* LegendBack */                "Back",
        /* LegendWebServer */           "Web Station",
        /* LegendBackup */              "Backup",
        /* LegendLanguage */            "Lang: EN (L3)",
        /* HintTheme */                 "Theme",
        /* HintNavigate */              "Navigate",
        /* HintRefresh */               "Refresh",
        /* HintLanguage */              "Language (ES)",

        /* StorageInternal */           "Internal Console SSD",
        /* StorageM2 */                 "M.2 Expansion SSD",
        /* StorageUsbFormat */          "USB Drive %d (%s)",
        /* StorageWebServer */          "Web Station Server",
        /* UsbConnectedFormat */        "USB drive connected at /mnt/usb%d",
        /* UsbDisconnectedFormat */      "USB drive disconnected from /mnt/usb%d",

        /* LocInternal */               "Internal: /data/MacheteSVG/",
        /* LocM2 */                     "SSD: /mnt/ext1/MacheteSVG/",
        /* LocUsb */                    "USB: /mnt/usb0/MacheteSVG/",
        /* LocWebServer */              "Web: http://<IP>:8383",

        /* DialogActionTitle */         "Save Management: %s",
        /* DialogBackupUsb */           "Create Backup on USB Drive (/mnt/usb0/)",
        /* DialogRestoreUsb */          "Restore Save from USB Drive (/mnt/usb0/)",
        /* DialogSyncWebServer */       "Download via Web Station",
        /* DialogCancel */              "Cancel",
        /* DialogBackupHeading */       "Select Backup Destination",
        /* DialogRestoreHeading */      "Select Save Data Source",
        /* DialogStructureSubtitle */   "Saves organized under /MacheteSVG/[TitleID - Title]/",

        /* WebServerDialogTitle */      "MacheteSVG Web Station",
        /* WebServerDialogSubtitle */   "Access and download saves directly from your PC or mobile phone:",
        /* WebServerStatusRunning */    "STATUS: ACTIVE (LISTENING)",
        /* WebServerStatusStopped */    "STATUS: STOPPED",
        /* WebServerUrlLabel */         "Browser Address (URL):",
        /* WebServerInstruction */      "Open this URL on any device connected to the same Wi-Fi / LAN network.",
        /* WebServerBtnStart */         "Start Web Server",
        /* WebServerBtnStop */          "Stop Web Server",
        /* WebServerClose */            "Close (Keeps running in background)",

        /* ToastJailbreakGranted */     "Jailbreak Granted! Root Privileges Active",
        /* ToastBackupSuccess */        "Backup created successfully on USB",
        /* ToastBackupFailed */         "Backup failed: Unable to write to destination",
        /* ToastRestoreSuccess */       "Save data restored successfully",
        /* ToastRestoreFailed */        "Restore failed: Save files corrupted or missing",
        /* ToastWebServerStarted */     "Web Server started! Listening for browser connections",
        /* ToastWebServerStopped */     "Web Server stopped",
        /* ToastScanUpdated */          "Games and saves updated (%zu found)",
        /* ToastLanguageSwitched */     "Language switched to English",
    },

    // Spanish (Language::Spanish = 1) - Para consolas en Espanol
    {
        /* AppTitle */                  "MacheteSVG",
        /* AppSubtitle */               "Gestor de Partidas Next-Gen y Estacion Web | Respaldo Directo en USB y Navegador",
        /* JailbreakActiveRoot */       "Jailbreak Activo (Root)",
        /* SandboxRestricted */         "Sandbox Restringido",
        /* WebServerActive */           "Estacion Web Activa",
        /* WebServerPrompt */           "Estacion Web (\xCE\x94)",

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
        /* WebServerStorageInfo */      "Estacion Web: Descargas directas en PC/Movil",
        /* BtnBackup */                 "Crear Copia de Seguridad",
        /* BtnRestore */                "Restaurar Partida",
        /* BtnWebServer */              "Abrir Estacion Web",

        /* LegendSelect */              "Seleccionar",
        /* LegendBack */                "Atras",
        /* LegendWebServer */           "Estacion Web",
        /* LegendBackup */              "Respaldar",
        /* LegendLanguage */            "Idioma: ES (L3)",
        /* HintTheme */                 "Tema",
        /* HintNavigate */              "Navegar",
        /* HintRefresh */               "Recargar",
        /* HintLanguage */              "Idioma (EN)",

        /* StorageInternal */           "Almacenamiento Interno SSD",
        /* StorageM2 */                 "SSD Expansi\xC3\xB3n M.2",
        /* StorageUsbFormat */          "Unidad USB %d (%s)",
        /* StorageWebServer */          "Servidor Web Local",
        /* UsbConnectedFormat */        "Unidad USB conectada en /mnt/usb%d",
        /* UsbDisconnectedFormat */      "Unidad USB desconectada de /mnt/usb%d",

        /* LocInternal */               "Interno: /data/MacheteSVG/",
        /* LocM2 */                     "SSD: /mnt/ext1/MacheteSVG/",
        /* LocUsb */                    "USB: /mnt/usb0/MacheteSVG/",
        /* LocWebServer */              "Web: http://<IP>:8383",

        /* DialogActionTitle */         "Gestion de Partida: %s",
        /* DialogBackupUsb */           "Crear Copia en USB (/mnt/usb0/)",
        /* DialogRestoreUsb */          "Restaurar desde USB (/mnt/usb0/)",
        /* DialogSyncWebServer */       "Descargar via Estacion Web",
        /* DialogCancel */              "Cancelar",
        /* DialogBackupHeading */       "Selecciona el Destino del Respaldo",
        /* DialogRestoreHeading */      "Selecciona el Origen de la Partida",
        /* DialogStructureSubtitle */   "Se creara la estructura ordenada /MacheteSVG/[TitleID - Nombre]/",

        /* WebServerDialogTitle */      "Estacion Web MacheteSVG",
        /* WebServerDialogSubtitle */   "Accede y descarga tus partidas directamente desde tu PC o telefono:",
        /* WebServerStatusRunning */    "ESTADO: ACTIVO (CONECTADO)",
        /* WebServerStatusStopped */    "ESTADO: DETENIDO",
        /* WebServerUrlLabel */         "Direccion en Navegador (URL):",
        /* WebServerInstruction */      "Abre esta URL en cualquier dispositivo conectado a la misma red Wi-Fi / LAN.",
        /* WebServerBtnStart */         "Iniciar Servidor Web",
        /* WebServerBtnStop */          "Detener Servidor Web",
        /* WebServerClose */            "Cerrar (Continua activo en segundo plano)",

        /* ToastJailbreakGranted */     "\xC2\xA1Jailbreak Concedido! Privilegios Root Activos",
        /* ToastBackupSuccess */        "Copia de seguridad creada con exito en USB",
        /* ToastBackupFailed */         "Error al crear copia: Destino inaccesible",
        /* ToastRestoreSuccess */       "Partida restaurada con exito",
        /* ToastRestoreFailed */        "Error al restaurar: Archivos corruptos o faltantes",
        /* ToastWebServerStarted */     "Servidor Web iniciado. Listo para descargas desde navegador",
        /* ToastWebServerStopped */     "Servidor Web detenido",
        /* ToastScanUpdated */          "Juegos y partidas actualizados (%zu encontrados)",
        /* ToastLanguageSwitched */     "Idioma cambiado a Espanol",
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
