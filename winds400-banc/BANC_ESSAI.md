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

| Fil   | Couleur | Capteur              | Signal mesuré                              |
|-------|---------|----------------------|--------------------------------------------|
| +V    | Rouge   | —                    | +12V DC (9-16.5V acceptés)                 |
| GND   | Noir    | —                    | Masse                                      |
| DIR   | Bleu    | Girouette            | PWM période fixe ~1280 µs, DC = angle      |
| SPD   | Vert    | Anémomètre (IR)      | Open-drain, 8 impulsions/tour (fréquence ∝ vitesse) |

### Fil Bleu — Girouette (PWM)

Signal PWM à période fixe mesurée à l'oscilloscope :
- **Période** : ~1280 µs (fixe)
- **Largeur d'impulsion** : 99 µs (min, ~0°) à 900 µs (max, ~360°)
- **Tension** : ~4.8V crête → pont diviseur 10 kΩ/15 kΩ pour ramener à 2.9V max (ESP8266 3.3V)
- **GPIO** : D2 (GPIO4 NodeMCU)
- **Formule** : `angle = (pw - pw_min) / (pw_max - pw_min) × 360`
- Auto-calibration min/max en cours d'exécution ; `/reset_calib` pour remettre à zéro

### Fil Vert — Anémomètre (IR optique)

Roue à 8 fentes — 8 fronts descendants par tour :
- **Type** : open-drain, pull-up 10 kΩ externe vers 3.3V
- **Niveaux** : 3.295V (pale devant faisceau) / 0.7V (fente ouverte)
- **GPIO** : D4 (GPIO2 NodeMCU) — ⚠ doit être HIGH au boot (pull-up branché)
- **ISR** : comptage FALLING, `IRAM_ATTR`
- **Calcul** : `spd_hz = pulses / s` (impulsions/s) ; `rpm = spd_hz × 60 / 8`

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

FIL BLEU (girouette — PWM ~4.8V crête)
  fil Bleu ──┬── 10 kΩ ──┬── D2 (GPIO4)
             │           │
            15 kΩ      (entrée ESP, 2.9V max)
             │
            GND

FIL VERT (anémomètre — open-drain)
  3.3V ── 10 kΩ ──┬── D4 (GPIO2)
                  │
  fil Vert ───────┘  (drain collecteur ouvert, tiré vers 3.3V)

ESP8266 NodeMCU v2
┌──────────────────────┐
│  D2 (GPIO4) ─────────┼── fil Bleu  via pont div 10k/15k
│  D4 (GPIO2) ─────────┼── fil Vert  via pull-up 10k → 3.3V
│  USB ────────────────┼── PC (flash + debug)
└──────────────────────┘
  AP WiFi : WindS400-Banc / wind1234
  IP      : 192.168.4.1
```

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

| Route            | Réponse                                      |
|------------------|----------------------------------------------|
| `GET /`          | Page HTML avec compas animé + vitesse vent   |
| `GET /data`      | JSON mis à jour chaque seconde               |
| `GET /reset_calib` | Remet pw_min/pw_max à zéro (recalibration girouette) |

Exemple de réponse `/data` :
```json
{
  "angle":  245.0,
  "pw_us":  650,
  "pw_min": 99,
  "pw_max": 900,
  "spd_hz": 157.73,
  "rpm":    1183.0,
  "spd_ms": 4.89,
  "valid":  true,
  "age":    1
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

## Calibration anémomètre — 29/06/2026

### Méthode
Sèche-cheveux comme source de flux concentré + anémomètre de référence en vis-à-vis.
8 points mesurés sur la plage 2.8–6.4 m/s, relevés en tr/min sur la page web.

### Données brutes

| m/s (réf) | tr/min (WindS400) | SPD_K calculé | Statut |
|-----------|-------------------|---------------|--------|
| 2.8       | 337               | 0.0623        | écarté (seuil démarrage) |
| 3.1       | 426               | 0.0546        | écarté (seuil démarrage) |
| 3.5       | 800               | 0.0328        | utilisé |
| 3.9       | 1032              | 0.0283        | utilisé |
| 4.2       | 874               | 0.0360        | **outlier écarté** (RPM < 3.9 m/s) |
| 4.8       | 1183              | 0.0304        | utilisé |
| 5.7       | 1365              | 0.0313        | utilisé |
| 6.4       | 1500              | 0.0320        | utilisé |

> Le point 4.2 m/s → 874 tr/min est physiquement impossible (RPM inférieur au point 3.9 m/s) ;
> instabilité du sèche-cheveux lors de cette mesure.

### Résultat

```
SPD_K = 0.031  (m/s par Hz impulsions)
spd_ms = spd_hz × SPD_K
```

Erreur max ±3% sur la plage 4.8–6.4 m/s. Plage basse (< 4 m/s) non fiable par cette méthode.

**Formule de recalibration :**
```
SPD_K = m/s_réf × 7.5 / tr/min_WindS400
        (7.5 = 60 s / 8 fentes)
```

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
