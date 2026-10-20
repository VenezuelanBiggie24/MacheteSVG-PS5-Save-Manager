// MacheteSVG - Event Logging, Crash Diagnostics & Breadcrumb Tracing
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdarg>
#include <cstdint>

namespace machete::log
{

// Inicializa el subsistema de registro y los interceptores de señales fatales (SIGSEGV, SIGBUS, etc.)
void init();

// Registra la ruta de ejecución actual (hilo de migajas)
void set_breadcrumb(const char *step);

// Obtiene la última migaja registrada
const char *get_breadcrumb();

// Escribe un mensaje con formato en todos los destinos de registro activos
void write(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

// Versión va_list de write
void vwrite(const char *tag, const char *fmt, va_list args);

// Reabre rutas en disco tras adquirir privilegios root fuera del sandbox
void reopen_paths();

// Reinicia y trunca los archivos de registro para iniciar una sesion limpia tras una actualizacion
void reset_fresh();

// Vuelca y sincroniza físicamente todos los descriptores de archivo en disco
void flush();

} // namespace machete::log

#define MACHETE_LOG(tag, ...) ::machete::log::write(tag, __VA_ARGS__)
#define MACHETE_BREADCRUMB(step) ::machete::log::set_breadcrumb(step)
