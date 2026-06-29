#!/usr/bin/env python3
"""
Serveur MCP « Œil dans l'atelier »  v5
=======================================

Expose les 4 ESP32-CAM et le microscope Dino-Lite USB à Claude Code via MCP.

Outils exposés :
  - list_cameras()                       : liste les 4 cams et leur statut
  - get_camera_snapshot(camera_number)   : capture une image fraîche d'une cam
                                           + sauvegarde sur disque
  - get_camera_status(camera_number)     : récupère le JSON /status d'une cam
  - set_camera_led(camera_number, state) : contrôle la LED (on/off/toggle)
  - set_camera_orientation(camera_number,
                           flip_vertical,
                           mirror_horizontal) : corrige l'orientation
  - get_microscope_snapshot(settle_ms)   : capture macro via Dino-Lite USB

Les cams sont supposées accessibles via leurs hostnames mDNS :
  cam-1.local, cam-2.local, cam-3.local, cam-4.local

Le microscope est attendu sur /dev/video1 (Dino-Lite Basic, 640x480 max).
"""

import asyncio
import base64
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

import httpx
from mcp.server import Server
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent, ImageContent

# ---- Configuration ----
NUM_CAMERAS = 4
CAMERA_HOSTNAME = "cam-{}.local"   # cam-1.local, cam-2.local, ...
HTTP_TIMEOUT = 8.0                 # secondes
SNAPSHOT_TIMEOUT = 15.0            # un peu plus long pour les images
SNAPSHOT_DIR = Path("/tmp")        # où sauvegarder les snapshots sur disque

# ---- Microscope Dino-Lite ----
MICROSCOPE_DEVICE = "/dev/v4l/by-id/usb-AnMo_Electronics_Corporation_Dino-Lite_Basic-video-index0"
MICROSCOPE_RESOLUTION = "640x480"   # max du Dino-Lite Basic
MICROSCOPE_TIMEOUT = 10.0           # secondes
MICROSCOPE_OUTPUT = SNAPSHOT_DIR / "oeil-microscope.jpg"


# ---- Logging utile (sur stderr, pas stdout : stdout = canal MCP) ----
def log(msg: str) -> None:
    print(f"[oeil-mcp] {msg}", file=sys.stderr, flush=True)


# ---- Helpers HTTP ----
def cam_url(camera_number: int, endpoint: str) -> str:
    """Construit l'URL d'un endpoint pour une cam donnée."""
    if not (1 <= camera_number <= NUM_CAMERAS):
        raise ValueError(f"Numéro de cam invalide : {camera_number} (1 à {NUM_CAMERAS})")
    host = CAMERA_HOSTNAME.format(camera_number)
    return f"http://{host}{endpoint}"


def snapshot_files(camera_number: int) -> tuple[Path, Path]:
    """Retourne (chemin_courant, chemin_horodaté) pour le snapshot d'une cam.

    Le chemin courant est toujours le même (pratique pour un visualiseur live).
    Le chemin horodaté garde l'historique (utile pour les comparaisons).
    """
    ts = datetime.now().strftime("%Y%m%d-%H%M%S")
    current = SNAPSHOT_DIR / f"oeil-cam-{camera_number}.jpg"
    archived = SNAPSHOT_DIR / f"oeil-cam-{camera_number}-{ts}.jpg"
    return current, archived


async def fetch_status(camera_number: int) -> dict[str, Any] | None:
    """Récupère le JSON /status d'une cam, ou None si injoignable."""
    url = cam_url(camera_number, "/status")
    try:
        async with httpx.AsyncClient(timeout=HTTP_TIMEOUT) as client:
            r = await client.get(url)
            r.raise_for_status()
            return r.json()
    except Exception as e:
        log(f"cam-{camera_number} status KO : {e}")
        return None


async def fetch_snapshot(camera_number: int) -> bytes | None:
    """Récupère un JPEG /snapshot d'une cam, ou None si problème."""
    url = cam_url(camera_number, "/snapshot")
    try:
        async with httpx.AsyncClient(timeout=SNAPSHOT_TIMEOUT) as client:
            r = await client.get(url)
            r.raise_for_status()
            return r.content
    except Exception as e:
        log(f"cam-{camera_number} snapshot KO : {e}")
        return None


async def fetch_led(camera_number: int, state: str) -> dict[str, Any] | None:
    """Pilote la LED d'une cam, retourne le JSON de réponse."""
    url = cam_url(camera_number, f"/led?state={state}")
    try:
        async with httpx.AsyncClient(timeout=HTTP_TIMEOUT) as client:
            r = await client.get(url)
            r.raise_for_status()
            return r.json()
    except Exception as e:
        log(f"cam-{camera_number} led KO : {e}")
        return None


async def post_settings(camera_number: int, payload: dict[str, Any]) -> dict[str, Any] | None:
    """POST des paramètres caméra sur /settings (persistant en flash).

    Champs acceptés par le firmware : frameSize, quality, brightness, contrast,
    saturation, hMirror, vFlip.
    """
    url = cam_url(camera_number, "/settings")
    try:
        async with httpx.AsyncClient(timeout=HTTP_TIMEOUT) as client:
            r = await client.post(url, json=payload)
            r.raise_for_status()
            return r.json()
    except Exception as e:
        log(f"cam-{camera_number} settings KO : {e}")
        return None


async def capture_microscope(settle_ms: int = 500) -> bytes | None:
    """Capture une image via le Dino-Lite USB en appelant fswebcam.

    settle_ms : délai après ouverture du device avant la capture (pour que
                l'exposition se stabilise). 500 ms est un bon défaut pour macro.
    Retourne le JPEG en bytes, ou None en cas d'échec.
    """
    if not Path(MICROSCOPE_DEVICE).exists():
        log(f"microscope : device {MICROSCOPE_DEVICE} inexistant")
        return None

    # Délai en secondes pour --delay (entier minimum 0)
    delay_s = max(0, settle_ms // 1000)

    cmd = [
        "fswebcam",
        "-d", MICROSCOPE_DEVICE,
        "-r", MICROSCOPE_RESOLUTION,
        "--no-banner",
        "-S", "5",              # skip les 5 premières frames (balance des blancs)
        "--delay", str(delay_s),
        "--jpeg", "85",         # qualité JPEG
        str(MICROSCOPE_OUTPUT),
    ]
    log(f"microscope : capture via {' '.join(cmd)}")

    try:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            _, stderr = await asyncio.wait_for(proc.communicate(), timeout=MICROSCOPE_TIMEOUT)
        except asyncio.TimeoutError:
            proc.kill()
            log("microscope : timeout fswebcam")
            return None

        if proc.returncode != 0:
            log(f"microscope : fswebcam KO ({proc.returncode}) : {stderr.decode(errors='ignore')[:300]}")
            return None

        if not MICROSCOPE_OUTPUT.exists():
            log("microscope : fichier de sortie absent après capture")
            return None

        return MICROSCOPE_OUTPUT.read_bytes()
    except FileNotFoundError:
        log("microscope : fswebcam non installé. sudo apt install fswebcam")
        return None
    except Exception as e:
        log(f"microscope : erreur inattendue : {e}")
        return None


# ---- Serveur MCP ----
app = Server("oeil-atelier")


@app.list_tools()
async def list_tools() -> list[Tool]:
    """Déclare les outils disponibles à Claude."""
    return [
        Tool(
            name="list_cameras",
            description=(
                "Liste les 4 caméras de l'atelier avec leur statut "
                "(en ligne ou non, IP, RSSI WiFi, uptime, état LED). "
                "À appeler en premier pour vérifier quelles cams sont disponibles."
            ),
            inputSchema={
                "type": "object",
                "properties": {},
                "required": [],
            },
        ),
        Tool(
            name="get_camera_snapshot",
            description=(
                "Capture une photo instantanée d'une caméra et renvoie l'image. "
                "L'image est aussi enregistrée sur disque dans deux fichiers : "
                "/tmp/oeil-cam-N.jpg (écrasé à chaque snapshot, pratique pour un "
                "visualiseur en mode live) et /tmp/oeil-cam-N-YYYYMMDD-HHMMSS.jpg "
                "(horodaté, conserve l'historique). "
                "Utiliser pour voir ce qui se passe devant la cam choisie. "
                "Numéros valides : 1 à 4."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "camera_number": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": NUM_CAMERAS,
                        "description": "Numéro de la cam (1 à 4)",
                    },
                },
                "required": ["camera_number"],
            },
        ),
        Tool(
            name="get_camera_status",
            description=(
                "Récupère le statut technique détaillé d'une caméra "
                "(IP, RSSI WiFi, mémoire libre, uptime, état LED). "
                "Utile pour diagnostic ou vérification de l'état réseau."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "camera_number": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": NUM_CAMERAS,
                    },
                },
                "required": ["camera_number"],
            },
        ),
        Tool(
            name="set_camera_led",
            description=(
                "Contrôle la LED flash blanche d'une caméra. "
                "Utile pour éclairer une zone sombre avant de prendre un snapshot. "
                "ATTENTION : la LED chauffe, l'éteindre après usage."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "camera_number": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": NUM_CAMERAS,
                    },
                    "state": {
                        "type": "string",
                        "enum": ["on", "off", "toggle"],
                        "description": "Action sur la LED",
                    },
                },
                "required": ["camera_number", "state"],
            },
        ),
        Tool(
            name="set_camera_orientation",
            description=(
                "Corrige l'orientation de l'image d'une caméra (sauvegarde persistante "
                "en flash, conservé après reboot). À utiliser quand une cam est montée "
                "à l'envers, sur le côté, ou que l'utilisateur signale une image "
                "retournée. "
                "flip_vertical=true ET mirror_horizontal=true ensemble = rotation 180°. "
                "Un seul des deux = effet miroir (utile si la cam est en biais). "
                "Pour ne modifier qu'un paramètre, omettre l'autre."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "camera_number": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": NUM_CAMERAS,
                    },
                    "flip_vertical": {
                        "type": "boolean",
                        "description": "Retournement vertical (haut-bas)",
                    },
                    "mirror_horizontal": {
                        "type": "boolean",
                        "description": "Miroir horizontal (gauche-droite)",
                    },
                },
                "required": ["camera_number"],
            },
        ),
        Tool(
            name="get_microscope_snapshot",
            description=(
                "Capture une image macro avec le microscope USB Dino-Lite "
                "(connecté en local sur /dev/video1, résolution 640x480). "
                "Idéal pour examiner des composants CMS, des soudures, des gravures, "
                "des pièces mécaniques fines (engrenages, vis, etc.). "
                "L'utilisateur doit avoir positionné le sujet sous la lentille et "
                "fait la mise au point manuellement avant l'appel. "
                "L'image est aussi sauvegardée dans /tmp/oeil-microscope.jpg "
                "(et version horodatée)."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "settle_ms": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 3000,
                        "description": (
                            "Délai en millisecondes pour stabiliser l'exposition "
                            "avant la capture (défaut 500). Augmenter à 1000-2000 "
                            "si la première image est mal exposée."
                        ),
                    },
                },
                "required": [],
            },
        ),
    ]


@app.call_tool()
async def call_tool(name: str, arguments: dict[str, Any]) -> list[TextContent | ImageContent]:
    """Implémente l'exécution des outils."""

    # ---- list_cameras ----
    if name == "list_cameras":
        tasks = [fetch_status(i) for i in range(1, NUM_CAMERAS + 1)]
        results = await asyncio.gather(*tasks)

        lines = ["État des caméras de l'atelier :\n"]
        for i, status in enumerate(results, start=1):
            if status is None:
                lines.append(f"  cam-{i} : HORS LIGNE")
            else:
                lines.append(
                    f"  cam-{i} : EN LIGNE  "
                    f"IP={status.get('ip', '?')}  "
                    f"RSSI={status.get('rssi', '?')}dBm  "
                    f"uptime={status.get('uptime', '?')}s  "
                    f"LED={status.get('led', '?')}"
                )
        return [TextContent(type="text", text="\n".join(lines))]

    # ---- get_camera_snapshot ----
    elif name == "get_camera_snapshot":
        cam = int(arguments["camera_number"])
        jpeg = await fetch_snapshot(cam)
        if jpeg is None:
            return [TextContent(
                type="text",
                text=f"Impossible de joindre cam-{cam}. Vérifier qu'elle est en ligne avec list_cameras."
            )]

        # Sauvegarde sur disque : un fichier "courant" + un fichier horodaté
        current_path, archived_path = snapshot_files(cam)
        saved_ok = True
        try:
            archived_path.write_bytes(jpeg)
            current_path.write_bytes(jpeg)
            log(f"cam-{cam} snapshot enregistré : {archived_path} (+ {current_path.name})")
        except Exception as e:
            log(f"cam-{cam} sauvegarde KO : {e}")
            saved_ok = False

        b64 = base64.b64encode(jpeg).decode("ascii")
        if saved_ok:
            path_msg = (
                f"\nImage enregistrée :"
                f"\n  - courante : {current_path}"
                f"\n  - horodatée : {archived_path}"
                f"\nOuvrir : xdg-open {current_path}"
            )
        else:
            path_msg = ""

        return [
            TextContent(
                type="text",
                text=f"Snapshot de cam-{cam} ({len(jpeg)} octets).{path_msg}",
            ),
            ImageContent(type="image", data=b64, mimeType="image/jpeg"),
        ]

    # ---- get_camera_status ----
    elif name == "get_camera_status":
        cam = int(arguments["camera_number"])
        status = await fetch_status(cam)
        if status is None:
            return [TextContent(type="text", text=f"cam-{cam} : HORS LIGNE ou injoignable")]
        return [TextContent(
            type="text",
            text=(
                f"cam-{cam} : EN LIGNE\n"
                f"  IP        : {status.get('ip', '?')}\n"
                f"  RSSI WiFi : {status.get('rssi', '?')} dBm\n"
                f"  Heap libre: {status.get('heap', '?')} octets\n"
                f"  Uptime    : {status.get('uptime', '?')} s\n"
                f"  LED       : {status.get('led', '?')}\n"
            )
        )]

    # ---- set_camera_led ----
    elif name == "set_camera_led":
        cam = int(arguments["camera_number"])
        state = arguments["state"]
        result = await fetch_led(cam, state)
        if result is None:
            return [TextContent(type="text", text=f"cam-{cam} : commande LED échouée")]
        return [TextContent(type="text", text=f"cam-{cam} LED = {result.get('led', '?')}")]

    # ---- set_camera_orientation ----
    elif name == "set_camera_orientation":
        cam = int(arguments["camera_number"])
        payload: dict[str, Any] = {}
        if "flip_vertical" in arguments:
            payload["vFlip"] = bool(arguments["flip_vertical"])
        if "mirror_horizontal" in arguments:
            payload["hMirror"] = bool(arguments["mirror_horizontal"])

        if not payload:
            return [TextContent(
                type="text",
                text=(
                    "Aucun paramètre fourni. Spécifier flip_vertical "
                    "et/ou mirror_horizontal (booléen)."
                ),
            )]

        result = await post_settings(cam, payload)
        if result is None:
            return [TextContent(
                type="text",
                text=f"cam-{cam} : modification orientation échouée",
            )]

        parts = []
        if "vFlip" in payload:
            parts.append(f"vFlip={'on' if payload['vFlip'] else 'off'}")
        if "hMirror" in payload:
            parts.append(f"hMirror={'on' if payload['hMirror'] else 'off'}")
        return [TextContent(
            type="text",
            text=(
                f"cam-{cam} orientation mise à jour : {', '.join(parts)} "
                f"(sauvegardé en flash, conservé après reboot)"
            ),
        )]

    # ---- get_microscope_snapshot ----
    elif name == "get_microscope_snapshot":
        settle_ms = int(arguments.get("settle_ms", 500))
        jpeg = await capture_microscope(settle_ms=settle_ms)
        if jpeg is None:
            return [TextContent(
                type="text",
                text=(
                    "Capture microscope échouée. "
                    "Vérifier que le Dino-Lite est branché et que /dev/video1 existe "
                    "(commande : v4l2-ctl --list-devices)."
                ),
            )]

        # Sauvegarde horodatée en plus du fichier courant
        ts = datetime.now().strftime("%Y%m%d-%H%M%S")
        archived = SNAPSHOT_DIR / f"oeil-microscope-{ts}.jpg"
        try:
            archived.write_bytes(jpeg)
            log(f"microscope : snapshot enregistré {archived} (+ {MICROSCOPE_OUTPUT.name})")
        except Exception as e:
            log(f"microscope : sauvegarde horodatée KO : {e}")
            archived = None

        b64 = base64.b64encode(jpeg).decode("ascii")
        path_msg = (
            f"\nImage enregistrée :"
            f"\n  - courante : {MICROSCOPE_OUTPUT}"
            + (f"\n  - horodatée : {archived}" if archived else "")
            + f"\nOuvrir : xdg-open {MICROSCOPE_OUTPUT}"
        )
        return [
            TextContent(
                type="text",
                text=f"Snapshot microscope ({len(jpeg)} octets, 640x480).{path_msg}",
            ),
            ImageContent(type="image", data=b64, mimeType="image/jpeg"),
        ]

    else:
        return [TextContent(type="text", text=f"Outil inconnu : {name}")]


# ---- Point d'entrée ----
async def main():
    log("Démarrage du serveur MCP « Œil dans l'atelier » v5")
    log(f"Snapshots : {SNAPSHOT_DIR}/oeil-cam-N.jpg (courant) + horodatés")
    log(f"Microscope : {MICROSCOPE_DEVICE} -> {MICROSCOPE_OUTPUT}")
    async with stdio_server() as (read_stream, write_stream):
        await app.run(read_stream, write_stream, app.create_initialization_options())


if __name__ == "__main__":
    asyncio.run(main())