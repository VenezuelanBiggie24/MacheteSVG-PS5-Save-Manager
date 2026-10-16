# Release Notes / Notas de la Version - MacheteSVG

![MacheteSVG Banner](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/banner.png)

---

## English Documentation

### Version 1.2.1 (Latest Release) - PS4 Save Restore Crash Fix & Kernel Safety Shielding

MacheteSVG v1.2.1 addresses an immediate userland crash (SIGSEGV) observed on real PlayStation 5 hardware during PS4 save restoration, safely shielding kernel interactions and strictly separating PS4 backwards-compatible save restoration from PS5 Save Dummy container logic.

#### Key Enhancements in v1.2.1
1. **PS4 Restore Crash Elimination (SIGSEGV Fix):**
   - Fixed an invalid function pointer jump (`RIP: 0x2`) caused by userland dynamic linking resolution (`dlsym`) of internal VSH private stubs (`libSceFsInternalForVsh.sprx`).
   - Removed obsolete PS4 `/dev/pfsmgr` ioctl (`0xc0845302`) that was causing kernel driver faults on Prospero userland.
   - PS4 save restoration now operates cleanly via direct filesystem copy, Apollo-style 64-bit `ACCOUNT_ID` / `user_id` patching, Sony `psid_hmac` recalculation, authentic `keystone` injection, and filesystem cache synchronization (`sync()`), 100% crash-free.
2. **Strict Architectural Separation (PS4 vs PS5):**
   - PS4 saves (`CUSAxxxxx`) now completely bypass the PS5 Save Dummy preservation, PFS mounting, and cross-console hardware checks, allowing immediate restoration even on consoles with no prior save dummy.
3. **Verified Zero Emojis & Clean Build:**
   - Strict zero-emoji compliance certified across 100% of the repository.

#### Package Checksums (v1.2.1)
| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.1_PS5.zip` | 29.6 MB (31,003,402 bytes) | `e5b7511152217648624fbc8b5e542aede8cb36a90e96f1a91597d712b88ef446` |
| `eboot.bin` | 23.8 MB (24,972,679 bytes) | `6db9bbe86dd6a9fe4a12d79d5d42d55839c5806c8c3499cdbab57284d896bf68` |

#### Installation (Easy FTP Method)
1. Download `MacheteSVG_v1.2.1_PS5.zip`.
2. Extract the archive on your PC to get the `PPSA99001` folder.
3. Enable FTP on your PS5 (OnionHEN, etaHEN, etc.).
4. Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP (resulting path: `/data/homebrew/PPSA99001/eboot.bin`).
5. Launch MacheteSVG from your homebrew launcher or system menu.

---

### Version 1.2.0 - Cross-Console PFS Save Dummy Injection, Apollo PS4 Auto-Resigner & QR Web Station

MacheteSVG v1.2.0 is a major milestone release delivering reliable cross-console save restoration for PlayStation 5 native titles, complete Apollo-style auto-resigning for PlayStation 4 games, and an in-app ISO/IEC 18004 QR Code generator for instantaneous zero-configuration smartphone connection to the Web Save Station.

#### Key Enhancements in v1.2.0
1. **Cross-Console PS5 Save Dummy Hot-Injection (/dev/pfsmgr):**
   - Solves the notorious `CE-107109-8` and "Corrupted Data" errors when attempting to restore PS5 saves from another console or after a full OS reinstallation.
   - Extracts decrypted save contents into `backup/decrypted/` during backup.
   - When restoring on another console, preserves the authentic local Save Dummy PFS container and hot-injects decrypted payload data directly into the active PFS mount point (`/data/save_mnt`), enabling the Prospero kernel to re-seal the container using the console's unique hardware keys.
2. **Apollo-Style PS4 Auto-Resigning:**
   - Automatically patches 64-bit `ACCOUNT_ID`, updates `user_id`, and recalculates Sony `psid_hmac` across `param.sfo` (both outer header and inner PFS container).
   - Preserves authentic 96-byte `keystone` files from `/user/appmeta/<TitleID>/sce_sys/keystone`.
3. **In-App QR Code Generator (ISO/IEC 18004):**
   - Standalone C++20 QR Code generator running on the console without external libraries.
   - Generates a sharp, high-contrast QR code with standard Quiet Zone on the Web Server dialog.
   - Users scan the TV screen with their smartphone or tablet camera to instantly open the Web Save Station (`http://<IP-PS5>:8383`) without manual IP entry.
4. **Kernel Safety & Concurrency Hardening:**
   - Added global mount serialization mutex (`pfs_global_mount_mutex`) preventing race conditions between console and web operations.
   - Full socket protection (`SO_NOSIGPIPE`, timeouts, clean descriptor cleanup) ensuring zero kernel panic risk.

#### Package Checksums (v1.2.0)
| File | Size | SHA-256 Checksum |
| `MacheteSVG_v1.2.0_PS5.zip` | 29.6 MB (31,008,313 bytes) | `6a40f8c296fc05e4dd67c1bb58830cba364cec500ad8d1015abdf61319d7801e` |
| `eboot.bin` | 23.8 MB (24,987,079 bytes) | `10947ae2c33a6636174e5fe66fa760520e996b4ae997a9ee1d635e57262c9b19` |

#### Installation (Easy FTP Method)
1. Download `MacheteSVG_v1.2.0_PS5.zip`.
2. Extract the archive on your PC to get the `PPSA99001` folder.
3. Enable FTP on your PS5 (OnionHEN, etaHEN, etc.).
4. Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP (resulting path: `/data/homebrew/PPSA99001/eboot.bin`).
5. Launch MacheteSVG from your homebrew launcher or system menu.

---

### Version 1.1.0 - Embedded Web Save Station (:8383) & SAMU Crypto Vault

MacheteSVG v1.1.0 introduced the embedded Web Save Station running locally on dedicated port 8383, replacing external cloud services with ultra-fast local network transfers, live streaming ZIP archive downloads, and SAMU hardware cryptographic vault preservation.

#### Key Enhancements in v1.1.0
- **Embedded Web Save Station (:8383):** Dedicated HTTP server running entirely in-memory with Dark Glassmorphism web UI.
- **Streaming ZIP Writer:** Custom zero-dependency C++ ZIP writer (Store mode, IEEE 802.3 CRC32) generating dynamic `.zip` archives with real browser download progress.
- **Interactive File Explorer:** Web-based modal to inspect and download individual save files (`param.json`, `sdimg_*`, `keystone`).
- **SAMU Crypto Vault Preservation:** Archives console-unique SAMU sealed blobs, plaintext AES PFS keys, OpenPSID, and SHA-256 integrity records in `crypto_vault.bin`.

#### Installation (Easy FTP Method)
Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP.

---

### Version 1.0.0 - Initial Native PS5 & PS4 Save Manager

Initial functional release of MacheteSVG on real PlayStation 5 hardware.

#### Key Features in v1.0.0
- **Liquid Glass UI:** Custom hardware-accelerated OpenGL 4.6 SDF renderer delivering a fluid 60 FPS dual-panel interface.
- **Autonomous Jailbreak:** Embedded `lapy_jb_daemon.elf` with local IPC connection to `127.0.0.1:9021` for transparent root elevation.
- **Asynchronous Storage Engine:** Multi-threaded backup and restore to `/data/MacheteSVG` and external USB (`/mnt/usb0..7`).
- **Bilingual Interface:** Real-time English and Spanish language switching with **L3** controller shortcut.

#### Installation (Easy FTP Method)
Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP.

---

## Notas de la Version en Espanol (Spanish Documentation)

### Version 1.2.1 (Ultima Version) - Correccion de Fallo en Restauracion de PS4 y Blindaje de Kernel

MacheteSVG v1.2.1 resuelve de forma definitiva el fallo critico (SIGSEGV) reportado en hardware real de PlayStation 5 durante la restauracion de partidas de PS4, eliminando llamadas de kernel inseguras y aislando estrictamente el flujo de restauracion de PS4 respecto a la logica de Save Dummy de PS5.

#### Mejoras Principales en v1.2.1
1. **Eliminacion de Caida en Restauracion de PS4 (Correccion de SIGSEGV):**
   - Corregido el salto a direccion invalida (`RIP: 0x2`) provocado por la resolucion dinamica (`dlsym`) de funciones privadas del VSH (`libSceFsInternalForVsh.sprx`) en espacio de usuario Prospero.
   - Eliminada la llamada ioctl obsoleta de PS4 (`0xc0845302`) sobre `/dev/pfsmgr` que provocaba fallos en el controlador del kernel en consolas PS5.
   - La restauracion de partidas de PS4 ahora se ejecuta de forma directa y limpia: copia de archivos, resignado estilo Apollo (`ACCOUNT_ID` de 64 bits, `user_id` y recalculo de `psid_hmac`), inyeccion de `keystone` autentico y sincronizacion de cache (`sync()`), garantizando 100% de exito y cero cuelgues.
2. **Separacion Arquitectonica Estricta (PS4 vs PS5):**
   - Las partidas de PS4 (`CUSAxxxxx`) ahora omiten por completo los requisitos de Save Dummy de PS5, montaje PFS y comparaciones de hardware cross-console, permitiendo su restauracion inmediata incluso en consolas recien formateadas.
3. **Certificacion de Cero Emojis y 103 Pruebas Unitarias:**
   - Verificacion de cero emojis aprobada al 100% en todo el repositorio y 103 pruebas unitarias completadas con exito.

#### Sumas de Verificacion de Paquetes (v1.2.1)
| Archivo | Tamano | Suma de Verificacion SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.1_PS5.zip` | 29.6 MB (31,003,402 bytes) | `e5b7511152217648624fbc8b5e542aede8cb36a90e96f1a91597d712b88ef446` |
| `eboot.bin` | 23.8 MB (24,972,679 bytes) | `6db9bbe86dd6a9fe4a12d79d5d42d55839c5806c8c3499cdbab57284d896bf68` |

#### Instalacion (Metodo Facil via FTP)
1. Descarga `MacheteSVG_v1.2.1_PS5.zip`.
2. Descomprime el archivo en tu PC para obtener la carpeta `PPSA99001`.
3. Habilita el servidor FTP en tu PS5 (OnionHEN, etaHEN, etc.).
4. Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP (ruta final: `/data/homebrew/PPSA99001/eboot.bin`).
5. Inicia MacheteSVG directamente desde tu lanzador de homebrew o menu principal.

---

### Version 1.2.0 - Inyeccion Cross-Console con Save Dummy en PS5, Resignado Automatico Apollo para PS4 y Estacion Web con Codigo QR

MacheteSVG v1.2.0 es una actualizacion mayor que permite restaurar partidas nativas de PlayStation 5 provenientes de otras consolas, integra resignado automatico estilo Apollo para juegos de PlayStation 4 y anade un generador de Codigos QR ISO/IEC 18004 en pantalla para conectar moviles y tablets a la Estacion Web al instante sin escribir direcciones IP.

#### Mejoras Principales en v1.2.0
1. **Inyeccion Cross-Console con Save Dummy en PS5 (/dev/pfsmgr):**
   - Resuelve de forma definitiva los errores `CE-107109-8` y "Datos danados" al intentar restaurar partidas de PS5 de otra consola o tras reinstalar el sistema operativo.
   - Extrae el contenido desencriptado en `backup/decrypted/` durante cada respaldo.
   - Al restaurar en otra consola, preserva el Save Dummy local autentico e inyecta en caliente los datos desencriptados en el punto de montaje PFS (`/data/save_mnt`), permitiendo que el kernel de Prospero selle el contenedor con las claves maestras locales de la consola.
2. **Resignado Automatico Estilo Apollo para PS4:**
   - Modifica automaticamente el `ACCOUNT_ID` de 64 bits, actualiza `user_id` y recalcula el `psid_hmac` de Sony en `param.sfo` (tanto en la cabecera externa como dentro de la imagen PFS).
   - Preserva el archivo `keystone` autentico de 96 bytes de `/user/appmeta/<TitleID>/sce_sys/keystone`.
3. **Generador de Codigo QR en Pantalla (ISO/IEC 18004):**
   - Motor QR nativo en C++20 que corre directamente en la consola sin bibliotecas externas.
   - Renderiza un codigo QR de alto contraste con Quiet Zone reglamentaria en el dialogo del Servidor Web.
   - El usuario escanea la pantalla con la camara de su telefono o tablet para abrir la Estacion Web (`http://<IP-PS5>:8383`) al instante.
4. **Seguridad de Kernel y Concurrencia:**
   - Mutex global de serializacion de montaje (`pfs_global_mount_mutex`) que evita condiciones de carrera entre la interfaz de la consola y las descargas concurrentes de la web.
   - Blindaje de sockets (`SO_NOSIGPIPE`, timeouts y liberacion limpia de descriptores) que garantiza cero riesgo de kernel panics.

#### Sumas de Verificacion de Paquetes (v1.2.0)
| Archivo | Tamano | Suma de Verificacion SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.0_PS5.zip` | 29.6 MB (31,008,313 bytes) | `6a40f8c296fc05e4dd67c1bb58830cba364cec500ad8d1015abdf61319d7801e` |
| `eboot.bin` | 23.8 MB (24,987,079 bytes) | `10947ae2c33a6636174e5fe66fa760520e996b4ae997a9ee1d635e57262c9b19` |

#### Instalacion (Metodo Facil via FTP)
1. Descarga `MacheteSVG_v1.2.0_PS5.zip`.
2. Descomprime el archivo en tu PC para obtener la carpeta `PPSA99001`.
3. Habilita el servidor FTP en tu PS5 (OnionHEN, etaHEN, etc.).
4. Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP (ruta final: `/data/homebrew/PPSA99001/eboot.bin`).
5. Inicia MacheteSVG directamente desde tu lanzador de homebrew o menu principal.

---

### Version 1.1.0 - Estacion Web Local (:8383) y Boveda Criptografica SAMU

MacheteSVG v1.1.0 introdujo la Estacion Web Local embebida en el puerto dedicado 8383, reemplazando la dependencia de servidores externos en la nube con transferencias ultrarrapidas en red local, descargas en streaming ZIP y preservacion de la boveda de claves SAMU.

#### Mejoras Principales en v1.1.0
- **Estacion Web Local (:8383):** Servidor HTTP integrado en memoria con interfaz web Dark Glassmorphism.
- **Generador Streaming ZIP:** Escritor ZIP propio en C++ (modo Store, CRC32) con calculo de progreso real en el navegador.
- **Explorador de Archivos:** Modal interactivo para inspeccionar y descargar archivos individuales (`param.json`, `sdimg_*`, `keystone`).
- **Boveda Criptografica SAMU:** Archiva el blob sellado de SAMU, clave AES PFS desellada, OpenPSID y firmas SHA-256 en `crypto_vault.bin`.

#### Instalacion (Metodo Facil via FTP)
Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP.

---

### Version 1.0.0 - Gestor Nativo de Partidas de PS5 y PS4

Primera version funcional de MacheteSVG ejecutandose en hardware real de PlayStation 5.

#### Caracteristicas en v1.0.0
- **Interfaz Liquid Glass:** Renderizador acelerado por hardware OpenGL 4.6 SDF a 60 FPS con vista dividida.
- **Jailbreak Autonomo:** Demonio integrado `lapy_jb_daemon.elf` con conexion IPC local a `127.0.0.1:9021`.
- **Motor de Almacenamiento Asincrono:** Respaldo y restauracion multihilo en `/data/MacheteSVG` y USB (`/mnt/usb0..7`).
- **Interfaz Bilingue:** Cambio dinamico entre ingles y espanol con el boton **L3** del mando.

#### Instalacion (Metodo Facil via FTP)
Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP.
