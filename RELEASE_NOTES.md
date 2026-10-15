# Release Notes: MacheteSVG v1.0.0 (Pre-release / Technical Preview) / Notas de la Versión v1.0.0 (Pre-release / Vista Previa Técnica)

![MacheteSVG Banner](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/banner.png)

---

## English Documentation

### Technical Preview Notice
This release is an official public pre-release / technical preview build intended for testing and validation across jailbroken PlayStation 5 consoles.

While core mechanics and UI rendering are fully operational, several advanced subsystems are currently undergoing active real-hardware stabilization:
- Full icon decoding throughput and memory caching for large game libraries.
- Sanitization and filtering of non-Latin / Unicode characters in game titles and directory trees.
- Background asynchronous file copying pipeline and progress reporting.
- Google Drive cloud synchronization device flow and upload/download queue reliability.

Active engineering is underway on these areas to ensure rock-solid stability, zero lockups, and optimal performance. A comprehensive update incorporating all final fixes and full functionality will be released in the upcoming version.

---

### Overview
MacheteSVG v1.0.0 is the initial public technical preview of the native PlayStation 5 and PlayStation 4 Savegame Manager and Cloud Sync utility for jailbroken PlayStation 5 consoles.

Built with a hardware-accelerated OpenGL 4.6 SDF renderer, MacheteSVG delivers a fluid 60 FPS "Liquid Glass" interface paired with cryptographic savefile safety features designed to prevent save corruption (`CE-108255-1`) and enable auto-resigning across accounts.

---

### Visual Interface & Real Hardware Captures

| English Dashboard (1080p Render) | PS5 Hardware Capture (Real Console) |
| :---: | :---: |
| ![English Dashboard](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/dashboard_english_1080p.png) | ![Real Hardware Dashboard](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/ps5_real_hardware_dashboard.png) |

| Google Drive RFC 8628 Device Flow | Modal Location Selector (Real Console) |
| :---: | :---: |
| ![Google Drive Dialog](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/google_drive_dialog_1080p.png) | ![Real Hardware Modal](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/ps5_real_hardware_modal.png) |

---

### Key Capabilities

#### 1. Zero-Corruption Save Architecture & Error CE-107109-8 Resolution
- **PS5 Dual-Folder Synchronization:** Backs up and restores both `/user/home/<id>/savedata_prospero/<TitleID>` (raw encrypted `sdimg_*` images) and `/user/home/<id>/savedata_prospero_meta/<TitleID>` (save database index, parameters, icons). Resolves error `CE-107109-8` by ensuring the Prospero savedata daemon indexes restored saves without conflicts.
- **Apollo Save Tool Integration & PS4 Re-signing:** Automatically extracts and injects authentic `keystone` files from `/user/appmeta/<TitleID>/sce_sys/keystone`. Patches `param.sfo` (`ACCOUNT_ID`, `PARAMS.user_id`, and Sony `psid_hmac` recalculation with `PSID_HMAC_KEY`) so foreign or pre-reinstall PS4 saves (`CUSAxxxxx`) load seamlessly on the active user account (`0x105f615a`).
- **User Ownership & Open Permissions:** Dynamically detects user account ownership (`/user/home/<user_id>`) and applies matching UID/GID alongside open file permissions (`0666` files, `0777` directories) with kernel sync flush, ensuring unprivileged game processes load restored saves immediately without permission errors.
- **SAMU Hardware Cryptographic Vault:** Archives console-unique 96-byte SAMU sealed blobs, plaintext 32-byte AES PFS keys via `/dev/pfsmgr` ioctl `0xc0845302` (`PFS_DECRYPT_KEK`), OpenPSID, and SHA-256 integrity signatures in `crypto_vault.bin` alongside each backup for digital preservation and future cross-console decryption.

#### 2. Google Drive Cloud Synchronization & Autonomous DNS Bypass
- **Autonomous RFC 1035 UDP DNS Resolver:** Queries upstream public DNS servers (`8.8.8.8`, `1.1.1.1`) directly over WAN UDP port 53, completely bypassing local loopback filtering payloads such as `NanoDNS` (`0x8041013D`) without requiring network reconfiguration or payload toggling.
- **Direct TLS Client with SNI:** Native BSD socket connection paired with `libSceSsl` transmits authentic SNI and HTTP headers directly to resolved Google IPs.
- **OAuth 2.0 Device Flow (RFC 8628):** Link your Google account securely without entering credentials on the console. Simply navigate to `https://www.google.com/device` and enter the code shown on screen.
- **Asynchronous Execution:** Upload and download operations run inside dedicated worker threads, ensuring the UI remains responsive at 60 FPS.
- **Storage Standard:** Backups are structured in cloud root directory `/MacheteSVG/[TitleID - Game Name]/`.

#### 3. Autonomous Sandbox Escape & Community Title ID Standard
- **Title ID `PPSA99001`:** Conforms to the community-adopted homebrew allocation scheme (`PPSA99xxx`) to avoid collisions with commercial PlayStation titles.
- **Embedded Lapy Daemon:** Packages `lapy_jb_daemon.elf` within the application. The app attempts automatic connection to the local ELF loader (`127.0.0.1:9021`) and triggers privilege elevation without requiring external payload injection.

#### 4. Internationalization Engine (i18n)
- **Automatic Language Detection:** Console language is detected at launch. Spanish consoles initialize in Spanish, while English and all other languages default to English.
- **On-the-Fly Toggle:** Press **L3** at any moment to switch instantly between English and Spanish.

#### 5. Persistent Event Logging & Crash Diagnostics Engine
- **Multi-Destination Real-Time Logging:** Writes execution traces and subsystem state directly to disk at `/data/homebrew/PPSA99001/machete.log`, `/download0/machete.log` (accessible within sandbox), `/data/machete.log`, and `/user/app/PPSA99001/machete.log`.
- **Async-Signal-Safe Crash Catcher:** Intercepts fatal signals (`SIGSEGV`, `SIGBUS`, `SIGABRT`, `SIGILL`, `SIGFPE`, `SIGSYS`, `SIGTRAP`) with fault memory addresses, signal codes, and execution breadcrumbs, flushing them immediately with raw POSIX syscalls (`write`, `fsync`) without heap allocations or mutex deadlocks.
- **Root Elevation Re-open:** Automatically establishes disk logging descriptors into `/data/homebrew/PPSA99001/` immediately upon sandbox escape.

---

### Controls Summary
- **D-Pad Up / Down:** Navigate installed games list.
- **Cross (X):** Confirm selection / Backup save data.
- **Square:** Restore save data from selected storage location.
- **Triangle:** Open Google Drive cloud synchronization.
- **L3 (Left Stick Click):** Toggle interface language (English / Spanish).
- **Circle:** Cancel dialog / Back.

---

### Distribution Packages & Integrity Checksums

| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| `MacheteSVG_v1.0.0_PS5.zip` | 29.5 MB (30,981,748 bytes) | `fd404538a96b2f399f5348c28bcabd2fc45a0ce9900fe737da1d90bb11287ca3` |
| `eboot.bin` | 23.8 MB (24,909,407 bytes) | `b2273fdc041f973233365d379dae0cbbbd955d168288c941eb7616186a4e2120` |

All package files are validated with open-mode permissions (`0777`) for out-of-the-box execution on jailbroken PlayStation 5 systems.

---

### Installation Instructions
1. Download `MacheteSVG_v1.0.0_PS5.zip` from the release assets.
2. Extract the archive contents (`PPSA99001` folder).
3. Connect to your PS5 via FTP or use a file manager homebrew.
4. Upload the `PPSA99001` folder to `/data/homebrew/` or `/user/app/`.
5. Launch MacheteSVG from your homebrew loader (Itemzflow, ProsperoStore, or etaHEN menu).

---

## Notas de la Versión en Español (Spanish Documentation)

### Aviso de Vista Previa Técnica
Esta entrega es una versión pública preliminar (pre-release / technical preview) destinada a la validación y pruebas directas en consolas PlayStation 5 con jailbreak.

Si bien las funciones esenciales y el motor de renderizado de interfaz son totalmente operativos, existen módulos que se encuentran en proceso activo de estabilización sobre hardware real:
- Tasa y rendimiento completo de decodificación de iconos y gestión de memoria para librerías con gran cantidad de juegos.
- Filtrado y saneamiento de caracteres no latinos / Unicode en títulos y rutas del sistema de archivos.
- Tubería de copias de seguridad en segundo plano y reporte reactivo de progreso.
- Sincronización en la nube con Google Drive (flujo de autorización e hilos de transferencia).

Se está trabajando intensamente en optimizar y corregir cada uno de estos aspectos para garantizar máxima solidez, fluidez absoluta y ausencia total de bloqueos. La siguiente versión integrará todas estas mejoras de manera definitiva.

---

### Resumen General
MacheteSVG v1.0.0 es la versión pública inicial del administrador de partidas guardadas de PlayStation 5 y PlayStation 4 con sincronización en la nube para consolas PlayStation 5 con jailbreak.

Desarrollado con renderizado acelerado por hardware OpenGL 4.6 SDF, MacheteSVG ofrece una interfaz fluida a 60 FPS con diseño "Liquid Glass" y mecanismos de seguridad criptográfica diseñados para evitar la corrupción de datos (`CE-108255-1`) y permitir el resignado automático entre diferentes perfiles de usuario.

---

### Capturas de Pantalla y Hardware Real

| Panel Principal en Inglés (Render 1080p) | Captura de Hardware Real en PS5 |
| :---: | :---: |
| ![Panel en Inglés](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/dashboard_english_1080p.png) | ![Panel en Consola Real](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/ps5_real_hardware_dashboard.png) |

| Autorización Google Drive RFC 8628 | Selector Modal de Ubicación (Consola Real) |
| :---: | :---: |
| ![Autorización Google Drive](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/google_drive_dialog_1080p.png) | ![Selector Modal](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/ps5_real_hardware_modal.png) |

---

### Capacidades Principales

#### 1. Arquitectura de Guardado con Cero Corrupción y Resolución del Error CE-107109-8
- **Sincronización Dual de Carpetas en PS5:** Respalda y restaura simultáneamente `/user/home/<id>/savedata_prospero/<TitleID>` (imágenes planas cifradas `sdimg_*`) y `/user/home/<id>/savedata_prospero_meta/<TitleID>` (base de datos indexada, parámetros e iconos). Elimina la causa raíz del error `CE-107109-8` al permitir que el demonio de guardado de Prospero reconozca e indexe de inmediato las partidas restauradas.
- **Integración del Motor de Apollo Save Tool y Resignado de PS4:** Extrae e inyecta automáticamente el archivo `keystone` auténtico desde `/user/appmeta/<TitleID>/sce_sys/keystone`. Modifica y refirma `param.sfo` (`ACCOUNT_ID`, `PARAMS.user_id` y recálculo del `psid_hmac` de Sony mediante la clave maestra `PSID_HMAC_KEY`) para que partidas de PS4 procedentes de otras consolas o instalaciones previas funcionen de inmediato en la cuenta actual (`0x105f615a`).
- **Propiedad de Usuario y Permisos Abiertos:** Detecta dinámicamente la propiedad de la cuenta del usuario (`/user/home/<user_id>`) y asigna los UID/GID correspondientes junto con permisos abiertos (`0666` en archivos, `0777` en directorios) y sincronización con el kernel, garantizando que el juego lea la partida inmediatamente sin conflictos de permisos.
- **Bóveda Criptográfica de Hardware SAMU:** Archiva el blob sellado por SAMU de 96 bytes, la clave AES PFS en texto plano de 32 bytes desellada en vivo mediante `/dev/pfsmgr` (ioctl `0xc0845302`), el OpenPSID y hashes de integridad SHA-256 en `crypto_vault.bin` junto a cada copia de seguridad para preservación digital y futuro descifrado entre consolas.

#### 2. Sincronización en la Nube con Google Drive y Evasión Autónoma de DNS
- **Resolvedor DNS Autónomo RFC 1035 UDP:** Realiza consultas directas por sockets UDP WAN al puerto 53 de servidores públicos (`8.8.8.8`, `1.1.1.1`), evitando de forma completamente autónoma bloqueadores locales como `NanoDNS` (`0x8041013D`) sin necesidad de alterar configuraciones de red ni desactivar payloads.
- **Cliente TLS Directo con SNI:** Conexión por socket BSD nativo combinada con `libSceSsl` que transmite SNI y cabeceras HTTP legítimas directamente a las IPs resueltas de Google.
- **OAuth 2.0 Device Flow (RFC 8628):** Vincula tu cuenta de Google de forma segura sin ingresar contraseñas en la consola. Ingresa a `https://www.google.com/device` e introduce el código que se muestra en pantalla.
- **Ejecución Asíncrona:** Las transferencias se ejecutan en hilos dedicados para que la interfaz se mantenga a 60 FPS sin bloqueos.
- **Estructura Estándar:** Las copias de seguridad se organizan en la carpeta `/MacheteSVG/[TitleID - Nombre del Juego]/`.

#### 3. Escape Autónomo del Sandbox y Title ID Comunitario
- **Title ID `PPSA99001`:** Se adapta al estándar establecido por la comunidad de homebrew (`PPSA99xxx`) para evitar conflictos con identificadores de juegos oficiales.
- **Demonio Lapy Integrado:** Incluye `lapy_jb_daemon.elf` dentro del paquete. La aplicación se conecta de forma automática al cargador ELF local (`127.0.0.1:9021`) y solicita la elevación de privilegios sin depender de inyectores externos.

#### 4. Motor de Internacionalización (i18n)
- **Detección Automática de Idioma:** Detecta el idioma del sistema al iniciar. Las consolas en español abren en español; las consolas en inglés u otros idiomas abren en inglés.
- **Cambio Inmediato:** Presiona **L3** en cualquier momento para alternar al instante entre inglés y español.

#### 5. Motor de Registro Persistente de Eventos y Diagnóstico Forense de Fallos
- **Registro en Múltiples Destinos en Tiempo Real:** Escribe trazas de ejecución y estado de los subsistemas directamente en disco en `/data/homebrew/PPSA99001/machete.log`, `/download0/machete.log` (accesible dentro del sandbox), `/data/machete.log` y `/user/app/PPSA99001/machete.log`.
- **Captura Forense Inmune a Bloqueos (Async-Signal-Safe):** Intercepta señales fatales (`SIGSEGV`, `SIGBUS`, `SIGABRT`, `SIGILL`, `SIGFPE`, `SIGSYS`, `SIGTRAP`) registrando la dirección de memoria fallida, código de señal y última migaja de ejecución (breadcrumb), volcándolos al disco inmediatamente con llamadas de bajo nivel POSIX (`write`, `fsync`) sin usar asignaciones dinámicas ni mutexes que provoquen bloqueos mutuos.
- **Reapertura al Salir del Sandbox:** Tras el escape del sandbox y la elevación a root, reabre automáticamente los descriptores hacia `/data/homebrew/PPSA99001/` para garantizar persistencia absoluta.

---

### Resumen de Controles
- **D-Pad Arriba / Abajo:** Desplazarse por la lista de juegos.
- **Cruz (X):** Confirmar selección / Realizar copia de seguridad.
- **Cuadrado:** Restaurar partida guardada desde la ubicación seleccionada.
- **Triángulo:** Abrir sincronización en la nube con Google Drive.
- **L3 (Click Stick Izquierdo):** Alternar idioma de la interfaz (Inglés / Español).
- **Círculo:** Cancelar diálogo / Retroceder.

---

### Paquetes de Distribución y Sumas de Verificación

| Archivo | Tamaño | Suma de Verificación SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.0.0_PS5.zip` | 29.5 MB (30,981,748 bytes) | `fd404538a96b2f399f5348c28bcabd2fc45a0ce9900fe737da1d90bb11287ca3` |
| `eboot.bin` | 23.8 MB (24,909,407 bytes) | `b2273fdc041f973233365d379dae0cbbbd955d168288c941eb7616186a4e2120` |

Todos los archivos han sido generados y verificados con permisos de modo abierto (`0777`) para ejecución inmediata en consolas PlayStation 5.

---

### Instrucciones de Instalación
1. Descarga el archivo `MacheteSVG_v1.0.0_PS5.zip` desde los activos de la versión.
2. Descomprime el contenido del archivo (carpeta `PPSA99001`).
3. Conéctate a tu PS5 mediante FTP o utiliza un gestor de archivos homebrew.
4. Sube la carpeta `PPSA99001` a `/data/homebrew/` o `/user/app/`.
5. Ejecuta MacheteSVG desde tu cargador de homebrew habitual (Itemzflow, ProsperoStore o menú de etaHEN).
