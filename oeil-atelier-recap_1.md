# Œil dans l'atelier — Documentation projet

> Réseau de 4 caméras ESP32-CAM + microscope USB dans l'atelier, accessibles
> depuis Claude Code via un serveur MCP en Python tournant sur Linux Mint.

---

## 1. Vue d'ensemble

**Objectif** : permettre une interaction conversationnelle avec un « œil » virtuel
dans l'atelier — demander à Claude « regarde la cam-2 et dis-moi ce que tu vois »,
ou orchestrer LED + snapshot + analyse en une seule requête naturelle.

**Composants principaux** :

```
┌──────────────────────────────────────────────────────────────┐
│  Claude Code (CLI, sur Linux Mint, compte Pro)               │
│                          ▲                                   │
│                          │ MCP (stdio)                       │
│                          ▼                                   │
│  Serveur MCP Python « oeil-atelier »                         │
│    - venv dans ~/projets/oeil-atelier-mcp/.venv/             │
│    - lib mcp + httpx                                         │
│       │                                  │                   │
│       │ HTTP REST                        │ V4L2 (fswebcam)   │
│       ▼                                  ▼                   │
│  4× ESP32-CAM AI-Thinker          Dino-Lite USB              │
│  (firmware perso, mDNS            (/dev/video1, 640×480,     │
│   cam-1.local … cam-4.local)       macro x50)                │
│  WiFi : wifi_saliou_net_2GEXT                                │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Matériel

### 2.1 Les 4 caméras

| Cam   | IP attribuée    | Antenne     | Position physique                |
|-------|-----------------|-------------|----------------------------------|
| cam-1 | 192.168.0.150   | PCB intégrée| _(à compléter selon emplacement)_|
| cam-2 | 192.168.0.44    | PCB intégrée| _(à compléter)_                  |
| cam-3 | 192.168.0.45    | externe U.FL| _(à compléter)_                  |
| cam-4 | 192.168.0.16    | externe U.FL| _(à compléter)_                  |

> **Astuce** : peindre/coller le numéro sur le boîtier physique de chaque cam
> pour ne pas se perdre lors des manipulations.

### 2.2 Matériel pour reflashage

Configuration éprouvée pendant le projet (à reproduire si besoin de reflasher) :

- **Alim de labo réglée à 5,0 V**, limite courant ~1 A
- **Module FTDI USB-série** (jumper sur **3,3 V** pour les niveaux logiques)
- ESP32-CAM **sortie de son support MB** (le MB n'est pas utilisé)

Câblage :

```
ESP32-CAM           <->   Source
─────────────────────────────────────────────
5V                  <-    Alim labo  +5 V
GND  (un pin)       <-    Alim labo  GND
GND  (un autre)     <-    FTDI       GND      ← masse commune CRITIQUE
U0R  (= RX, GPIO3)  <-    FTDI       TX
U0T  (= TX, GPIO1)  ->    FTDI       RX
3V3                       NE PAS CONNECTER
```

**Pour entrer en mode download** (avant chaque upload) :
1. Jumper temporaire **IO0 ↔ GND**
2. Appui bref sur **RST** de la cam
3. Lancer l'upload depuis PlatformIO
4. À la fin (« Hard resetting via RTS pin... »), retirer le jumper IO0
5. Appui RST → boot normal

> ⚠️ Sur la cam-1, la pin GND en bas à droite (sous U0TXD) avait un faux contact.
> Utiliser un autre GND si vous y revenez.

### 2.3 Microscope USB Dino-Lite

- **Modèle** : Dino-Lite Basic (entrée de gamme)
- **Identifiants USB** : reconnu par v4l2 comme `Dino-Lite Basic`
- **Devices V4L2** : `/dev/video1` (utilisé) et `/dev/video2` (vide, à ignorer)
- **Résolution max** : 640×480 (YUYV uniquement, pas de MJPEG matériel)
- **Grossissement** : ~x50 optique → ~64 pixels/mm sur le sujet
- **Éclairage** : LED annulaires mécaniques (molette physique, non pilotables logiciel)
- **Utilisation** : examen CMS, soudures, gravures, mécanique fine (Hamann, etc.)

**Cadrage et mise au point** : se fait visuellement sur le PC Mint via VLC :

```bash
vlc v4l2:///dev/video1
```

(ou : VLC → Media → Open Capture Device → /dev/video1 → Play)

Une fois la mise au point faite et le sujet dans le champ, demander à Claude
de prendre un snapshot via l'outil MCP. VLC peut rester ouvert ou être fermé,
fswebcam saura ouvrir le device de toute façon.

---

## 3. Firmware ESP32-CAM

### 3.1 Localisation du projet

```
~/oeil-atelier/                  (à reconfirmer selon votre arborescence)
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── config_manager.h         persistance config (SSID, password, num)
│   ├── camera_setup.h           init caméra + paramètres
│   ├── serial_console.h         console interactive de config
│   └── web_handlers.h           routes HTTP + page web
└── README.md
```

### 3.2 Endpoints HTTP exposés par chaque cam

| Méthode | URL                | Description                                  |
|---------|--------------------|----------------------------------------------|
| GET     | `/`                | Page web minimale (preview + boutons)        |
| GET     | `/snapshot`        | JPEG instantané (en RAM, pas de SD)          |
| GET     | `/stream`          | Flux MJPEG multipart (1 connexion à la fois) |
| GET     | `/led?state=...`   | LED on / off / toggle                        |
| GET     | `/status`          | JSON : camNumber, IP, RSSI, heap, uptime, LED|
| GET     | `/settings`        | JSON paramètres caméra courants              |
| POST    | `/settings`        | Modifie paramètres (persistant en LittleFS)  |

### 3.3 Console série au boot

Au démarrage de chaque cam, **3 secondes** pour entrer dans la console
(toute touche reçue sur le port série).

Commandes :

| Commande              | Effet                                    |
|-----------------------|------------------------------------------|
| `help`                | Liste des commandes                      |
| `info`                | Configuration courante                   |
| `ssid <ssid>`         | Définit le SSID (espaces autorisés)      |
| `pass <mot-de-passe>` | Définit le mot de passe WiFi             |
| `num <1-4>`           | Définit le numéro de la cam              |
| `save`                | Sauvegarde et redémarre                  |
| `clear`               | Efface toute la config et redémarre      |
| `start`               | Quitte la console et démarre normalement |
| `reboot`              | Redémarre l'ESP                          |

### 3.4 Paramètres caméra persistants

Sauvegardés dans `/camera.json` du LittleFS, conservés entre reboots.
Modifiables via POST `/settings` ou via l'outil MCP `set_camera_orientation`.

Champs : `frameSize`, `quality`, `brightness`, `contrast`, `saturation`,
`hMirror`, `vFlip`.

**Tailles de frame** :
0=QQVGA 160×120 · 4=QVGA 320×240 · 6=VGA 640×480 · 7=SVGA 800×600 (défaut) ·
8=XGA 1024×768 · 9=SXGA 1280×1024 · 10=UXGA 1600×1200

---

## 4. Serveur MCP

### 4.1 Localisation

```
/home/ch/projets/oeil-atelier-mcp/
├── .venv/                       environnement Python isolé
├── oeil_mcp_server.py           le serveur (~390 lignes)
├── requirements.txt             dépendances : mcp + httpx
└── README.md
```

### 4.2 Outils exposés à Claude

| Outil                       | Paramètres                                | Effet                                  |
|-----------------------------|-------------------------------------------|----------------------------------------|
| `list_cameras`              | _(aucun)_                                 | Statut des 4 cams (parallélisé)        |
| `get_camera_snapshot`       | `camera_number` (1-4)                     | Photo + sauvegarde disque              |
| `get_camera_status`         | `camera_number` (1-4)                     | JSON détaillé                          |
| `set_camera_led`            | `camera_number`, `state` (on/off/toggle)  | Pilote la LED flash                    |
| `set_camera_orientation`    | `camera_number`, `flip_vertical?`, `mirror_horizontal?` | Corrige rotation (persistant) |
| `get_microscope_snapshot`   | `settle_ms?` (0-3000, défaut 500)         | Capture macro via Dino-Lite USB        |

### 4.3 Sauvegarde des snapshots

Chaque `get_camera_snapshot` écrit deux fichiers :

- `/tmp/oeil-cam-N.jpg` — toujours la dernière image (écrasé)
- `/tmp/oeil-cam-N-YYYYMMDD-HHMMSS.jpg` — horodaté, historique conservé

Chaque `get_microscope_snapshot` écrit également :

- `/tmp/oeil-microscope.jpg` — toujours la dernière image (écrasé)
- `/tmp/oeil-microscope-YYYYMMDD-HHMMSS.jpg` — horodaté

> `/tmp` est nettoyé au reboot de Mint. Pour conserver, copier ailleurs.

### 4.4 Lancement

Pas de lancement manuel — Claude Code démarre le serveur automatiquement
au besoin (à chaque nouvelle session `claude` qui utilise un de ses outils).

Pour tester manuellement (vérifie les imports) :

```bash
cd ~/projets/oeil-atelier-mcp
source .venv/bin/activate
python3 oeil_mcp_server.py
# devrait afficher : [oeil-mcp] Démarrage du serveur MCP « Œil dans l'atelier » v4
# Ctrl+C pour arrêter
```

---

## 5. Claude Code

### 5.1 Démarrage

```bash
cd ~/projets/oeil-atelier-mcp    # n'importe quel répertoire fonctionne
claude
```

### 5.2 Gestion du MCP « oeil-atelier »

```bash
claude mcp list                  # liste les serveurs MCP configurés
claude mcp remove oeil-atelier   # retire (si besoin)

# Réinstaller :
claude mcp add oeil-atelier \
  /home/ch/projets/oeil-atelier-mcp/.venv/bin/python3 \
  /home/ch/projets/oeil-atelier-mcp/oeil_mcp_server.py
```

### 5.3 Exemples d'utilisation conversationnelle

```
> Liste l'état de mes 4 cams
> Prends un snapshot de cam-2 et décris ce que tu vois
> Allume la LED de cam-3, attends 2 secondes, prends un snapshot, éteins la LED, analyse l'image
> L'image de cam-3 est à l'envers, corrige son orientation
> Compare les snapshots de cam-1 et cam-2
> Regarde au microscope et identifie le composant que je viens de placer
> Examine la soudure au microscope, est-ce qu'elle a l'air saine ?
> Lis le marquage du composant au microscope (essayer settle_ms=1500 si mal exposé)
```

---

## 6. Maintenance courante

### 6.1 Vérifier que les cams sont en ligne (sans Claude)

```bash
for i in 1 2 3 4; do
  echo "=== cam-$i ==="
  curl -s --max-time 3 http://cam-$i.local/status
  echo
done
```

### 6.2 Vérifier qu'une cam est joignable depuis Linux

```bash
ping -c 2 cam-2.local
avahi-resolve -n cam-2.local
```

### 6.3 Rebooter une cam à distance

Aucun endpoint dédié, mais via le port série si vous y avez accès :
console série → `reboot`. Sinon, coupure/remise du courant.

### 6.4 Changer le WiFi des cams

Connecter chaque cam au FTDI, ouvrir le moniteur série, taper une touche
dans les 3 premières secondes, puis :

```
ssid NouveauReseau
pass NouveauMotDePasse
save
```

### 6.5 Reset complet d'une cam

Console série → `clear` (efface config WiFi + num + orientation).

### 6.6 Mettre à jour le serveur MCP

```bash
cd ~/projets/oeil-atelier-mcp
# remplacer oeil_mcp_server.py par la nouvelle version
# /exit dans Claude Code en cours, puis relancer claude
```

Si nouvelles dépendances :

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

---

## 7. Dépannage

### 7.1 Une cam n'apparaît plus dans `list_cameras`

1. Ping : `ping cam-N.local`
2. Si KO : `avahi-resolve -n cam-N.local` puis ping de l'IP directement
3. Si l'IP ne répond pas non plus : cam plantée → couper/remettre le courant
4. Si après reboot la cam n'arrive pas à se connecter au WiFi → reflashage console série

### 7.2 mDNS qui rate côté Linux

```bash
systemctl status avahi-daemon
sudo systemctl restart avahi-daemon
```

Contournement : remplacer `cam-{}.local` par les IP directes dans
`oeil_mcp_server.py` (constante `CAMERA_HOSTNAME`).

### 7.3 « Connection reset by peer » dans les logs des cams

Bénin. Un client (Firefox, curl) a fermé sa connexion brutalement.
N'affecte pas le fonctionnement.

### 7.4 Snapshot retourne 500 ou timeout

- Probable saturation du serveur (qui ne gère qu'1 connexion à la fois)
- Vérifier qu'aucun navigateur n'est ouvert sur `/stream` de la même cam
- Rebooter la cam si persistant

### 7.5 Brown-out au boot (cam qui redémarre en boucle, « nul nul » au moniteur série)

- Alimenter via une vraie source 5V (alim labo ou bon chargeur USB-C),
  pas via l'USB du PC qui peut être trop limité
- Vérifier la masse commune entre alim labo et FTDI

### 7.6 Image à l'envers

```
> set_camera_orientation(camera_number=N, flip_vertical=true, mirror_horizontal=true)
```

Ou directement :
```bash
curl -X POST http://cam-N.local/settings -H "Content-Type: application/json" \
     -d '{"vFlip": true, "hMirror": true}'
```

### 7.7 Microscope qui ne répond pas

```bash
# Vérifier que le device existe
ls -la /dev/video*
v4l2-ctl --list-devices

# Le Dino-Lite doit apparaître. Si /dev/video1 a changé de numéro
# (par exemple suite à un débranchement/rebranchement d'autres webcams),
# mettre à jour MICROSCOPE_DEVICE dans oeil_mcp_server.py.

# Test direct sans Claude
fswebcam -d /dev/video1 -r 640x480 --no-banner -S 5 /tmp/test.jpg
ls -la /tmp/test.jpg
```

### 7.8 Image microscope trop sombre ou floue

- Augmenter `settle_ms` (1000 à 2000) pour laisser le temps à l'exposition de se stabiliser
- Allumer les LED du microscope via la molette physique
- Vérifier la mise au point via VLC : `vlc v4l2:///dev/video1`

---

## 8. Backlog évolutions futures

Idées notées au fil du projet, à reprendre quand un besoin réel émerge :

- [ ] `snapshot_with_led(cam, hold_ms)` : LED on → délai → snapshot → LED off
- [ ] `snapshot_all_cameras()` : capture parallèle des 4 cams en une requête
- [ ] `compare_snapshots(cam, delay_seconds)` : détection de changement entre deux photos
- [ ] `set_camera_image_params(brightness, contrast, saturation)` : pour ajuster l'image
- [ ] `set_camera_resolution(cam, size)` : passer en UXGA pour sujets éloignés
- [ ] `aim_camera(camera_number, pan, tilt)` : si une ou deux cams sont montées sur servos
- [ ] `set_microscope_resolution(width, height)` : basculer entre les 5 résolutions du Dino-Lite
- [ ] `microscope_burst(count, delay_ms)` : rafale macro (utile car focale variable)
- [ ] **Hardware** : capteur PIR sur GPIO 13 pour capture déclenchée par mouvement
- [ ] **Hardware** : 5e ESP32 avec relais pour piloter l'éclairage de l'atelier
       (la LED des cams ne porte qu'à ~50 cm — un vrai éclairage piloté serait
       plus utile pour les sujets éloignés)
- [ ] **Hardware** : ESP avec capteur température/pression (déjà en place, page web standalone)
       — à intégrer au MCP uniquement si un cas d'usage croisé émerge
- [ ] OTA (mise à jour firmware par WiFi) : changer `board_build.partitions`
       pour `default.csv` et ajouter ArduinoOTA dans `main.cpp` (~15 lignes)

**Fait** :
- [x] Microscope USB Dino-Lite intégré au MCP (v5, juin 2026)

---

## 9. Décisions d'architecture (pour mémoire)

- **Firmware** : un seul binaire pour les 4 cams, configuration par numéro
  via console série au boot. Pas de build flag par cam.
- **mDNS** : `cam-N.local` plutôt que des IPs en dur, pour robustesse face
  aux changements DHCP du routeur.
- **HTTP synchrone** (1 connexion à la fois) : choix volontaire pour simplicité.
  Le MCP fait des appels ponctuels courts, pas d'impact réel.
- **Pas de SD** : économie de complexité, pas d'usure de flash, snapshots
  en RAM uniquement.
- **Linux Mint comme hub central** : tout tourne ici (Claude Code + MCP),
  les cams sont juste des capteurs réseau.
- **Compte Claude Pro requis** : Claude Code et son support MCP sont
  derrière un plan payant. Forfait annuel pris pour le projet.

---

## 10. Carnet de bord — états de référence

État valide constaté à la fin de la phase 2 (juin 2026) :

```
cam-1 : EN LIGNE  IP=192.168.0.150  RSSI=-60dBm  uptime≈30min  LED=off
cam-2 : EN LIGNE  IP=192.168.0.44   RSSI=-50dBm  uptime≈15min  LED=off
cam-3 : EN LIGNE  IP=192.168.0.45   RSSI=-51dBm  uptime≈27h    LED=off
cam-4 : EN LIGNE  IP=192.168.0.16   RSSI=-51dBm  uptime≈27h    LED=off
```

Heap libre stable à ~191 Ko sur les 4 cams (aucune fuite mémoire constatée).

---

_Document généré dans le cadre du projet « Œil dans l'atelier »._
_Version du serveur MCP : v5 (juin 2026, ajout microscope USB)._
