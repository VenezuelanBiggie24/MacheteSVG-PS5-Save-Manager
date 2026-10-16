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
- Web Save Station local HTTP streaming and concurrent browser download management.

Active engineering is underway on these areas to ensure rock-solid stability, zero lockups, and optimal performance. A comprehensive update incorporating all final fixes and full functionality will be released in the upcoming version.

---

### Overview
MacheteSVG v1.0.0 is the initial public technical preview of the native PlayStation 5 and PlayStation 4 Savegame Manager and Web Save Station utility for jailbroken PlayStation 5 consoles.

Built with a hardware-accelerated OpenGL 4.6 SDF renderer, MacheteSVG delivers a fluid 60 FPS "Liquid Glass" interface paired with cryptographic savefile safety features designed to prevent save corruption (`CE-108255-1`) and enable auto-resigning across accounts.

---

### Visual Interface & Real Hardware Captures

| English Dashboard (1080p Render) | PS5 Hardware Capture (Real Console) |
| :---: | :---: |
| ![English Dashboard](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/dashboard_english_1080p.png) | ![Real Hardware Dashboard](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/ps5_real_hardware_dashboard.png) |

| Web Save Station Browser UI (1080p) | Web Save Station Console Dialog |
| :---: | :---: |
| ![Web Station Browser](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/web_station_browser_1080p.png) | ![Web Save Station](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/web_station_dialog_1080p.png) |

---

### Key Capabilities

#### 1. Zero-Corruption Save Architecture & Error CE-107109-8 Resolution
- **PS5 Dual-Folder Synchronization:** Backs up and restores both `/user/home/<id>/savedata_prospero/<TitleID>` (raw encrypted `sdimg_*` images) and `/user/home/<id>/savedata_prospero_meta/<TitleID>` (save database index, parameters, icons). Resolves error `CE-107109-8` by ensuring the Prospero savedata daemon indexes restored saves without conflicts.
- **Apollo Save Tool Integration & PS4 Re-signing:** Automatically extracts and injects authentic `keystone` files from `/user/appmeta/<TitleID>/sce_sys/keystone`. Patches `param.sfo` (`ACCOUNT_ID`, `PARAMS.user_id`, and Sony `psid_hmac` recalculation with `PSID_HMAC_KEY`) so foreign or pre-reinstall PS4 saves (`CUSAxxxxx`) load seamlessly on the active user account (`0x105f615a`).
- **User Ownership & Open Permissions:** Dynamically detects user account ownership (`/user/home/<user_id>`) and applies matching UID/GID alongside open file permissions (`0666` files, `0777` directories) with kernel sync flush, ensuring unprivileged game processes load restored saves immediately without permission errors.
- **SAMU Hardware Cryptographic Vault:** Archives console-unique 96-byte SAMU sealed blobs, plaintext 32-byte AES PFS keys via `/dev/pfsmgr` ioctl `0xc0845302` (`PFS_DECRYPT_KEK`), OpenPSID, and SHA-256 integrity signatures in `crypto_vault.bin` alongside each backup for digital preservation and future cross-console decryption.

#### 2. Embedded Web Save Station (LAN / Wi-Fi Download Server)
- **Local HTTP Web Server:** Integrated high-performance HTTP server listening on port 8383 with automatic PS5 LAN IP detection. Runs on a dedicated background thread with 4 MB stack without blocking the 60 FPS UI.
- **Modern Dark Glassmorphism Web UI:** Responsive single-page web app (HTML5 / CSS3 / ES6) embedded directly inside the binary. Zero external CDN dependencies; 100% offline-ready.
- **Live Game Save Browser:** View installed games, real cover art, Title IDs, platform tags (PS5 / PS4), and save sizes from your phone or PC browser.
- **Instant ZIP Streaming:** Custom C++ streaming ZIP archive generator (IEEE 802.3 CRC32, Store mode) allowing one-click download of individual game saves or an entire library archive (`.zip`) with precalculated `Content-Length` for accurate browser progress bars.
- **Individual File Explorer:** Interactive modal to inspect files inside each save directory and download individual files (`param.json`, `sdimg_*`, `keystone`).
- **One-Touch Activation:** Toggle the server on/off via the top header badge or by pressing **Triangle** on your DualSense controller.

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
- **Triangle:** Open Web Save Station (LAN / Wi-Fi local HTTP server).
- **L3 (Left Stick Click):** Toggle interface language (English / Spanish).
- **Circle:** Cancel dialog / Back.

---

### Distribution Packages & Integrity Checksums

| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| `MacheteSVG_v1.0.0_PS5.zip` | 29.5 MB (30,975,227 bytes) | `5b006c0a457b1ef8a8e3504e2aaf1cbc173d59cfa92c289cbe535118d4a03a6a` |
| `eboot.bin` | 23.8 MB (24,913,671 bytes) | `504b05e3fd61b30abbb288a754bbba4512d3b1fa7daa1651547627e0b7b95c24` |

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
- Estación Web Local (transmisión streaming de archivos ZIP y gestión de descargas concurrentes en el navegador).

Se está trabajando intensamente en optimizar y corregir cada uno de estos aspectos para garantizar máxima solidez, fluidez absoluta y ausencia total de bloqueos. La siguiente versión integrará todas estas mejoras de manera definitiva.

---

### Resumen General
MacheteSVG v1.0.0 es la versión pública inicial del administrador de partidas guardadas de PlayStation 5 y PlayStation 4 con Estación Web Local integrada para consolas PlayStation 5 con jailbreak.

Desarrollado con renderizado acelerado por hardware OpenGL 4.6 SDF, MacheteSVG ofrece una interfaz fluida a 60 FPS con diseño "Liquid Glass" y mecanismos de seguridad criptográfica diseñados para evitar la corrupción de datos (`CE-108255-1`) y permitir el resignado automático entre diferentes perfiles de usuario.

---

### Capturas de Pantalla y Hardware Real

| Panel Principal en Inglés (Render 1080p) | Captura de Hardware Real en PS5 |
| :---: | :---: |
| ![Panel en Inglés](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/dashboard_english_1080p.png) | ![Panel en Consola Real](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/ps5_real_hardware_dashboard.png) |

| Interfaz Web en Navegador (1080p) | Diálogo de Estación Web Local (Consola) |
| :---: | :---: |
| ![Interfaz Web en Navegador](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/web_station_browser_1080p.png) | ![Estación Web Local](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/web_station_dialog_1080p.png) |

---

### Capacidades Principales

#### 1. Arquitectura de Guardado con Cero Corrupción y Resolución del Error CE-107109-8
- **Sincronización Dual de Carpetas en PS5:** Respalda y restaura simultáneamente `/user/home/<id>/savedata_prospero/<TitleID>` (imágenes planas cifradas `sdimg_*`) y `/user/home/<id>/savedata_prospero_meta/<TitleID>` (base de datos indexada, parámetros e iconos). Elimina la causa raíz del error `CE-107109-8` al permitir que el demonio de guardado de Prospero reconozca e indexe de inmediato las partidas restauradas.
- **Integración del Motor de Apollo Save Tool y Resignado de PS4:** Extrae e inyecta automáticamente el archivo `keystone` auténtico desde `/user/appmeta/<TitleID>/sce_sys/keystone`. Modifica y refirma `param.sfo` (`ACCOUNT_ID`, `PARAMS.user_id` y recálculo del `psid_hmac` de Sony mediante la clave maestra `PSID_HMAC_KEY`) para que partidas de PS4 procedentes de otras consolas o instalaciones previas funcionen de inmediato en la cuenta actual (`0x105f615a`).
- **Propiedad de Usuario y Permisos Abiertos:** Detecta dinámicamente la propiedad de la cuenta del usuario (`/user/home/<user_id>`) y asigna los UID/GID correspondientes junto con permisos abiertos (`0666` en archivos, `0777` en directorios) y sincronización con el kernel, garantizando que el juego lea la partida inmediatamente sin conflictos de permisos.
- **Bóveda Criptográfica de Hardware SAMU:** Archiva el blob sellado por SAMU de 96 bytes, la clave AES PFS en texto plano de 32 bytes desellada en vivo mediante `/dev/pfsmgr` (ioctl `0xc0845302`), el OpenPSID y hashes de integridad SHA-256 en `crypto_vault.bin` junto a cada copia de seguridad para preservación digital y futuro descifrado entre consolas.

#### 2. Estación Web Local Integrada (Descarga por Wi-Fi / LAN)
- **Servidor Web HTTP Local:** Servidor HTTP de alto rendimiento embebido en el puerto 8383 con detección automática de la IP local de la consola PS5. Corre en un hilo de fondo dedicado con 4 MB de pila sin afectar los 60 FPS de la interfaz.
- **Interfaz Web Dark Glassmorphism:** Aplicación web moderna (HTML5 / CSS3 / ES6) integrada directamente en el binario sin enlaces a CDNs externos ni requerir conexión a Internet.
- **Explorador de Partidas en Vivo:** Visualiza tus títulos instalados, carátulas auténticas, Title IDs, plataformas (PS5 / PS4) y peso de datos de guardado desde cualquier navegador en PC o teléfono inteligente.
- **Transmisión de Archivos ZIP al Vuelo:** Generador streaming de archivos ZIP en C++ (IEEE 802.3 CRC32, modo Store) que permite descargar partidas individuales o la librería completa en un archivo `.zip` con barra de progreso real en el navegador gracias al cálculo anticipado de `Content-Length`.
- **Explorador de Archivos Individuales:** Modal interactivo en el navegador para explorar el contenido interno de cada partida y descargar archivos sueltos (`param.json`, `sdimg_*`, `keystone`).
- **Activación Rápida:** Enciende o apaga el servidor con un solo toque desde la cabecera superior o pulsando el botón **Triángulo** en el mando DualSense.

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
- **Triángulo:** Abrir Estación Web Local (Servidor HTTP para descarga por Wi-Fi / LAN).
- **L3 (Click Stick Izquierdo):** Alternar idioma de la interfaz (Inglés / Español).
- **Círculo:** Cancelar diálogo / Retroceder.

---

### Paquetes de Distribución y Sumas de Verificación

| Archivo | Tamaño | Suma de Verificación SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.0.0_PS5.zip` | 29.5 MB (30,975,227 bytes) | `5b006c0a457b1ef8a8e3504e2aaf1cbc173d59cfa92c289cbe535118d4a03a6a` |
| `eboot.bin` | 23.8 MB (24,913,671 bytes) | `504b05e3fd61b30abbb288a754bbba4512d3b1fa7daa1651547627e0b7b95c24` |

Todos los archivos han sido generados y verificados con permisos de modo abierto (`0777`) para ejecución inmediata en consolas PlayStation 5.

---

### Instrucciones de Instalación
1. Descarga el archivo `MacheteSVG_v1.0.0_PS5.zip` desde los activos de la versión.
2. Descomprime el contenido del archivo (carpeta `PPSA99001`).
3. Conéctate a tu PS5 mediante FTP o utiliza un gestor de archivos homebrew.
4. Sube la carpeta `PPSA99001` a `/data/homebrew/` o `/user/app/`.
5. Ejecuta MacheteSVG desde tu cargador de homebrew habitual (Itemzflow, ProsperoStore o menú de etaHEN).
