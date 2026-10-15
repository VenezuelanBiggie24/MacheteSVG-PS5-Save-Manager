# Changelog / Registro de Cambios

All notable changes to the MacheteSVG project will be documented in this file.
This project adheres to Semantic Versioning (`MAJOR.MINOR.PATCH`).

Todos los cambios notables en el proyecto MacheteSVG serán documentados en este archivo.
Este proyecto se adhiere al Versionado Semántico (`MAJOR.MINOR.PATCH`).

---

## [1.0.0] - 2026-10-09

### English

#### Added
- **Native PS5 & PS4 Savegame Management:** Full support for scanning, backing up, and restoring save data for both PlayStation 5 native titles and PlayStation 4 backwards-compatible games.
- **Keystone Cryptographic Preservation:** Protects console-generated `sce_sys/keystone` and `sce_sys/param.sfo` files during restoration, allowing auto-resigning of saves and preventing system error `CE-108255-1`.
- **vnode Cache Invalidation (`sync()` / `fsync()`):** Atomic write operations and cache flushes ensure backwards-compatible PS4 save games reload instantly without requiring console reboots.
- **Embedded Web Save Station:** Local HTTP web server on port 8080 with auto-detected LAN IP, streaming ZIP generation (Store mode, IEEE 802.3 CRC32), dark glassmorphism responsive web UI, and individual file browsing/downloading.
- **Autonomous Sandbox Escape:** Bundled and automated launching of `lapy_jb_daemon.elf` (`127.0.0.1:9021`), acquiring root privileges and filesystem access without requiring external payload injectors.
- **Community Title ID Standard:** Adopted `PPSA99001` to adhere to community homebrew conventions and eliminate retail ID collisions.
- **Hardware-Accelerated Liquid Glass Interface:** OpenGL 4.6 SDF renderer running at fluid 60 FPS with dual-panel layout, spring kinematics, and real-time box art rendering.
- **Internationalization (i18n):** Native bilingual engine defaulting to English, automatic Spanish detection via system language service, and instantaneous language switching via **L3**.
- **Packaging Pipeline:** Comprehensive toolchain with `zip-open-modes.py` enforcing 0777 open-mode permissions for compatibility with PS5 FTP and homebrew installers.

---

### Español

#### Añadido
- **Gestión Nativa de Partidas de PS5 y PS4:** Soporte completo para escaneo, respaldo y restauración de partidas guardadas tanto para títulos nativos de PlayStation 5 como de PS4 en retrocompatibilidad.
- **Preservación Criptográfica de Keystone:** Protección de los archivos de consola `sce_sys/keystone` y `sce_sys/param.sfo` durante la restauración, posibilitando el resignado automático de partidas y previniendo el error `CE-108255-1`.
- **Invalidación de Caché de vnodes (`sync()` / `fsync()`):** Escrituras atómicas y vaciado de caché del sistema para recarga instantánea de partidas de PS4 sin necesidad de reiniciar la consola.
- **Estación Web Local Integrada:** Servidor HTTP local en el puerto 8080 con detección automática de la IP LAN, transmisión de archivos ZIP al vuelo (modo Store, CRC32 IEEE 802.3), interfaz web adaptable con diseño dark glassmorphism y descarga de archivos individuales o biblioteca completa.
- **Escape Autónomo del Sandbox:** Inclusión y lanzamiento automático de `lapy_jb_daemon.elf` (`127.0.0.1:9021`), adquiriendo privilegios de superusuario (root) y acceso al sistema de archivos sin necesidad de inyectores de payloads externos.
- **Estándar de Title ID Comunitario:** Adopción del código `PPSA99001` respetando la convención de homebrew para evitar colisiones con juegos comerciales.
- **Interfaz Acelerada por Hardware Liquid Glass:** Renderizador SDF sobre OpenGL 4.6 a 60 FPS con diseño de panel dividido, física de resortes y carátulas en tiempo real.
- **Motor de Internacionalización (i18n):** Soporte bilingüe con inglés predeterminado, detección automática de español por configuración de consola y cambio instantáneo mediante el botón **L3**.
- **Flujo de Empaquetado:** Herramientas de empaquetado con `zip-open-modes.py` forzando permisos de modo abierto 0777 para compatibilidad total con servidores FTP e instaladores de homebrew de PS5.
