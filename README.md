# Œil dans l'atelier — Serveur MCP

Serveur MCP (Model Context Protocol) qui expose les 4 ESP32-CAM de l'atelier
à Claude Code via stdio.

## Installation

```bash
cd ~/projets/oeil-atelier-mcp
# (placer oeil_mcp_server.py et requirements.txt ici)

# Environnement virtuel Python (recommandé pour ne pas polluer le système)
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Test manuel (vérifier que les imports passent)

```bash
source .venv/bin/activate
python3 -c "import mcp, httpx; print('OK')"
```

## Déclaration dans Claude Code

Claude Code utilise un fichier de configuration pour déclarer les serveurs MCP.

```bash
claude mcp add oeil-atelier \
  /home/ch/projets/oeil-atelier-mcp/.venv/bin/python3 \
  /home/ch/projets/oeil-atelier-mcp/oeil_mcp_server.py
```

(Ajustez les chemins selon votre installation réelle.)

## Outils exposés

- `list_cameras()` : statut des 4 cams (en ligne / RSSI / uptime / LED)
- `get_camera_snapshot(camera_number)` : photo instantanée d'une cam (1 à 4)
- `get_camera_status(camera_number)` : statut technique détaillé
- `set_camera_led(camera_number, state)` : on / off / toggle de la LED flash

## Utilisation dans Claude Code

```
> Liste mes caméras d'atelier
> Regarde la cam-2 et dis-moi ce que tu vois
> Allume la LED de cam-3, attends 2s, puis prends un snapshot, puis éteins la LED
```

## Dépannage

Logs du serveur : tout est écrit sur stderr (préfixe `[oeil-mcp]`). Visibles dans
Claude Code via le panneau MCP, ou en lançant le serveur manuellement pour test.

mDNS qui ne répond pas : vérifier que `avahi-daemon` tourne :
```bash
systemctl status avahi-daemon
avahi-resolve -n cam-1.local
```

En dernier recours, remplacer `cam-{}.local` par les IP directes dans
le code (constante `CAMERA_HOSTNAME`).
