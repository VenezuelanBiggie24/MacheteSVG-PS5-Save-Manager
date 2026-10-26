#!/usr/bin/env bash
# MacheteSVG - Packaging and Distribution Script for Jailbroken PS5
# Copyright (C) 2026 MacheteSVG Team
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
title_id="PPSA99700"
app_name="MacheteSaveGameManager"
version="1.3.5"

echo "=================================================="
echo "   Empaquetador de Distribución de MacheteSaveGameManager     "
echo "=================================================="

# 1. Validar recursos
echo "==> [1/4] Validando activos de presentación..."
bash "$root/tools/validate-assets.sh"

echo "==> [2/4] Validando pistas de audio..."
python3 "$root/tools/audio-check.py"

# 2. Compilar aplicación nativa (eboot.bin + sce_module)
echo "==> [3/5] Compilando binario nativo PS5 (eboot.bin)..."
PATH="/opt/homebrew/bin:/opt/homebrew/opt/coreutils/libexec/gnubin:/opt/homebrew/opt/llvm/bin:$PATH" \
PS5_CLANG=/opt/homebrew/opt/llvm/bin/clang \
HOST_CXX=/opt/homebrew/opt/llvm/bin/clang++ \
CXX=/opt/homebrew/opt/llvm/bin/clang++ \
make -C "$root" app

dist_dir="$root/dist/$title_id"
[[ -f "$dist_dir/eboot.bin" ]] || {
    echo "Error crítico: eboot.bin no fue generado en $dist_dir/eboot.bin" >&2
    exit 1
}

echo "==> [4/5] Ensamblando recursos adicionales y guías técnicas..."
cp "$root/sce_sys/icon0.png" "$dist_dir/sce_sys/"
cp "$root/sce_sys/param.json" "$dist_dir/sce_sys/"
if [[ -f "$root/sce_sys/snd0.at9" ]]; then
    cp "$root/sce_sys/snd0.at9" "$dist_dir/sce_sys/"
fi

# Copiar Lapy JB Daemon integrado dentro de la estructura de la app (daemon/lapy_jb_daemon.elf)
mkdir -p "$dist_dir/daemon"
if [[ -d "$root/payloads/lapy_jb" ]]; then
    cp -r "$root/payloads/lapy_jb/"* "$dist_dir/daemon/"
    chmod 0777 "$dist_dir/daemon/lapy_jb_daemon.elf" 2>/dev/null || true
fi
rm -f "$dist_dir/lapy.elf"
chmod -R 0777 "$dist_dir"

# Eliminar archivos obsoletos si existían
rm -f "$dist_dir/INSTRUCCIONES_PS5.txt"
rm -rf "$root/dist/lapy_jb" "$root/dist/PPSA00001"

# 3. Crear guía de instalación bilingüe (Nombre en inglés: INSTRUCTIONS.txt, Inglés primero, Español después)
cat << 'EOF' > "$dist_dir/INSTRUCTIONS.txt"
=====================================================
   MacheteSaveGameManager v1.3.5 - PS5 & PS4 Savegame Manager
=====================================================

[ENGLISH GUIDE]
SYSTEM REQUIREMENTS & COMPATIBILITY:
- Console: PlayStation 5 (All Jailbroken Firmwares)
- Environments: OnionHEN, etaHEN, or any PS5 jailbreak environment
- Mounting engine: Native Prospero libSceSaveData / /dev/pfsmgr PFS mounting
- Save Formats: PlayStation 5 native titles and PlayStation 4 backwards-compatible (BC) games

AUTONOMOUS & FULLY SELF-CONTAINED:
- MacheteSaveGameManager contains everything needed to operate out of the box.
- No external daemons or payload senders are required.
- If a resident daemon is running, MacheteSaveGameManager detects and uses it.
- If no daemon is running, MacheteSaveGameManager automatically launches its embedded daemon
  via the local loader on 127.0.0.1:9021 without requiring user intervention.

KEY FEATURES:
1. Cross-Console PS5 Save Dummy Injection:
   - Uses /dev/pfsmgr to extract decrypted save files into backup/decrypted.
   - When restoring saves from another console, preserves the local Save Dummy
     container and safely hot-injects decrypted payload into the PFS mount,
     guaranteeing zero data corruption across fresh installs or console switches.
2. Apollo-Style PS4 Auto-Resigning & Database Indexing:
   - 100% automatic Account ID patching (64-bit), user_id update, and Sony
     psid_hmac recalculation on param.sfo inside PFS containers.
   - Direct system savedata.db registration and corruption flag repair
     (is_broken=0, fake_broken=0) to permanently eliminate "Corrupted Data" errors.
   - Keystone preservation ensures authentic game identity across accounts.
3. Web Save Station (:8383) with In-App QR Code:
   - Built-in HTTP web server listening on port 8383.
   - Press Triangle to display the server URL and a scan-ready QR code to
     instantly connect from your mobile device camera without typing.

PACKAGE CONTENTS:
- PPSA99700/eboot.bin             (Signed native application executable)
- PPSA99700/sce_module/libc.prx   (Prospero runtime compatibility layer)
- PPSA99700/sce_sys/param.json    (Title ID: PPSA99700 package metadata)
- PPSA99700/sce_sys/icon0.png     (Official Liquid Glass Memory Card icon)
- PPSA99700/assets/               (SDF fonts, UI graphics, soundbank)
- PPSA99700/daemon/               (Embedded privilege escalation daemon)
- PPSA99700/INSTRUCTIONS.txt      (Installation and operational guide)
- PPSA99700/LEGAL.txt             (License, warranty, and trademark notices)

INSTALLATION INSTRUCTIONS (EASY FTP METHOD):
1. Ensure your PS5 and computer/phone are connected to the same local network.
2. Enable the FTP server on your PS5 (via OnionHEN / etaHEN settings or payload).
3. Connect using any FTP client (FileZilla, WinSCP, Cyberduck, etc.).
4. Copy the entire "PPSA99700" folder directly into:
     /data/homebrew/
   The resulting path must be:
     /data/homebrew/PPSA99700/eboot.bin
5. Launch MacheteSaveGameManager directly from your homebrew launcher or system menu!

CONTROLLER SHORTCUTS:
- D-Pad: Navigate game list and menus
- Cross (X): Select / Confirm action
- Circle (O): Cancel / Back
- Triangle: Open Web Save Station (LAN / Wi-Fi local HTTP server with QR Code)
- Square: Refresh games list and connected USB devices
- L3 (Left Stick Click): Switch language (English / Spanish) in real-time

-----------------------------------------------------

[GUIA EN ESPAÑOL]
REQUISITOS DEL SISTEMA Y COMPATIBILIDAD:
- Consola: PlayStation 5 (Todos los Firmwares con Jailbreak)
- Entornos: OnionHEN, etaHEN o cualquier entorno jailbreak de PS5
- Motor de montaje: libSceSaveData nativo de Prospero / montaje PFS con /dev/pfsmgr
- Formatos: Títulos nativos de PlayStation 5 y juegos de PS4 en retrocompatibilidad (BC)

TOTALMENTE AUTONOMO Y AUTOCONTENIDO:
- MacheteSaveGameManager incluye todo lo necesario para funcionar de inmediato.
- No se requieren demonios externos ni aplicaciones de inyección de payloads.
- Si un demonio residente está activo, MacheteSaveGameManager lo detecta y lo utiliza.
- Si no hay ningún demonio en ejecución, MacheteSaveGameManager auto-lanza su demonio integrado
  mediante el cargador local en 127.0.0.1:9021 de forma transparente.

CARACTERISTICAS PRINCIPALES:
1. Inyeccion Cross-Console de Partidas PS5 con Save Dummy:
   - Utiliza /dev/pfsmgr para extraer los datos desencriptados en backup/decrypted.
   - Al restaurar partidas de otra consola, preserva el Save Dummy local e inyecta
     en caliente los datos desencriptados en el montaje PFS, garantizando cero
     errores de corrupcion tras formateos o cambio de consola.
2. Resignado Automatico Estilo Apollo e Indexacion de Base de Datos para PS4:
   - Modificacion 100% automatica del Account ID de 64 bits, actualizacion de user_id
     y recalculo exacto de psid_hmac en param.sfo dentro de contenedores PFS.
   - Registro directo en la base de datos savedata.db del sistema y reparacion de
     banderas de corrupcion (is_broken=0, fake_broken=0) para eliminar errores de
     "Datos dañados".
   - Preservacion de keystone para que el juego reconozca la partida de inmediato.
3. Estacion Web Local (:8383) con Codigo QR en Pantalla:
   - Servidor HTTP integrado en el puerto 8383 sin colisiones en la consola.
   - Pulsa Triangulo para ver la URL y un Codigo QR escaneable con la camara del
     movil para entrar al instante sin escribir direcciones IP.

CONTENIDO DEL PAQUETE:
- PPSA99700/eboot.bin             (Ejecutable nativo firmado)
- PPSA99700/sce_module/libc.prx   (Capa de compatibilidad runtime Prospero)
- PPSA99700/sce_sys/param.json    (Metadatos de paquete Title ID: PPSA99700)
- PPSA99700/sce_sys/icon0.png     (Icono oficial Liquid Glass Memory Card)
- PPSA99700/assets/               (Fuentes SDF, gráficos de interfaz, banco de sonido)
- PPSA99700/daemon/               (Demonio integrado de privilegios root)
- PPSA99700/INSTRUCTIONS.txt      (Guía de instalación y uso)
- PPSA99700/LEGAL.txt             (Licencia, garantía y avisos legales)

INSTRUCCIONES DE INSTALACION (METODO FACIL VIA FTP):
1. Conecta tu PS5 y tu computadora o telefono a la misma red local.
2. Habilita el servidor FTP en tu PS5 (en los ajustes de OnionHEN, etaHEN o payload).
3. Conéctate usando un cliente FTP (FileZilla, WinSCP, Cyberduck, etc.).
4. Copia la carpeta "PPSA99700" completa directamente en:
     /data/homebrew/
   La ruta resultante debe ser:
     /data/homebrew/PPSA99700/eboot.bin
5. Inicia MacheteSaveGameManager directamente desde tu lanzador de homebrew o menú principal.

CONTROLES DEL MANDO DUALSENSE:
- D-Pad: Navegar por la lista de juegos y opciones
- Cruz (X): Seleccionar / Confirmar acción
- Círculo (O): Cancelar / Volver
- Triángulo: Abrir Estacion Web Local (Servidor HTTP con Codigo QR)
- Cuadrado: Actualizar lista de juegos y dispositivos USB
- L3 (Click Stick Izquierdo): Alternar idioma (Inglés / Español) en tiempo real
=====================================================
EOF

# 4. Crear documento legal con licencias y marcas (Nombre en inglés: LEGAL.txt, Inglés primero, Español después)
cat << 'EOF' > "$dist_dir/LEGAL.txt"
=====================================================
   MacheteSVG v1.0.0 - Legal Notice & Licensing
=====================================================

[ENGLISH]
1. LICENSE:
MacheteSVG is free software licensed under the GNU General Public License
Version 3.0 (GPL-3.0), as published by the Free Software Foundation.
A copy of the GNU General Public License is included in the project repository
and available at: https://www.gnu.org/licenses/gpl-3.0.html

2. DISCLAIMER OF WARRANTY & LIMITATION OF LIABILITY:
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

3. TRADEMARKS & NON-AFFILIATION:
"PlayStation", "PS5", "PS4", and "DualSense" are registered trademarks or
trademarks of Sony Interactive Entertainment Inc.
MacheteSVG is an independent open-source homebrew software project.
MacheteSVG is not affiliated with, endorsed by, or sponsored by Sony Interactive
Entertainment Inc. or any of its subsidiaries or affiliates.

4. THIRD-PARTY NOTICES & ACKNOWLEDGMENTS:
- ProsperoStore / ps5-homebrew-ui (Copyright (C) 2026 BlackBearReloaded) - GPL-3.0
- Apollo Save Tool (Copyright (C) Bucanero) - GPL-3.0
- PS5 Lapy JB Daemon (Copyright (C) ArkSama & Lapy)
- ShadowMountPlus (Copyright (C) Drakmor & VoidWhisper) - GPL-3.0
- kstuff-lite (Copyright (C) EchoStretch & sleirsgoevy)
- KSystemLib
- ps5-payload-sdk (Copyright (C) John Törnblom) - BSD-2-Clause
- stb_image / stb_truetype (Copyright (C) Sean Barrett) - Public Domain / MIT
- cJSON (Copyright (C) Dave Gamble) - MIT

-----------------------------------------------------

[ESPAÑOL]
1. LICENCIA:
MacheteSVG es software libre distribuido bajo los términos de la Licencia
Pública General GNU Versión 3.0 (GPL-3.0), publicada por la Free Software Foundation.
Una copia de la licencia está disponible en: https://www.gnu.org/licenses/gpl-3.0.html

2. DESCARGO DE GARANTIA Y LIMITACION DE RESPONSABILIDAD:
ESTE SOFTWARE SE SUMINISTRA "TAL CUAL", SIN GARANTIAS DE NINGUN TIPO, YA SEAN
EXPRESAS O IMPLICITAS, INCLUYENDO PERO SIN LIMITARSE A LAS GARANTIAS IMPLICITAS
DE COMERCIABILIDAD Y APTITUD PARA UN PROPOSITO PARTICULAR. EN NINGUN CASO EL
TITULAR DE LOS DERECHOS DE AUTOR O COLABORADORES SERAN RESPONSABLES DE DAÑOS
DIRECTOS, INDIRECTOS, INCIDENTALES, ESPECIALES, EJEMPLARES O CONSECUENTES
(INCLUYENDO LA PERDIDA DE USO, DATOS O BENEFICIOS, O INTERRUPCION DEL NEGOCIO)
SIN IMPORTAR LA CAUSA O CUALQUIER TEORIA DE RESPONSABILIDAD DERIVADA DEL USO
DE ESTE SOFTWARE.

3. MARCAS REGISTRADAS Y NO AFILIACION:
"PlayStation", "PS5", "PS4" y "DualSense" son marcas registradas de Sony
Interactive Entertainment Inc.
MacheteSVG es un proyecto independiente de software homebrew de código abierto.
MacheteSVG no está afiliado, respaldado ni patrocinado por Sony Interactive
Entertainment Inc. ni ninguna de sus filiales.

4. AVISOS DE TERCEROS Y RECONOCIMIENTOS:
- ProsperoStore / ps5-homebrew-ui (Copyright (C) 2026 BlackBearReloaded) - GPL-3.0
- Apollo Save Tool (Copyright (C) Bucanero) - GPL-3.0
- PS5 Lapy JB Daemon (Copyright (C) ArkSama & Lapy)
- ShadowMountPlus (Copyright (C) Drakmor y VoidWhisper) - GPL-3.0
- kstuff-lite (Copyright (C) EchoStretch y sleirsgoevy)
- KSystemLib
- ps5-payload-sdk (Copyright (C) John Törnblom) - BSD-2-Clause
- stb_image / stb_truetype (Copyright (C) Sean Barrett) - Public Domain / MIT
- cJSON (Copyright (C) Dave Gamble) - MIT
=====================================================
EOF

chmod 0644 "$dist_dir/INSTRUCTIONS.txt" "$dist_dir/LEGAL.txt"

# También colocar copias en la raíz del repositorio para documentación directa
cp "$dist_dir/INSTRUCTIONS.txt" "$root/INSTRUCTIONS.txt"
cp "$dist_dir/LEGAL.txt" "$root/LEGAL.txt"

echo "==> [5/5] Creando archivo ZIP final limpio que contiene únicamente la carpeta $title_id..."
store_zip="$root/dist/${title_id}.zip"
release_zip="$root/dist/MacheteSaveGameManager_v${version}_PS5.zip"
rm -f "$store_zip" "$release_zip"
(cd "$root/dist" && zip -r -q "$store_zip" "$title_id")
cp "$store_zip" "$release_zip"

echo "==> [5/5+] Aplicando permisos abiertos 0777 en las entradas del archivo ZIP..."
python3 "$root/tools/zip-open-modes.py" "$store_zip"
python3 "$root/tools/zip-open-modes.py" "$release_zip"

echo ""
echo "=================================================="
echo " Empaquetado completado con éxito."
echo " Paquete Store:   $store_zip"
echo " Paquete Release: $release_zip"
echo " Carpeta de app:  $dist_dir"
echo "=================================================="
