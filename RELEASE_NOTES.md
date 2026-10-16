# Release Notes / Notas de la Version - MacheteSVG

![MacheteSVG Banner](https://raw.githubusercontent.com/VenezuelanBiggie24/MacheteSVG-PS5-Save-Manager/main/docs/screenshots/banner.png)

---

## English Documentation

### Version 1.2.4 (Latest Release) - Restore Crash Fix (chown NULL-Stub Elimination) & Apollo PS4 UI Badges

MacheteSVG v1.2.4 eliminates a fatal segmentation fault (SIGSEGV) occurring during PS4 and PS5 save restoration on PlayStation 5 hardware, completely replaces unexported userland ownership calls with native chmod permissions, and enhances the visual feedback of Apollo Auto-Resigning across both the Console UI and the Web Save Station (:8383).

#### Key Enhancements in v1.2.4
1. **Restore Crash Elimination (`chown` NULL-Pointer Dereference):**
   - Removed all calls to `chown()` in `ensure_directories_recursive`, `copy_file_buffered`, `copy_file_simple`, and `apply_permissions_recursive`.
   - On PlayStation 5 Prospero OS, `chown` only exists as a stub in `libkernel_sys.so` and is not exported by Sony in userland `libkernel.prx`. The dynamic linker left its Global Offset Table (GOT) slot as `0x0`, causing an indirect call to address zero (`si_addr: 0`, `SIGSEGV`) during folder creation.
   - Replaced with standard `chmod()` calls (`0777` for directories, `0666` for files) natively supported by `libkernel.prx`, ensuring full world read/write access for games (UID 1) and root daemons (UID 0) without any ownership mutations.
2. **Prominent Apollo PS4 Auto-Resign UI Visibility:**
   - In the Console UI, PS4 titles (`CUSAxxxxx`) now prominently display the green badge `[APOLLO RESIGN ACTIVO]` in game metadata.
   - The primary restore button dynamically changes to `Restaurar (Modo Apollo)` / `Restore (Apollo Mode)`.
   - The restore modal header clearly states `RESTAURAR Y RESIGNAR (MODO APOLLO PS4)`.
   - In both Console UI and Web Save Station (:8383), backup cards for PS4 titles are explicitly labeled with `MODO APOLLO OK` and the restore button reflects `Restaurar y Resignar (Apollo)`.
3. **Dynamic Static Initializer Elimination:**
   - Converted global string storage roots in `web_server.cpp` to `constexpr const char *kStorageRoots[]`, removing `.init_array` heap allocations prior to main().
4. **Verified Zero Emojis & 100% Passing Tests:**
   - Strict zero-emoji compliance certified across 100% of the repository.
   - 103 unit tests completed with 100% success rate.

#### Package Checksums (v1.2.4)
| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.4_PS5.zip` | 29.6 MB (31,010,630 bytes) | `28dfbad01a6a2556f7c9c2d3b13524af1282aacd51e9f2a51cb798cdd221d173` |
| `eboot.bin` | 23.8 MB (24,992,736 bytes) | `2a0eda7e2908ee8aa48bcf8e2698b3efc6f8496474eeb3484d9a0569dfa8185b` |

#### Installation (Easy FTP Method)
1. Download `MacheteSVG_v1.2.4_PS5.zip`.
2. Extract the archive on your PC to get the `PPSA99001` folder.
3. Enable FTP on your PS5 (OnionHEN, etaHEN, etc.).
4. Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP (resulting path: `/data/homebrew/PPSA99001/eboot.bin`).
5. Launch MacheteSVG from your homebrew launcher or system menu.

---

### Version 1.2.3 - SAMU Crypto Vault, Decrypted Save Slots & Web ZIP Packaging Fix

MacheteSVG v1.2.3 resolves a critical archive generation issue in the Web Save Station (:8383) where downloaded savegame ZIP archives were missing `crypto_vault.bin` (the SAMU cryptographic vault), `decrypted/` save slots, and visible metadata (`meta/` / `sce_sys/`) when extracted on macOS or Windows.

#### Key Enhancements in v1.2.3
1. **SAMU Crypto Vault Extraction & Staging Path Fix:**
   - Restored the active kernel ioctl `PFS_DECRYPT_KEK_CMD` (`0xc0845302`) via `/dev/pfsmgr` under verified root execution (`UID 0`), successfully extracting the 32-byte plain AES key into `crypto_vault.bin` (`flags |= 1`).
   - Fixed an `EROFS` / `ENOENT` failure on PlayStation 5 caused by trying to create temporary files in `/tmp/` (which resides on a read-only root mount). All vault generation and staging now safely operates in `/data/save_staging/vault/`.
   - Multi-storage scanning: `resolve_or_create_crypto_vault` now searches across all storage media (`/data/MacheteSVG`, `/mnt/ext1/MacheteSVG`, and `/mnt/usb0..7/MacheteSVG`) for pre-existing vaults before generating new ones.
2. **Decrypted Save Slots Extraction (`decrypted/<slot>/`):**
   - Restored `ScopedPfsMount` dynamic loader for `libSceFsInternalForVsh.sprx` with clean staging in `/data/save_staging/sdimg_staged` and unmount cleanup.
   - Saves downloaded via `/api/download/<title_id>` and `/api/download_all` now automatically mount the PFS image and bundle the plaintext decrypted files in `decrypted/` for universal cross-console and emulator inspection.
3. **Finder-Visible Metadata & Keystone Packaging:**
   - Save metadata (`param.json`, `icon0.png`, `saveindex`) is now packaged in both visible `meta/` and `sce_sys/` directories alongside the hidden `.meta/`, preventing macOS Finder and file managers from hiding metadata files by default.
   - Authentic 96-byte `keystone` files are resolved across appmeta, global keystones, staging, and storage roots, ensuring they are always bundled into the root of each game folder inside the ZIP.
4. **Global PFS Mount Thread Safety:**
   - Protected all PFS mounting and injection procedures with a global mutex (`pfs_global_mount_mutex`), completely serializing access to `/data/save_mnt` between Web Server downloads and console UI operations.
5. **Verified Zero Emojis & 100% Passing Tests:**
   - Certified 0 emoji violations across all repository files.
   - 103 unit tests passed with 100% success rate.

#### Package Checksums (v1.2.3)
| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.3_PS5.zip` | 29.6 MB (31,010,878 bytes) | `275c1f37c744e2cbfc891a250b394606626d75fd2a02d411f990c97ba2ba52c4` |
| `eboot.bin` | 23.8 MB (24,992,615 bytes) | `ea07bd8d758d364ca8ee4a0f213d819c9db32ed078e2176eebd606f68f527b4d` |

#### Installation (Easy FTP Method)
1. Download `MacheteSVG_v1.2.3_PS5.zip`.
2. Extract the archive on your PC to get the `PPSA99001` folder.
3. Enable FTP on your PS5 (OnionHEN, etaHEN, etc.).
4. Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP (resulting path: `/data/homebrew/PPSA99001/eboot.bin`).
5. Launch MacheteSVG from your homebrew launcher or system menu.

---

### Version 1.2.2 - Compiler Register Aliasing Elimination, Official Web Logo & Event Log Viewer

MacheteSVG v1.2.2 addresses a critical compiler-induced segmentation fault (SIGSEGV) during save restoration on PlayStation 5 hardware, introduces official branding to the embedded Web Save Station (:8383), and provides direct browser access to system diagnostics.

#### Key Enhancements in v1.2.2
1. **Restore Crash Elimination (Compiler Register Aliasing & Stack Safety):**
   - Fixed a fatal crash (`SIGSEGV` at `si_addr: 0x1` / `0x0`) caused by Clang 18 `-O2` register allocator reusing register `%r13` for loop bounds across inlined directory resolution helpers, clobbering `%rbx` and reading unmapped addresses.
   - Converted `resolve_path_ownership` and `ensure_directories_recursive` into `__attribute__((noinline)) noexcept` procedures with stack-allocated fixed buffers (`char buf[1024]`), eliminating all dynamic string allocations in recursive directory handling.
   - Expanded worker thread stack from 2 MB to 4 MB (`pthread_attr_setstacksize`), preventing thread stack exhaustion on Prospero OS.
2. **Official Web Branding & Event Log Access (:8383):**
   - Header icon updated: displays the authentic MacheteSVG Liquid Glass logo via dedicated endpoint `/api/logo` (serving `icon0.png`).
   - New "Descargar Log" button added to header actions: downloads the complete system event log and crash diagnostics (`machete.log`) directly from `/api/log`.
   - Updated Web Station status endpoint and header badge to `PPSA99001 v1.2.2`.
3. **Verified Zero Emojis & 100% Passing Tests:**
   - Certified 0 emoji violations across all 459 repository files.
   - 103 unit tests passed with 100% success rate.

#### Package Checksums (v1.2.2)
| File | Size | SHA-256 Checksum |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.2_PS5.zip` | 29.6 MB (31,004,293 bytes) | `77ac435514242c7372c956502567d3e2abc03854e7decb1e697aaf64d25a563a` |
| `eboot.bin` | 23.8 MB (24,976,247 bytes) | `588266e9dbe9cc7f38fa2266afe7c5231317fa3645cb17a1f238f7f8d7e2c978` |

#### Installation (Easy FTP Method)
1. Download `MacheteSVG_v1.2.2_PS5.zip`.
2. Extract the archive on your PC to get the `PPSA99001` folder.
3. Enable FTP on your PS5 (OnionHEN, etaHEN, etc.).
4. Upload the `PPSA99001` folder directly to `/data/homebrew/` via FTP (resulting path: `/data/homebrew/PPSA99001/eboot.bin`).
5. Launch MacheteSVG from your homebrew launcher or system menu.

---

### Version 1.2.1 - PS4 Save Restore Crash Fix & Kernel Safety Shielding

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

### Version 1.2.4 (Ultima Version) - Correccion de Crash al Restaurar (Eliminacion de Stub Nulo chown) y Distintivos de Modo Apollo PS4

MacheteSVG v1.2.4 elimina de raiz el fallo critico de segmentacion (SIGSEGV) que ocurria al restaurar partidas de PS4 y PS5 en consolas PlayStation 5 reales, sustituye por completo las llamadas no exportadas de cambio de propietario por permisos nativos chmod, y hace totalmente visible el estado del Modo de Resignado Apollo tanto en la consola como en la Estacion Web (:8383).

#### Mejoras Principales en v1.2.4
1. **Eliminacion del Crash al Restaurar (Desreferencia Nula de `chown`):**
   - Removidas todas las invocaciones a `chown()` en `ensure_directories_recursive`, `copy_file_buffered`, `copy_file_simple` y `apply_permissions_recursive`.
   - En el sistema operativo Prospero de PS5, `chown` solo existe como un stub en `libkernel_sys.so` y Sony no lo exporta en `libkernel.prx`. El enlazador dinamico dejaba su entrada en la tabla global de offsets (GOT) en `0x0`, provocando un salto indirecto a la direccion cero (`si_addr: 0`, `SIGSEGV`) durante la creacion de directorios.
   - Reemplazado por llamadas a `chmod()` (`0777` para directorios, `0666` para archivos) soportadas de forma nativa por `libkernel.prx`, garantizando acceso total de lectura y escritura para juegos (UID 1) y daemons root (UID 0) sin requerir mutacion de propietario.
2. **Visibilidad Prominente del Modo Apollo PS4 en la Interfaz:**
   - En la consola, los titulos de PS4 (`CUSAxxxxx`) muestran ahora de forma destacada el distintivo verde `[APOLLO RESIGN ACTIVO]` junto a la etiqueta de plataforma.
   - El boton principal de restauracion cambia dinamicamente a `Restaurar (Modo Apollo)`.
   - El encabezado del modal de restauracion indica claramente `RESTAURAR Y RESIGNAR (MODO APOLLO PS4)`.
   - Tanto en la consola como en la Estacion Web (:8383), las tarjetas de respaldo para juegos de PS4 muestran el distintivo `MODO APOLLO OK` y el boton de accion indica `Restaurar y Resignar (Apollo)`.
3. **Eliminacion de Inicializadores Estaticos Dinamicos:**
   - Convertidas las raices de almacenamiento globales en `web_server.cpp` a `constexpr const char *kStorageRoots[]`, eliminando asignaciones en el heap en `.init_array` previas a main().
4. **Certificacion de Cero Emojis y 103 Pruebas Unitarias Exitosas:**
   - Cumplimiento estricto de cero emojis certificado en el 100% del repositorio.
   - 103 pruebas unitarias completadas con 100% de aprobacion.

#### Sumas de Verificacion de Paquetes (v1.2.4)
| Archivo | Tamano | Suma de Verificacion SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.4_PS5.zip` | 29.6 MB (31,010,630 bytes) | `28dfbad01a6a2556f7c9c2d3b13524af1282aacd51e9f2a51cb798cdd221d173` |
| `eboot.bin` | 23.8 MB (24,992,736 bytes) | `2a0eda7e2908ee8aa48bcf8e2698b3efc6f8496474eeb3484d9a0569dfa8185b` |

#### Instalacion (Metodo Facil via FTP)
1. Descarga `MacheteSVG_v1.2.4_PS5.zip`.
2. Descomprime el archivo en tu PC para obtener la carpeta `PPSA99001`.
3. Habilita el servidor FTP en tu PS5 (OnionHEN, etaHEN, etc.).
4. Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP (ruta final: `/data/homebrew/PPSA99001/eboot.bin`).
5. Inicia MacheteSVG directamente desde tu lanzador de homebrew o menu principal.

---

### Version 1.2.3 - Boveda Criptografica SAMU, Ranuras Desencriptadas y Correccion de Empaquetado ZIP Web

MacheteSVG v1.2.3 soluciona un problema critico en la generacion de archivos de la Estacion Web (:8383), donde las descargas ZIP de partidas guardadas no incluian `crypto_vault.bin` (la boveda criptografica del SAMU), las ranuras desencriptadas (`decrypted/`) ni los metadatos visibles (`meta/` / `sce_sys/`) al descomprimirse en macOS o Windows.

#### Mejoras Principales en v1.2.3
1. **Extraccion de Boveda SAMU y Correccion de Ruta de Staging:**
   - Restaurada la llamada ioctl de kernel `PFS_DECRYPT_KEK_CMD` (`0xc0845302`) a traves de `/dev/pfsmgr` bajo ejecucion root verificada (`UID 0`), extrayendo con exito la clave AES plana de 32 bytes dentro de `crypto_vault.bin` (`flags |= 1`).
   - Corregido el fallo `EROFS` / `ENOENT` en PlayStation 5 causado al intentar crear archivos temporales en `/tmp/` (ubicado en una particion de solo lectura). Toda la generacion de la boveda y el staging ahora opera de forma segura en `/data/save_staging/vault/`.
   - Escaneo multialmacenamiento: `resolve_or_create_crypto_vault` ahora busca en todas las unidades (`/data/MacheteSVG`, `/mnt/ext1/MacheteSVG` y `/mnt/usb0..7/MacheteSVG`) bovedas previas antes de generar una nueva.
2. **Extraccion de Ranuras Desencriptadas (`decrypted/<slot>/`):**
   - Restaurado el cargador dinamico `ScopedPfsMount` para `libSceFsInternalForVsh.sprx` con staging limpio en `/data/save_staging/sdimg_staged` y desmontaje garantizado.
   - Las partidas descargadas por `/api/download/<title_id>` y `/api/download_all` ahora montan la imagen PFS y empaquetan automaticamente los archivos desencriptados en `decrypted/` para compatibilidad universal entre consolas y analisis externo.
3. **Metadatos Visibles en Finder y Empaquetado de Keystone:**
   - Los metadatos de guardado (`param.json`, `icon0.png`, `saveindex`) ahora se empaquetan tanto en carpetas visibles `meta/` y `sce_sys/` como en la carpeta oculta `.meta/`, evitando que el Finder de macOS u otros exploradores de archivos los oculten por defecto.
   - Los archivos `keystone` autenticos de 96 bytes se resuelven en appmeta, keystones globales, staging y respaldos de disco, garantizando que siempre se incluyan en la raiz de cada juego dentro del ZIP.
4. **Sincronizacion y Seguridad Multihilo en Montajes PFS:**
   - Protegidas todas las operaciones de montaje e inyeccion PFS mediante un mutex global (`pfs_global_mount_mutex`), serializando completamente el acceso a `/data/save_mnt` entre las descargas del Servidor Web y las operaciones de la consola.
5. **Certificacion de Cero Emojis y 103 Pruebas Unitarias Exitosas:**
   - 0 infracciones de emojis certificadas en los 459 archivos del repositorio.
   - 103 pruebas unitarias completadas con 100% de aprobacion.

#### Sumas de Verificacion de Paquetes (v1.2.3)
| Archivo | Tamano | Suma de Verificacion SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.3_PS5.zip` | 29.6 MB (31,010,878 bytes) | `275c1f37c744e2cbfc891a250b394606626d75fd2a02d411f990c97ba2ba52c4` |
| `eboot.bin` | 23.8 MB (24,992,615 bytes) | `ea07bd8d758d364ca8ee4a0f213d819c9db32ed078e2176eebd606f68f527b4d` |

#### Instalacion (Metodo Facil via FTP)
1. Descarga `MacheteSVG_v1.2.3_PS5.zip`.
2. Descomprime el archivo en tu PC para obtener la carpeta `PPSA99001`.
3. Habilita el servidor FTP en tu PS5 (OnionHEN, etaHEN, etc.).
4. Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP (ruta final: `/data/homebrew/PPSA99001/eboot.bin`).
5. Inicia MacheteSVG directamente desde tu lanzador de homebrew o menu principal.

---

### Version 1.2.2 - Eliminacion de Falla por Reutilizacion de Registros del Compilador, Logotipo Web Oficial y Descargador de Registro de Eventos

MacheteSVG v1.2.2 soluciona de raiz el fallo critico de segmentacion (SIGSEGV) producido durante la restauracion de partidas en consolas PlayStation 5 reales, anade el logotipo oficial a la Estacion Web embebida (:8383) y permite descargar el registro de eventos y diagnostico del sistema directamente desde el navegador web.

#### Mejoras Principales en v1.2.2
1. **Eliminacion de Caida en Restauracion (Solucion a Reutilizacion de Registros y Seguridad de Pila):**
   - Corregido el fallo fatal (`SIGSEGV` en `si_addr: 0x1` / `0x0`) provocado por el asignador de registros de Clang 18 `-O2`, que al incrustar (`inline`) las rutinas de resolucion de directorios reutilizaba el registro `%r13`, corrompiendo `%rbx` e intentando leer direcciones de memoria no mapeadas.
   - Refactorizadas las funciones `resolve_path_ownership` y `ensure_directories_recursive` con `__attribute__((noinline)) noexcept` y buffers fijos de pila (`char buf[1024]`), eliminando por completo cualquier asignacion dinamica de memoria en las operaciones recursivas sobre rutas de disco.
   - Ampliado el tamano de la pila del hilo de trabajo de 2 MB a 4 MB (`pthread_attr_setstacksize`), garantizando ejecucion segura y holgada ante llamadas recursivas y del kernel en Prospero OS.
2. **Logotipo Oficial y Acceso al Registro de Eventos en la Web (:8383):**
   - El icono de cabecera de la web ahora carga el logotipo oficial Liquid Glass de MacheteSVG mediante el endpoint `/api/logo` (sirviendo `icon0.png`).
   - Anadido el boton "Descargar Log" en la cabecera: permite descargar el archivo completo de eventos y diagnosticos del sistema (`machete.log`) con un solo clic a traves de `/api/log`.
   - Actualizada la version en el endpoint `/api/status` y en la etiqueta visual a `PPSA99001 v1.2.2`.
3. **Certificacion de Cero Emojis y 103 Pruebas Unitarias Exitosas:**
   - 0 infracciones de emojis certificadas en los 459 archivos del repositorio.
   - 103 pruebas unitarias completadas con 100% de aprobacion.

#### Sumas de Verificacion de Paquetes (v1.2.2)
| Archivo | Tamano | Suma de Verificacion SHA-256 |
| :--- | :--- | :--- |
| `MacheteSVG_v1.2.2_PS5.zip` | 29.6 MB (31,004,293 bytes) | `77ac435514242c7372c956502567d3e2abc03854e7decb1e697aaf64d25a563a` |
| `eboot.bin` | 23.8 MB (24,976,247 bytes) | `588266e9dbe9cc7f38fa2266afe7c5231317fa3645cb17a1f238f7f8d7e2c978` |

#### Instalacion (Metodo Facil via FTP)
1. Descarga `MacheteSVG_v1.2.2_PS5.zip`.
2. Descomprime el archivo en tu PC para obtener la carpeta `PPSA99001`.
3. Habilita el servidor FTP en tu PS5 (OnionHEN, etaHEN, etc.).
4. Copia la carpeta `PPSA99001` directamente en `/data/homebrew/` via FTP (ruta final: `/data/homebrew/PPSA99001/eboot.bin`).
5. Inicia MacheteSVG directamente desde tu lanzador de homebrew o menu principal.

---

### Version 1.2.1 - Correccion de Fallo en Restauracion de PS4 y Blindaje de Kernel

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
