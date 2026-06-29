# Diagnostic & Banc d'essai — WindS400 Advansea / PLASTIMO

Girouette/anémomètre marine, protocole RS485 / NMEA 0183.

> **Contexte** : L'indicateur a été déposé pour **absence d'indication girouette**.
> L'anémomètre (vitesse vent) semble fonctionnel. Ce document sert à la fois
> au diagnostic de la panne et à la construction d'un banc d'essai de remplacement.

| Champ            | Valeur                  |
|------------------|-------------------------|
| Marque commerciale | Advansea (WindS400)   |
| Fabricant PCB    | **PLASTIMO** (France)   |
| Référence PCB    | `24-21-331-001`         |
| Marquage dessous | `PLASTIMO / 24-21-331-001` |

---

## Inventaire des composants identifiés au microscope

| Marquage        | Identification                          | Rôle                          |
|-----------------|-----------------------------------------|-------------------------------|
| `4.7-25L / ND`  | Condensateur tantale/électrolytique 4.7µF 25V (Nichicon) | Découplage alimentation |
| `2951 / OMC`    | LP2951 (régulateur LDO 100mA ajustable) | Génération 3.3V ou 5V interne |
| `316BCG`        | PIC16Fxxx probable (microcontrôleur Microchip QFN) | Traitement capteurs + NMEA |
| `ST / DE`       | Diode ST SMA — protection ESD/TVS       | Protection lignes RS485 A/B   |
| `A7G / E2`      | Régulateur LDO SOT-23-5 (MIC5205 ou XC6206) | Alimentation secondaire  |
| `102`           | Résistance 1 kΩ (code SMD)              | Pull-up/pull-down RS485       |

---

## Architecture du système WindS400

```
TÊTE DE MÂT (PCB analysé)              AFFICHEUR (dans le bateau)
┌──────────────────────────┐           ┌──────────────────────┐
│  Girouette (potentio.)   │           │                      │  NMEA 0183
│  Anémomètre (IR optique) │  Bus AS-1 │  Indicateur          │ ──────────► ESP / charteur
│  Microcontrôleur         │ ────────► │  vent/vitesse        │  4800 bauds
│  + LP2951 + transistors  │  38400 bd │                      │
└──────────────────────────┘           └──────────────────────┘
   Rouge/Noir = 12V alim
   Bleu  = direction vent (girouette)
   Vert  = vitesse vent (anémomètre IR)
```

> La sortie NMEA 0183 est produite par **l'afficheur**, pas par la tête de mât.
> Pour un banc d'essai avec seulement la tête de mât, il faut lire le bus AS-1
> ou les signaux bruts Bleu/Vert directement.

## Capteurs de la tête de mât

| Fil   | Couleur | Capteur              | Type de signal attendu          |
|-------|---------|----------------------|---------------------------------|
| +V    | Rouge   | —                    | +12V DC (9-16.5V acceptés)      |
| GND   | Noir    | —                    | Masse                           |
| DIR   | Bleu    | Girouette            | Analogique (potentiomètre) ou impulsions |
| SPD   | Vert    | Anémomètre (IR)      | Train d'impulsions (fréquence ∝ vitesse) |

### Émetteur/récepteur IR (dessous PCB)
- Compte les tours de l'anémomètre par interruption du faisceau IR
- Génère un **train d'impulsions** sur le fil Vert : plus le vent est fort, plus la fréquence est élevée
- Visible à l'oscilloscope en faisant tourner l'anémomètre à la main

## Protocole de communication (bus AS-1 vers afficheur)

- **Interface** : Bus AS-1 propriétaire Advansea — half-duplex, **38400 bauds**, 8N1
- **Alimentation** : 9V à 16.5V DC (typiquement 12V), < 150mA, fusible 1A
- **Trame NMEA** (disponible uniquement en sortie de l'afficheur) :

```
$WIMWV,245.0,R,12.3,M,A*checksum\r\n
         │   │  │   │ └─ A=valide / V=invalide
         │   │  │   └─── unité : M=m/s  N=nœuds  K=km/h
         │   │  └─────── vitesse du vent
         │   └────────── R=apparent / T=réel
         └────────────── direction en degrés (0-359)
```

---

## Schéma de câblage banc d'essai

```
ALIMENTATION 12V
┌──────────────┐
│  12V DC 1A   ├─── Rouge ──────────────── fil Rouge WindS400 (+12V)
│              ├─── GND ────────────────── fil Noir  WindS400 (GND)
└──────────────┘        │
                        └── GND commun ESP8266

LECTURE SIGNAUX CAPTEURS
                        fil Bleu (girouette) ───────── GPIO input ESP  (ou oscillo CH1)
                        fil Vert (anémomètre) ──────── GPIO input ESP  (ou oscillo CH2)

ESP8266 (optionnel — pour lire les signaux bruts)
┌──────────────────────┐
│  GPIO4  ─────────────┼── fil Bleu  (direction)
│  GPIO5  ─────────────┼── fil Vert  (vitesse IR)
│  3.3V + diviseur R   │   ⚠ mettre diviseur 12V→3.3V si signal en 12V
│  USB ────────────────┼── PC (debug)
└──────────────────────┘

MODULE MAX3485 (si on veut décoder le bus AS-1 vers l'afficheur)
→ à brancher entre les fils AS-1 (orange selon doc) et l'ESP
→ 38400 bauds, protocole à reverse-engineer
```

> ⚠ **Avant de brancher les fils Bleu/Vert sur l'ESP** : mesurer la tension au multimètre
> sous 12V d'alimentation — si signal en 12V, mettre un pont diviseur (10kΩ + 4.7kΩ) pour
> ramener à 3.3V compatible ESP8266.

---

## Matériel nécessaire

| Composant              | Référence              | Prix indicatif |
|------------------------|------------------------|----------------|
| Microcontrôleur        | ESP8266 NodeMCU v2     | ~3 €           |
| Module RS485           | MAX3485 breakout 3.3V  | ~1 €           |
| Alimentation           | 12V DC 1A              | ~5 €           |
| Câble                  | Dupont M/F             | —              |
| Résistance terminaison | 120 Ω (optionnel)      | —              |

> Préférer le **MAX3485** au MAX485 : il est nativement 3.3V, pas besoin de level shifter entre lui et l'ESP8266.

---

## Structure PlatformIO

```
winds400-banc/
├── platformio.ini       ← configuration build
└── src/
    └── main.cpp         ← code complet ESP8266
```

### platformio.ini

```ini
[env:esp8266]
platform  = espressif8266
board     = nodemcuv2
framework = arduino

lib_deps =
    plerup/EspSoftwareSerial @ ^8.1.0

monitor_speed = 115200
upload_speed  = 921600
```

---

## Démarrage du banc d'essai

1. Flasher l'ESP8266 : `pio run --target upload`
2. Ouvrir le moniteur série : `pio device monitor`
3. Connecter téléphone/PC au WiFi **`WindS400-Banc`** (mdp : `wind1234`)
4. Ouvrir **`http://192.168.4.1`** dans le navigateur

---

## API REST embarquée

| Route   | Réponse                                      |
|---------|----------------------------------------------|
| `GET /` | Page HTML avec compas animé                  |
| `GET /data` | JSON mis à jour chaque seconde           |

Exemple de réponse `/data` :
```json
{
  "dir": 245.0,
  "speed": 12.3,
  "unit": "M",
  "ref": "R",
  "valid": true,
  "age": 1
}
```

---

## Dépannage

| Symptôme                  | Cause probable              | Solution                        |
|---------------------------|-----------------------------|---------------------------------|
| Aucune trame reçue        | Mauvais baud rate           | Essayer 38400 dans `nmeaSerial.begin()` |
| Trame reçue mais invalide | Fils A/B inversés           | Permuter les fils A et B        |
| Signal instable           | Pas de résistance 120 Ω     | Ajouter 120 Ω entre A et B      |
| Page web inaccessible     | Mauvaise IP                 | Vérifier moniteur série au boot |
| ESP plante en boucle      | Conflit GPIO15 au boot      | Laisser GPIO15 flottant au flash, brancher après |

---

## Diagnostic panne girouette

### Symptôme constaté
- **Panne** : absence d'indication de direction vent sur l'afficheur
- **Anémomètre** : fonctionnel (signal visible à l'oscillo)
- **Signal girouette** : intermittent — visible à l'oscillo lors des premiers tests,
  puis disparu. Signe probable d'une connexion défaillante ou soudure froide.

### Hypothèses classées par probabilité

| Priorité | Cause probable | Test |
|----------|---------------|------|
| ⭐⭐⭐ | **Soudure froide** sur capteur Hall ou fil Bleu | Microscope sur les soudures des SOT-23 proches du fil Bleu |
| ⭐⭐⭐ | **Connexion oxydée** sur le fil Bleu (milieu marin) | Continuité ohmmètre fil Bleu bout à bout |
| ⭐⭐   | **Capteur Hall défaillant** (IC A7G/E2) | Alimentation 12V + mesure tension sur pins du SOT-23 |
| ⭐⭐   | **Alimentation interne LP2951** instable | Mesurer sortie LP2951 sous 12V → doit être stable 3.3V ou 5V |
| ⭐     | **Aimant dégaussé** en bout de cylindre | Approcher un tournevis → doit attirer |

### Procédure de diagnostic pas à pas

**Étape 1 — Vérifier l'aimant (hors tension)**
Approcher un tournevis ou vis acier de l'extrémité du cylindre côté PCB.
L'aimant doit attirer clairement. Sinon → aimant dégaussé ou déplacé.

**Étape 2 — Continuité du fil Bleu (hors tension)**
Ohmmètre entre la pointe du fil Bleu et son pad de soudure sur le PCB.
Doit faire 0 Ω. Si résistance élevée ou infinie → câble rompu ou oxydé.

**Étape 3 — Alimentation interne (sous 12V)**
Mesurer la sortie du LP2951 (marquage `2951`) directement sur le PCB.
Doit être stable à 3.3V ou 5V. Si chute ou oscillation → LP2951 ou condensateur associé défaillant.

**Étape 4 — Tension sur capteur Hall (sous 12V)**
Mesurer sur les pins du ou des petits IC SOT-23 (marquage `A7G/E2`) :
- Pin alim : doit être stable (3.3V ou 5V)
- Pin sortie : doit varier en tournant la girouette à la main

**Étape 5 — Inspection visuelle au microscope**
Examiner les soudures des composants SOT-23 et du pad fil Bleu.
Chercher : soudure terne/craquelée, microfissure, vernis décollé.

### Observations à l'oscilloscope (Jeulin 20MHz)
- Réglage utilisé : **0.2V/div**, **50µs/div**
- Signal girouette (fil Bleu) : ressemble à du PWM, tension ~0 à 4mV au multimètre
  (multimètre trop lent pour signal PWM rapide — valeur non représentative)
- Signal anémomètre (fil Vert) : train d'impulsions visible, fréquence ∝ vitesse rotation

---

## CONCLUSION DIAGNOSTIC — Cause de panne identifiée et résolue

### Cause racine : aimant décollé de son logement

**Panne mécanique, pas électronique.** Le PCB et les capteurs Hall sont intacts.

L'aimant (petit disque, magnétisation diamétrale) s'est décollé de sa cage
à l'intérieur du cylindre de la girouette. Libre de se déplacer, il ne suivait
plus la rotation de la girouette → signal absent ou intermittent sur le fil Bleu.

```
Cylindre (coupe) :

   tige girouette (inox)
          │
     ┌────┴────┐
     │  [cage] │  ← logement qui centre l'aimant automatiquement
     │  [ 🧲 ] │  ← aimant diametral sorti de son logement  ← CAUSE PANNE
     └────┬────┘
          │ joints toriques (étanchéité)
       vers PCB (Hall sensors)
```

### Procédure de réparation

1. Remettre l'aimant dans sa cage (il se centre automatiquement)
2. Sécuriser avec une micro-goutte de cyanoacrylate ou époxy bi-composant sur le bord
3. Vérifier l'état des joints toriques avant fermeture (ne pas les écraser)
4. Refermer le cylindre
5. **Tester à l'oscilloscope** (fil Bleu) avant remontage en tête de mât :
   tourner la girouette à la main → signal doit varier
6. Remonter en tête de mât
7. **Recaler le zéro** via la fonction de calibration de l'afficheur Advansea

### Notes importantes
- L'orientation rotationnelle de l'aimant dans la cage **n'est pas critique**
  car l'afficheur dispose d'une fonction de recalage du zéro
- La cage centre l'aimant automatiquement — pas de problème d'alignement
- Magnétisation **diamétrale** (pôles N/S sur les côtés du disque, perpendiculaires
  à l'axe de rotation) — c'est le montage standard pour encodeur Hall 360°

---

## Câblage officiel — manuel Advansea WindS400

> Source : manuel officiel Advansea WIND S400 (ManualsLib)

| Couleur | Signal            | Notes                                 |
|---------|-------------------|---------------------------------------|
| Rouge   | +12V alim (+)     | 9V à 16.5V DC, fusible 1A recommandé |
| Noir    | GND (−)           | Masse commune                         |
| Orange  | Bus AS-1 (×2)     | Bus propriétaire 38400 bauds half-duplex, jusqu'à 20 appareils |
| Jaune   | Entrée NMEA 0183+ | 4800 bauds, 8N1                       |
| Noir    | Entrée NMEA 0183− | (2e fil noir)                         |

> **Important** : le bus AS-1 est **propriétaire Advansea**, pas du RS485 standard.
> Pour lire les données sur ESP8266, utiliser la **sortie NMEA 0183** (fils Jaune/Noir)
> directement sur un port série TTL — pas besoin de module MAX3485.

### Correspondance avec les fils observés au microscope

Les fils **Bleu et Blanc** visibles sur `dessous_pcb02.png` correspondent probablement
aux deux fils du **bus AS-1 Orange** (couleurs câble interne ≠ couleurs doc officielle)
ou à la sortie NMEA. À confirmer à la mise sous tension.

---

*Relevé PCB effectué au microscope Dino-Lite — juin 2026*
