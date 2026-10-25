// MacheteSVG - Sandbox Escape & Jailbreak Daemon Integration
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Interfaces with etaHEN, OnionHEN, and Lapy JB Daemon via the standardized
// /download0/etahen_jailbreak jailbreak-on-demand IPC interface to unsandbox
// the application and gain root access to /data, /user/home, and /user/app.

#pragma once

#include <string>

namespace machete::jailbreak
{

enum class JailbreakStatus
{
    Unchecked,
    Escaped,     // Root/Unsandboxed: /data y /user/home son plenamente accesibles
    Requested,   // Petición enviada al daemon, en espera de ucred bump
    Sandboxed    // Sin daemon activo: restringido al sandbox de la aplicación
};

struct JailbreakInfo
{
    JailbreakStatus status = JailbreakStatus::Unchecked;
    int pid = 0;
    std::string daemon_type;     // "Lapy JB Daemon / etaHEN / OnionHEN"
    std::string status_message;  // Descripción legible para la UI
    bool root_access = false;    // getuid() == 0 o acceso de lectura/escritura a /data
};

// Intenta escapar del sandbox solicitando la elevación a Lapy JB Daemon / etaHEN / OnionHEN.
// Bloquea brevemente (hasta 1.5s en sondeo de 50ms) esperando a que el daemon consuma el archivo.
JailbreakInfo request_jailbreak();

// Verifica el escape del sandbox y privilegios de lectura/escritura (UID 0)
bool verify_root_access(int pid = 0) noexcept;

// Comprueba el estado actual de privilegios y acceso a sistemas de archivos
JailbreakInfo check_status();

// Lanza de forma autónoma el daemon integrado enviándolo al ELF loader local (127.0.0.1:9021),
// idéntico a la arquitectura autónoma de ProsperoStore / Homebrew Store.
bool auto_launch_embedded_daemon();

// Limpia de forma activa PPSA99700 de listas blancas y configuraciones de ShadowMount / etaHEN
// para evitar que el payload intente pausar KStuff y cerrar la aplicación homebrew.
void clean_shadowmount_interference() noexcept;

// Devuelve una cadena descriptiva para mostrar en la interfaz gráfica
const char *status_to_string(JailbreakStatus status);

} // namespace machete::jailbreak
