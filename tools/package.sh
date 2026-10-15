#!/usr/bin/env bash
# MacheteSVG - Packaging and Distribution Script for Jailbroken PS5
# Copyright (C) 2026 MacheteSVG Team
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
title_id="PPSA99001"
app_name="MacheteSVG"
version="1.0.0"

echo "=================================================="
echo "   Empaquetador de Distribución de MacheteSVG     "
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
   MacheteSVG v1.0.0 - PS5 & PS4 Savegame Manager
=====================================================

[ENGLISH GUIDE]
SYSTEM REQUIREMENTS & COMPATIBILITY:
- Console: PlayStation 5 (All Jailbroken Firmwares)
- Environments: OnionHEN, etaHEN, or any PS5 jailbreak environment
- Mounting engine: Native Prospero libSceSaveData / ShadowMount
- Save Formats: PlayStation 5 native titles and PlayStation 4 backwards-compatible (BC) games

AUTONOMOUS & FULLY SELF-CONTAINED:
- MacheteSVG contains everything needed to operate out of the box.
- No external daemons or payload senders are required.
- If a resident daemon is running, MacheteSVG detects and uses it.
- If no daemon is running, MacheteSVG automatically launches its embedded daemon
  via the local loader on 127.0.0.1:9021 without requiring user intervention.

PACKAGE CONTENTS:
- PPSA99001/eboot.bin             (Signed native application executable)
- PPSA99001/sce_module/libc.prx   (Prospero runtime compatibility layer)
- PPSA99001/sce_sys/param.json    (Title ID: PPSA99001 package metadata)
- PPSA99001/sce_sys/icon0.png     (Official Liquid Glass Memory Card icon)
- PPSA99001/assets/               (SDF fonts, UI graphics, soundbank)
- PPSA99001/daemon/               (Embedded privilege escalation daemon)
- PPSA99001/INSTRUCTIONS.txt      (Installation and operational guide)
- PPSA99001/LEGAL.txt             (License, warranty, and trademark notices)

INSTALLATION INSTRUCTIONS:
METHOD 1: VIA FTP (Recommended)
1. Ensure your PS5 and computer are connected to the same local network.
2. Enable FTP server on your PS5 (e.g., via OnionHEN / etaHEN settings).
3. Connect using an FTP client (FileZilla, WinSCP, Cyberduck, etc.).
4. Upload the entire "PPSA99001" folder to:
     /data/homebrew/
   The resulting path must be:
     /data/homebrew/PPSA99001/eboot.bin
5. Refresh your homebrew launcher (Itemzflow, ProsperoStore, or system menu).

METHOD 2: VIA USB DRIVE (exFAT)
1. Copy the "PPSA99001" folder to the root of an exFAT-formatted USB drive.
2. Insert the USB drive into your PS5.
3. Using a file manager (PS5-Explorer or Itemzflow), copy "PPSA99001" to /data/homebrew/.

CONTROLLER SHORTCUTS:
- D-Pad: Navigate game list and menus
- Cross (X): Select / Confirm action
- Circle (O): Cancel / Back
- Triangle: Open Web Save Station (LAN / Wi-Fi local HTTP server)
- Square: Refresh games list and connected USB devices
- L3 (Left Stick Click): Switch language (English / Spanish) in real-time

-----------------------------------------------------

[GUIA EN ESPAÑOL]
REQUISITOS DEL SISTEMA Y COMPATIBILIDAD:
- Consola: PlayStation 5 (Todos los Firmwares con Jailbreak)
- Entornos: OnionHEN, etaHEN o cualquier entorno jailbreak de PS5
- Motor de montaje: libSceSaveData nativo de Prospero / ShadowMount
- Formatos: Títulos nativos de PlayStation 5 y juegos de PS4 en retrocompatibilidad (BC)

TOTALMENTE AUTONOMO Y AUTOCONTENIDO:
- MacheteSVG incluye todo lo necesario para funcionar de inmediato.
- No se requieren demonios externos ni aplicaciones de inyección de payloads.
- Si un demonio residente está activo, MacheteSVG lo detecta y lo utiliza.
- Si no hay ningún demonio en ejecución, MacheteSVG auto-lanza su demonio integrado
  mediante el cargador local en 127.0.0.1:9021 de forma transparente.

CONTENIDO DEL PAQUETE:
- PPSA99001/eboot.bin             (Ejecutable nativo firmado)
- PPSA99001/sce_module/libc.prx   (Capa de compatibilidad runtime Prospero)
- PPSA99001/sce_sys/param.json    (Metadatos de paquete Title ID: PPSA99001)
- PPSA99001/sce_sys/icon0.png     (Icono oficial Liquid Glass Memory Card)
- PPSA99001/assets/               (Fuentes SDF, gráficos de interfaz, banco de sonido)
- PPSA99001/daemon/               (Demonio integrado de privilegios root)
- PPSA99001/INSTRUCTIONS.txt      (Guía de instalación y uso)
- PPSA99001/LEGAL.txt             (Licencia, garantía y avisos legales)

INSTRUCCIONES DE INSTALACION:
METODO 1: MEDIANTE FTP (Recomendado)
1. Conecta tu PS5 y tu computadora a la misma red local.
2. Habilita el servidor FTP en tu PS5 (por ejemplo, en las opciones de OnionHEN o etaHEN).
3. Conéctate usando un cliente FTP (FileZilla, WinSCP, Cyberduck, etc.).
4. Sube la carpeta "PPSA99001" completa a:
     /data/homebrew/
   La ruta resultante debe ser:
     /data/homebrew/PPSA99001/eboot.bin
5. Actualiza tu lanzador de homebrew (Itemzflow, ProsperoStore o menú de inicio).

METODO 2: MEDIANTE UNIDAD USB (exFAT)
1. Copia la carpeta "PPSA99001" en la raíz de una memoria USB formateada en exFAT.
2. Conecta la memoria USB en tu PS5.
3. Usando un administrador de archivos (PS5-Explorer o Itemzflow), copia "PPSA99001" a /data/homebrew/.

CONTROLES DEL MANDO DUALSENSE:
- D-Pad: Navegar por la lista de juegos y opciones
- Cruz (X): Seleccionar / Confirmar acción
- Círculo (O): Cancelar / Volver
- Triángulo: Abrir Estacion Web Local (Servidor HTTP para descarga por Wi-Fi / LAN)
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
zip_output="$root/dist/MacheteSVG_v${version}_PS5.zip"
rm -f "$zip_output"
(cd "$root/dist" && zip -r -q "$zip_output" "$title_id")

echo "==> [5/5+] Aplicando permisos abiertos 0777 en las entradas del archivo ZIP..."
python3 "$root/tools/zip-open-modes.py" "$zip_output"

echo ""
echo "=================================================="
echo " Empaquetado completado con éxito."
echo " Paquete generado: $zip_output"
echo " Carpeta de app:   $dist_dir"
echo "=================================================="
