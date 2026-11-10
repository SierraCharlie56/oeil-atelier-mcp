# Projet — Régulation température bain refroidissement tube laser 110W

> Démarré 2026-09-02

---

## Objectif

Réguler à **22°C** l'eau d'un bain tampon de 30L qui refroidit le circuit de retour d'un tube laser CO2 110W, via un **ESP32-WROOM-32**. Le système inclut aussi un **interverrouillage de sécurité** sur la ligne de sécurité du MYJG (alimentation du tube laser) : le laser ne peut fonctionner que si le refroidissement est opérationnel.

## Principe

- **Bac 30L** avec deux serpentins distincts :
  - Serpentin 1 : circuit du compresseur/chiller (refroidissement du bain)
  - Serpentin 2 : circuit de retour d'eau du tube laser (chauffe le bain, à refroidir)
- **Canne chauffante** dans le bac, pour compenser en hiver si la température descend sous la consigne.
- **ESP32-WROOM-32** pilote compresseur ET canne chauffante en tout-ou-rien via optocoupleurs **MOC3023** + triacs **BTA16-600BW** (voir [inventaire_composants.md](inventaire_composants.md)).
- **Alimentation de l'ESP32** : module secteur AC-DC **Hi-Link HLK-PM01** (230V AC → 5V DC, 3W) dans le coffret — pas de bloc USB externe (voir section dédiée, décidé 2026-09-07).
- **2× sondes DS18B20** commandées — usage prévu : une dans le bain (régulation), une sur le retour laser (surveillance delta de température / détection anomalie de circulation).
- **Relais de sécurité** en série sur la ligne de sécurité du MYJG (autorise/bloque le fonctionnement du laser selon l'état du refroidissement).
- **LED Rouge** (défaut, bloque le relais) et **LED Verte** (OK, autorise le relais) — indicateurs de sécurité physiques, indépendants de l'écran/du site web.
- **Entrée** lisant l'état du bouton ON/OFF de la découpeuse (contact mis à la masse).
- **Afficheur température entrée laser** : petit écran **OLED SSD1306 0,96" 128×64, I2C** (module séparé — voir ci-dessous, changement du 2026-09-05).
- **Site web embarqué** (protégé par mot de passe) : lecture des paramètres/E-S en temps réel + bouton de forçage de fermeture du relais.

## À faire / en attente

- [x] **Mesurer les specs du compresseur** — fait le 2026-09-02 : démarrage ~7A, fonctionnement 1,4A. Dimensionnement BTA16-600BW validé (voir ci-dessous).
- [x] **Choisir la plateforme** — décidé le 2026-09-02 : **ESP32-WROOM-32** (carte de dev en stock avec écran 1,9" intégré, cf. identification ci-dessous). Nécessaire vu le nombre d'E/S (relais sécurité, 2 LED, 2 MOC3023, 1-Wire, entrée découpeuse) — l'ESP8266 aurait obligé à mobiliser les broches de strapping boot.
- [x] **Sémantique du bouton "forcer fermeture" du site web** — décidé le 2026-09-02 : **bypass total**. Usage : opérations de maintenance nécessitant la sécurité MYJG fermée (ex. vérifier la continuité de la chaîne de sécurité — porte + autres capots en série avec ce relais — indépendamment de l'état du refroidissement). Voir garde-fous ci-dessous.
- [x] **Câblage détaillé MOC3023 → BTA16-600BW** — fait le 2026-09-02, voir schéma et valeurs ci-dessous.
- [x] **Circuit de commande du relais de sécurité** — fait le 2026-09-02, voir schéma ci-dessous.
- [ ] Vérifier le courant réel de la bobine du relais (mesure résistance bobine au multimètre) pour ajuster R_B si besoin.
- [x] **Firmware** — fait le 2026-09-03, compile sans erreur (RAM 13,9%, Flash 62,2%). Voir section ci-dessous.
- [ ] Éditer `src/secrets.h` avec les vrais identifiants WiFi/site web avant de flasher.
- [ ] Calibrer l'ordre des sondes DS18B20 sur le bus (vérifier physiquement quel index = bain / laser).
- [ ] Câblage réel + tests sur le matériel (compresseur, canne, relais, interlock, site web).
- [ ] **Alimentation ESP32 par module secteur Hi-Link HLK-PM01** (décidé 2026-09-07) — câbler +Vo→5V / -Vo→GND du DevKit, fusible temporisé + condensateur réservoir en entrée de carte, valider la tenue de la tension 5V pendant un pic d'émission WiFi avec le relais fermé (marge de courant serrée, voir section « Alimentation de l'ESP32 »). Pas encore câblé.
- [ ] **(idée à l'étude — 2026-09-04)** Extension « supervision chaîne de sécurité MYJG » (capot + débit-contact + switch laser ramenés dans l'ESP32). Rien câblé, firmware non modifié — voir section dédiée en fin de fichier. En attente de réponses matérielles (découpeuse laser pas sous la main).
- [x] **Changement de carte (2026-09-05)** : ESP32-WROOM-32 DevKit nu + écran OLED SSD1306 I2C 128×64 séparé, remplace la carte à écran ST7789 intégré endommagé. Firmware migré et recompilé (voir section Firmware). Reste : recevoir/câbler l'OLED et flasher pour valider l'affichage sur le matériel réel.

## Carte ESP32 identifiée

**Changement matériel décidé le 2026-09-05 :** abandon de la carte à écran ST7789 intégré (endommagé) au profit d'un **ESP32-WROOM-32 DevKit nu** (38 broches, USB micro + puce série intégrée, sans écran) + un petit **écran OLED SSD1306 0,96" 128×64 en I2C** séparé (~2€, très répandu, `Adafruit_SSD1306` + `Adafruit_GFX`). Confirmé sur photo (`~/Images/ESP32-2.jpg`) : **GPIO16 et GPIO17 ne sont pas sortis sur le connecteur de cette carte non plus** — même limitation que la carte précédente (`~/Images/ESP32-LCD.png`), à ne jamais réassumer disponibles sans revérifier.

**Gain de ce changement :** l'écran SPI intégré mobilisait 6 GPIO (32, 23, 18, 15, 2, 4) ; l'OLED I2C n'en utilise que 2 (SDA/SCL) — net +4 broches propres disponibles, ce qui détend nettement le budget de broches pour l'extension supervision MYJG à l'étude (voir section dédiée, mise à jour).

**Broches de l'écran OLED :**

| Fonction OLED | GPIO |
|---|---|
| SDA (I2C) | 21 |
| SCL (I2C) | 22 |

Adresse I2C standard `0x3C`, résolution 128×64 (`SCREEN_WIDTH`/`SCREEN_HEIGHT` dans `config.h`).

⚠️ L'écran reste **purement informatif** (température courante + statut lisible) — jamais utilisé pour une indication de sécurité. Les LED rouge/verte restent seules garantes du statut autorisé/bloqué, y compris avec ce nouvel écran monochrome sans code couleur.

## Alimentation de l'ESP32 (Hi-Link HLK-PM01)

Décidé le 2026-09-07. Installation fixe alimentée en permanence : on alimente l'ESP32 directement depuis le secteur du coffret avec un module AC-DC encapsulé **[Hi-Link HLK-PM01](inventaire_composants.md#hi-link-hlk-pm01--module-dalimentation-ac-dc-secteur--5v-3w)** (100-240V AC → 5V DC, 0,6A, 3W, isolé classe II) — pas de bloc USB externe à perdre ou débrancher, tout dans l'IP54.

**Câblage :**

```
   Phase 230V ──[ fusible T 250-500mA ]── HLK-PM01  AC
   Neutre 230V ─────────────────────────── HLK-PM01  AC

   HLK-PM01 +Vo ──┬── broche 5V (VIN) du DevKit ESP32
                  │                     (→ AMS1117 embarqué fabrique le 3,3V)
              [ 470-1000µF ] + [ 100nF ]   au ras de la carte
                  │
   HLK-PM01 -Vo ──┴── GND du DevKit ESP32
```

- La masse **-Vo = GND ESP32** est un rail **SELV flottant**, isolé du secteur — cohérent avec la logique d'isolation du projet. Tout ce qui touche au secteur ou à la ligne MYJG passe déjà par un MOC3023 / le relais Finder / un PC817 : **rien ne change** côté interfaces.

**Budget de courant (le point sensible) :**

| Consommateur | Courant | Note |
|---|---|---|
| ESP32-WROOM-32 | ~150-250mA moyen, **pics ~500mA** | bursts d'émission WiFi |
| Bobine relais BESTEP (rail 3,3V) | **~120mA en continu** | dès que la sécurité autorise = état normal de marche |
| 2 LED rouge/verte | ~10-20mA | |
| OLED SSD1306 | ~10-20mA | |
| LED de chaque MOC3023 | ~12mA / canal actif | compresseur et/ou canne |

Crête cumulée possible **proche voire au-delà des 600mA** du module (pic WiFi + relais fermé + un canal MOC actif).

**Conclusion / réserves :**

- Confortable pour l'ESP32 **seul** ; **serré** avec la bobine du relais en continu + les pics WiFi.
- **Obligatoire** : condensateur réservoir **≥ 470µF (viser 1000µF)** + 100nF céramique au ras de la broche 5V du DevKit — le HLK-PM01 a une boucle de régulation lente et ne suit pas les bursts WiFi.
- **À valider au banc** : mesurer la tension 5V sous charge pendant l'émission WiFi avec le relais fermé. Si elle plonge sous ~4,7V (brownout ESP32, ou le relais qui décroche), passer à un module **même empreinte plus large** : **HLK-5M05** (5W, 1A) ou **HLK-10M05** (10W, 2A). Système de sécurité près de 30L d'eau → prendre la marge d'emblée est raisonnable.
- Alternative si le budget reste trop juste : alimenter la bobine du relais 3V depuis une source séparée pour sortir ses ~120mA continus du budget du HLK-PM01.
- **Ne pas cumuler USB (flash/debug) et HLK-PM01** : couper le secteur du coffret avant de brancher l'USB pour un reflash, pour ne pas faire se rencontrer les deux sources sur le rail 5V.

## Plan de brochage ESP32

| Fonction | GPIO | Note |
|---|---|---|
| MOC3023 #1 (compresseur) | 13 | |
| MOC3023 #2 (canne chauffante) | 14 | |
| Relais sécurité MYJG | 27 | via module relais ou transistor, jamais en direct |
| LED Rouge | 26 | |
| LED Verte | 25 | |
| Bus 1-Wire (DS18B20 ×2) | 19 | + pull-up 4,7kΩ — GPIO16 indisponible sur cette carte, voir plus haut |
| Entrée état découpeuse ON/OFF | 33 | pull-up **interne** (`INPUT_PULLUP`) — contact ferme à la masse = ON → lecture LOW ; débranché = HIGH = OFF (fail-safe sans composant externe) |
| Écran OLED SSD1306 — SDA | 21 | I2C, ajouté 2026-09-05 |
| Écran OLED SSD1306 — SCL | 22 | I2C, ajouté 2026-09-05 |

**Large marge disponible** (GPIO 4, 18, 23, 32 libres et propres ; + GPIO 5, 12 en réserve strapping ; + GPIO 34, 35, 36, 39 en entrée seule) — élargie par le passage à l'OLED I2C (2 broches) qui libère les 6 broches SPI de l'ancien écran intégré. Aucune broche de strapping boot mobilisée pour les signaux ci-dessus.

**Correction 2026-09-03 :** l'entrée découpeuse a été déplacée du GPIO34 vers le **GPIO33**. GPIO34 est input-only et n'a aucune pull-up interne sur l'ESP32 — sans résistance externe câblée, la broche flotte et peut lire n'importe quoi (observé sur le banc de test : "Decoupeuse: ON" sans câble branché, alors que le fail-safe voulu est OFF). Le GPIO33 dispose d'une pull-up interne activable en logiciel (`INPUT_PULLUP`), qui garantit le bon défaut (débranché = HIGH = OFF) sans composant externe.

## Câblage détaillé MOC3023 → BTA16-600BW (×2 : compresseur + canne chauffante)

**Correction 2026-11-10 :** brochage BTA16 vérifié au multimètre sur la pièce reçue (TO-220AB isolé) : **1 = MT1, 2 (milieu) = MT2, 3 = gâchette, languette isolée**. L'ancienne version de ce schéma (gâchette = broche 2, MT2 = broche 3 + languette) était fausse. Montage MOC3023 remis selon la fiche technique : pin6 → R_G → MT2, pin4 → gâchette.

Isolation galvanique basse tension (ESP32) / secteur (230V) assurée par le MOC3023 — à respecter physiquement dans le coffret (pas de piste/fil basse tension à proximité du côté secteur).

```
                              AC LINE (Phase, 230V)
                                      │
                                   [ CHARGE ]   (compresseur ou canne)
                                      │
                    ┌─────────────────┴─────────────────┐
                    │                                     │
              BTA16 MT2 (broche 2, milieu)          R_G (voir tableau, par canal)
                    │                                     │
                    │                               MOC3023 pin6
                    │                               MOC3023 pin4
                    │                                     │
              BTA16 Gate (broche 3) ◄─────────────────────┘
                    │
              BTA16 MT1 (broche 1)
                    │
                    └───────────────────────────────────────► NEUTRE

   ESP32 GPIO (13 ou 14) ──R_LED (voir tableau)── MOC3023 pin1 (anode LED)
                                            MOC3023 pin2 (cathode) ── GND ESP32
```

**Valeurs différenciées par canal (décidé le 2026-09-05)** — deux jeux distincts plutôt qu'une valeur unique pour les deux :

| Canal | R_LED (ESP32, 3,3V) | R_G (secteur, gâchette) | Justification |
|---|---|---|---|
| Compresseur (GPIO13, charge inductive, ~7A démarrage) | **180Ω, 1/4W** (~11,7mA, ~2× l'IFT max garanti de 5 mA du MOC3023) | **330-360Ω, 1W, tenue 350V+** | Plus de courant de gâchette dispo pour le BTA16 (non logic-level), utile pour un démarrage moteur et près du zéro de la sinusoïde (MOC3023 = déclenchement en phase aléatoire, pas de zero-cross) |
| Canne chauffante (GPIO14, charge résistive) | **220Ω, 1/4W** en 3,3V (~8,5 mA ; 330Ω si un jour piloté en 5V) — *passé de 330Ω à 220Ω le 2026-11-10, voir note* | **1kΩ, 1W** | Reprise telle quelle du schéma déjà utilisé et validé sur le thermostat de chauffage de l'atelier de l'utilisateur (`Documentation/SchemaTriac.png`, même BTA16-600BW) ; charge résistive = moins critique sur la marge de courant |

**Note 2026-11-10 — R_LED canne 330Ω → 220Ω :** mesuré au banc avec 330Ω : 1,9 V aux bornes de la résistance → **~5,8 mA** dans la LED du MOC3023 (V_F ≈ 1,4 V sous 3,3 V). Fonctionne, mais à peine au-dessus de l'IFT max garanti du MOC3023 (**5 mA** — la valeur 10 mA citée auparavant est celle du MOC3022). Marge insuffisante vu le vieillissement de la LED d'un optocoupleur → 220Ω (~8,5 mA). Le schéma EasyEDA (R3 330Ω 1/4W) est à mettre à jour en conséquence.

**Nomenclature (2 canaux, valeurs différentes) :**
- Compresseur : 1× MOC3023, 1× BTA16-600BW, 1× R_LED 180Ω 1/4W, 1× R_G 330-360Ω 1W
- Canne chauffante : 1× MOC3023, 1× BTA16-600BW, 1× R_LED 220Ω 1/4W, 1× R_G 1kΩ 1W

**À commander :** R_LED 180Ω (compresseur) + R_LED 220Ω (canne) + R_G 330-360Ω 1W (compresseur) + R_G 1kΩ 1W (canne) — ne pas unifier sur une seule valeur pour les deux canaux. Vérifier au banc le démarrage du compresseur une fois câblé (absence de ronflement/hésitation).

## Circuit de commande du relais de sécurité (bobine 3V)

Un GPIO ne peut pas piloter une bobine de relais directement (courant insuffisant + pas de protection contre le pic inductif à la coupure). Transistor NPN en commutation basse tension + diode de roue libre.

**Option module tout fait :** un [module relais 1 canal BESTEP / Songle SRD-03VDC-SL-C](inventaire_composants.md#module-relais-1-canal-bestep-relais-songle-srd-03vdc-sl-c) (en stock) intègre déjà transistor de commande, diode de roue libre et LED d'état — il remplace le circuit discret ci-dessous. Bobine 3V compatible rail 3,3V. Réserves : conso bobine ~120mA (plus que l'hypothèse ~75mA du discret), fail-safe à valider au banc via le cavalier high/low (IN bas/flottant → bobine non alimentée), contact sur COM + NO.

**Rappel logique fail-safe :** GPIO HIGH → transistor conduit → bobine alimentée → contact NO fermé → ligne MYJG autorisée. GPIO LOW/flottant/reset → bobine non alimentée → contact ouvert → laser bloqué par défaut.

```
                    +3,3V (rail ESP32)
                         │
                    ┌────┴────┐
                    │ Bobine  │  relais 3V
                    │ relais  │
                    └────┬────┘
                         │
              ┌──────────┤◄── cathode (bague) de D1 (1N4007) ici
              │          │    D1 en // sur la bobine, anode côté collecteur
              │          │
              │     Collecteur (NPN, ex. 2N2222 / BC337)
              │          │
   ESP32 GPIO27──R_B──── Base
        (470-680Ω)       │
                     Émetteur ─── GND
                         │
              R_BE (10kΩ) entre Base et GND
              (pull-down anti-flottement au boot)
```

**Valeurs :**

| Composant | Valeur | Rôle |
|---|---|---|
| Transistor | NPN générique (2N2222, BC337, 2N3904...) | Commutation bobine (typiquement 30-80mA pour un petit relais 3V) |
| R_B (base) | 470-680Ω, 1/4W | Sature le transistor avec marge (calcul : Ib=Ic/β_forcé, β_forcé≈20) |
| R_BE (base-émetteur) | 10kΩ, 1/4W | Maintient le transistor bloqué si le GPIO flotte au démarrage — important pour le fail-safe |
| D1 (roue libre) | 1N4007 (ou 1N4148) | Absorbe le pic inductif à la coupure — cathode côté +3,3V |

**À vérifier :** courant réel de la bobine (résistance mesurée au multimètre ou valeur imprimée type "DC3V XXΩ") pour ajuster R_B si besoin — calcul ci-dessus basé sur une hypothèse ~75mA.

**Amélioration possible (optionnelle) :** si le relais est SPDT (contact NC disponible), le câbler vers une entrée ESP32 libre (ex. GPIO35) pour que le firmware vérifie que le relais a réellement changé d'état — utile pour détecter un contact collé sur une ligne de sécurité.

## Sécurité — logique fail-safe du relais

Le relais de la ligne de sécurité MYJG doit être **ouvert par défaut** (non alimenté = sécurité active) : toute coupure de courant, crash logiciel, reset ou watchdog doit laisser le circuit en position "arrêt", jamais fermé par défaut. Le circuit de commande (transistor/module relais) doit être conçu pour qu'un GPIO flottant ou à l'état bas au démarrage n'active pas le relais.

## Bouton "forcer fermeture" (site web) — bypass total

Le relais thermique n'est qu'un maillon de la chaîne de sécurité MYJG (en série avec la porte et d'autres capots). Le bouton de forçage sert à fermer ce relais pour tester le reste de la chaîne en maintenance, indépendamment de l'état du refroidissement. C'est un **bypass total** : il ignore même un dépassement réel de seuil de température.

**Garde-fous à implémenter côté firmware :**

- **Pas de persistance au reboot** — l'état bypass ne doit jamais être sauvegardé en flash ; à chaque redémarrage de l'ESP32, on repart toujours en mode protégé.
- **Timeout automatique** — le bypass se désactive tout seul après un délai (ex. 10-15 min), pour ne pas rester actif indéfiniment après la maintenance.
- **Indicateur visible pendant le bypass** — ex. LED rouge clignotante (motif différent du "défaut" fixe) tant que le bypass est actif, pour ne jamais confondre "sécurité OK" et "sécurité forcée".

## Firmware (2026-09-03)

Code source : `/mnt/nas-documents/_CH/PlatformIO/Projets(linux)/laser_bain_regulation/` (PlatformIO, ESP32-WROOM-32, framework Arduino) — copie de sauvegarde/référence.

⚠️ **Compiler depuis la copie locale**, pas depuis le NAS : `/home/ch/PlatformIO-local/laser_bain_regulation/`. Le dossier `.pio` (cache de build) corrompt les archives sur ce montage SMB (erreur `file format not recognized` rencontrée le 2026-09-03) — build local confirmé propre (RAM 13,9%, Flash 62,2%).

**Essai matériel du 2026-09-03 (premier flash) :** ✅ validé sur banc de test (réseau/mot de passe provisoires, sondes DS18B20 pas encore reçues).
- Écran illisible au premier essai (texte dans la zone cassée) — corrigé en reprenant `rotation=2` + `setViewport(0,0,170,200)` de l'ancien projet `esp_lcd_1.9` (au lieu de `rotation=0` sans viewport). Confirmé lisible après reflash.
- Comportement fail-safe confirmé conforme : sans sonde connectée, écran affiche "Bain: ---", "Laser: ---" et "BLOQUE" en rouge (LED rouge fixe) — le firmware coupe bien tout et garde le relais ouvert en cas de défaut sonde.

**Choix d'architecture :**

- **WebServer synchrone** (bibliothèque standard ESP32), pas d'async — trafic faible, plus simple à maintenir/déboguer pour un système de sécurité.
- **Authentification HTTP Basic** sur toutes les routes web — suffisant pour un réseau local de confiance, non exposé à internet.
- **Identifiants dans `src/secrets.h`** (non versionné, `.gitignore`) — jamais de mot de passe en clair dans le code. Copier `secrets.h.example` et renseigner les vraies valeurs avant de flasher.
- **Écran OLED SSD1306 via `Adafruit_SSD1306`/`Adafruit_GFX`** (I2C, `Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL)`) — remplace depuis le 2026-09-05 l'ancien écran ST7789 SPI intégré (`TFT_eSPI`) ; simple déclaration en `lib_deps`, plus besoin de `build_flags` dédiés dans `platformio.ini`.

**Schéma EasyEDA Pro vérifié le 2026-09-05** (`~/Images/SCH_Schematic1.png`, projet `Refroidissement_bain_laser`) : vérification faite en lisant directement les données du projet EasyEDA (fichier `.eprj` SQLite, pas juste l'image PNG rendue — le rendu PNG seul était ambigu à cause de croisements de fils sans point de jonction). Résultat : câblage **conforme au firmware sur tous les points critiques** — OLED SDA=GPIO21, SCL=GPIO22, VCC=3V3, GND=masse ; bus 1-Wire=GPIO19 avec pull-up R5 4,7kΩ vers 3V3, les deux DS18B20 en parallèle (VDD/GND/DQ corrects) ; relais sécurité GPIO27→header "Relay BESTEP MYJG" ; canne=GPIO14, compresseur=GPIO13 ; LED verte=GPIO25, rouge=GPIO26 ; valeurs des résistances R1-R4 conformes (1kΩ1W, 330Ω1W, 330Ω1/4W, 180Ω1/4W). Deux broches libres (GPIO18, GPIO23) sorties vers des headers "Signaux Annexes" de réserve — cohérent avec le budget de broches prévu pour l'extension MYJG à l'étude.

⚠️ **Détail cosmétique repéré (sans impact, rien à corriger) :** le symbole EasyEDA "ESP32 Devkit V1.1" utilisé nomme la broche physique n°20 "D1" au lieu de "D4" (erreur de la bibliothèque de symbole, pas de l'utilisateur) — cette broche n'est pas utilisée dans ce schéma donc aucune conséquence, mais à garder en tête si un jour câblée : elle correspond à GPIO4, pas GPIO1.

**Migration écran 2026-09-05 (ESP32 nu + OLED SSD1306 I2C, remplace l'écran ST7789 intégré) :** `platformio.ini` (lib_deps + suppression des `build_flags` TFT_eSPI), `config.h` (ajout `PIN_OLED_SDA`=21, `PIN_OLED_SCL`=22, `OLED_I2C_ADDR`=0x3C, `SCREEN_WIDTH`/`SCREEN_HEIGHT`=128/64) et `main.cpp` (`updateDisplay()` réécrite pour l'API Adafruit_GFX, `setup()` avec `Wire.begin()` + `display.begin()` non bloquant en cas d'échec — l'écran reste informatif, une panne ici ne doit jamais toucher à la sécurité). Recompilé avec succès (RAM 14,4%, Flash 64,5%, quasi identique à avant). Sources locales et copie NAS synchronisées. **Reste à faire :** flasher la carte et vérifier visuellement l'affichage sur l'OLED réel (positionnement du texte jamais testé sur écran physique, seulement en compilation).

**Logique implémentée :**

- Régulation hystérésis à deux bandes + anti-cycling compresseur (4 min mini entre démarrages, 2 min mini avant arrêt) — voir `updateRegulation()`.
- Interlock sécurité : relais fermé uniquement si sonde bain valide ET température < `SAFETY_MAX_BATH_TEMP_C` (26°C par défaut) — voir `updateInterlock()`.
- Bypass maintenance total, jamais persisté en flash (variable RAM uniquement), timeout auto 15 min, LED rouge clignotante distincte pendant l'activation.
- LED rouge/verte pilotées uniquement par l'état réel de sécurité (jamais par l'écran, endommagé).
- Site web (`/`, `/bypass/on`, `/bypass/off`) : page auto-rafraîchie (5s), bouton bypass avec confirmation JS explicite.
- Écran : affichage informatif température/statut uniquement.

**Avant de flasher :**

1. Éditer `src/secrets.h` (SSID/mot de passe WiFi + mot de passe site web).
2. Calibrer `IDX_SONDE_BAIN`/`IDX_SONDE_LASER` dans `config.h` — l'ordre des sondes sur le bus 1-Wire n'est pas garanti à l'avance, à vérifier en réchauffant une sonde et observant quel index bouge (moniteur série ou écran).

## Specs compresseur mesurées (2026-09-02)

| | Valeur mesurée | Marge vs BTA16-600BW (16A) |
|---|---|---|
| Courant de démarrage (pointe) | ~7A | ×2,3 |
| Courant en fonctionnement | 1,4A | ×11 |

**Conclusion :** BTA16-600BW largement dimensionné, pas de changement de triac nécessaire. Suffixe **BW** = version snubberless (haute tolérance au dv/dt de commutation) → pas de réseau RC snubber requis pour cette charge inductive. Dissipateur thermique non indispensable vu le courant continu de 1,4A (marge de sécurité possible si disponible).

## Points de conception validés

### Hystérésis à deux bandes séparées (éviter conflit chauffage/refroidissement)

| Actionneur | Seuil ON | Seuil OFF |
|---|---|---|
| Refroidissement (compresseur) | > 22,5°C | < 22°C |
| Chauffage (canne) | < 21°C | > 21,5°C |

Zone morte entre 21,5°C et 22°C où rien ne fonctionne.

### Protection anti-cycling compresseur

Un compresseur ne supporte pas les cycles marche/arrêt rapprochés (stabilisation du fluide frigorigène nécessaire). **Délai minimum obligatoire entre deux démarrages** (typiquement 3-5 min), à coder côté firmware indépendamment de l'hystérésis de température — sous peine d'usure prématurée voire de casse du compresseur.

### Choix MOC3023 vs MOC3041

- **MOC3023** (non zero-crossing) : adapté à la charge inductive du compresseur — permet une commutation en phase avec le courant plutôt qu'avec la tension.
- Canne chauffante (charge résistive) : MOC3023 fonctionne aussi, mais un MOC3041 (zero-crossing) réduirait les EMI/transitoires si besoin d'affiner plus tard.

### Sécurité électrique (non négociable)

- Différentiel 30mA obligatoire en amont.
- Coffret IP54+ minimum pour l'électronique de puissance (230V à proximité de 30L d'eau).
- Aucune connexion secteur non isolée à portée du bac.
- L'isolation galvanique du MOC3023 protège l'ESP, pas l'utilisateur — précautions physiques indispensables en plus.
- **Module d'alimentation secteur (Hi-Link HLK-PM01)** : entrée au potentiel réseau — fusible temporisé 250-500mA en série sur la phase au plus près du module, broches AC capotées/inaccessibles, distances d'isolement respectées. La masse -Vo (= GND ESP32) reste un rail SELV flottant, jamais reliée à la terre ni au secteur.

## Composants utilisés (voir fiches détaillées dans l'inventaire)

- [MOC3023](inventaire_composants.md#moc3023--optocoupleur-sortie-triac-sans-zcd) — driver optocoupleur TRIAC
- [BTA16-600BW](inventaire_composants.md#bta16-600bw--triac-16a--600v) — TRIAC de puissance
- [Module relais 1 canal BESTEP (Songle SRD-03VDC-SL-C)](inventaire_composants.md#module-relais-1-canal-bestep-relais-songle-srd-03vdc-sl-c) — candidat relais de sécurité MYJG (bobine 3V, SPDT)
- [Relais Finder 55.34.8.230.0040](inventaire_composants.md#finder-553482300040--relais-embrochable-4-inverseurs-4rt-bobine-230v-ac) — isolation 230V pour la détection ON/OFF découpeuse (voir section dédiée)
- [Hi-Link HLK-PM01](inventaire_composants.md#hi-link-hlk-pm01--module-dalimentation-ac-dc-secteur--5v-3w) — alimentation secteur 230V AC → 5V DC de l'ESP32 (voir section « Alimentation de l'ESP32 »)
- Écran OLED SSD1306 0,96" 128×64 I2C — ajouté 2026-09-05, remplace l'écran ST7789 intégré endommagé (à ajouter à l'inventaire si besoin)
- DS18B20 (2×) — sondes température 1-Wire, commandées, fiche donnée en conversation (à ajouter à l'inventaire si besoin)

## Détection ON/OFF découpeuse via relais Finder (isolation 230V)

Décidé le 2026-09-03. L'entrée `PIN_DECOUPEUSE` (GPIO33, `INPUT_PULLUP`) attend un **contact sec vers la masse** : fermé à GND = LOW = découpeuse ON ; ouvert = HIGH = OFF (fail-safe sans composant externe). Plutôt que de câbler en direct un contact de la machine (potentiel inconnu, possible switching de phase), on interpose un [relais Finder 55.34.8.230.0040](inventaire_composants.md#finder-553482300040--relais-embrochable-4-inverseurs-4rt-bobine-230v-ac) comme **barrière d'isolation galvanique**.

**Câblage :**

```
   230V AC "découpeuse en marche"          ESP32
   (// bobine du contacteur machine)
            │                               GPIO33 ──────┐
       ┌────┴────┐                                       │ NO (borne 5)
       │ Bobine  │ Finder A1/A2 (13/14)          ┌───────┴───────┐
       │ 230V AC │                               │  1 pôle Finder │
       └────┬────┘                               │  (inverseur)   │
            │                                    └───────┬───────┘
   Découpeuse ON  → bobine excitée → NO fermé            │ COM (borne 9)
   Découpeuse OFF → bobine relâchée → NO ouvert       GND ESP32
```

- Découpeuse ON → NO fermé → GPIO33 à la masse → **LOW = ON** ✅
- Découpeuse OFF / hors tension / relais débranché → NO ouvert → GPIO33 tiré à 3,3V par la pull-up interne → **HIGH = OFF** ✅ (fail-safe conservé)
- **Aucune modification firmware** : la sémantique « contact vers GND » est exactement celle codée pour GPIO33.

**Réserves :**

- **Vérifier la tension de commande de la découpeuse avant de câbler** : la bobine du Finder est en **230V AC**. Si la machine pilote son contacteur en 24V, ce relais ne convient pas. Prendre le 230V en parallèle sur la bobine du contacteur machine, **jamais sur les phases moteur en direct**.
- **Commutation « sèche »** : le contact 7A AgNi ne verra qu'environ **70 µA** (3,3V / ~45 kΩ de pull-up interne) — régime *dry circuit*, risque d'oxydation / intermittence à long terme. Acceptable pour un compteur d'usure « confort ». Pour fiabiliser : mettre **deux pôles du Finder en parallèle**, ou préférer un [PC817](inventaire_composants.md#pc817--optocoupleur-simple-canal-sortie-transistor) / un relais à contacts dorés.
- **Ligne GPIO33 dans le coffret** (nœud haute impédance qui côtoie du 230V) : ajouter un **pull-up externe 10 kΩ vers 3,3V** + **100 nF vers GND** au ras de la broche pour éviter les LOW parasites par couplage capacitif — même logique que la correction GPIO34→GPIO33.
- **Détection de contact collé** (optionnel) : câbler le pôle **NC** du Finder vers un GPIO input-only libre (34/35/36/39) pour repérer un « découpeuse ON permanent » qui fausserait le compteur d'usure (sans incidence sur la sécurité, l'interlock MYJG étant sur une chaîne séparée).

## Compteur d'utilisation découpeuse (usure tube laser)

Ajouté le 2026-09-03, décidé avec le collègue. Objectif : suivre le temps de fonctionnement cumulé du tube laser via l'entrée `PIN_DECOUPEUSE` (GPIO33), pour anticiper son remplacement.

- **Temps cumulé ON** : compté par pas de 2s (résolution du cycle de régulation), persisté en NVS (flash interne, survit aux coupures). Écriture flash économisée : sauvegarde seulement à l'arrêt de la découpeuse, et toutes les 5 min si une session dure longtemps (`DECOUPEUSE_AUTOSAVE_MS`) — pas à chaque cycle de 2s, pour ne pas user la flash.
- **Bouton RAZ** (site web, authentifié, confirmation JS) : remet le cumul à zéro. **Ne touche pas** l'historique des connexions (utile même après un changement de tube). À utiliser uniquement lors du changement de tube laser.
- **Historique des 10 dernières connexions** (tampon circulaire en NVS) : horodatage à chaque front OFF→ON de la découpeuse. Nécessite une horloge murale — synchro NTP (`pool.ntp.org` / `time.google.com`, fuseau Europe/Paris avec bascule heure d'été/hiver auto) lancée une fois le WiFi connecté. Si le NTP n'a jamais répondu au moment d'une connexion, l'entrée s'affiche "heure inconnue" plutôt que de planter.
- Impact mémoire négligeable (10× uint32 + 2 octets ≈ 42 octets en NVS) — testé : compile OK, RAM 14,3%, Flash 64,3%.
- **Durée par session ajoutée le 2026-09-03** (suite tests réels sur banc, découpeuse câblée sur GPIO33) : chaque entrée de l'historique affiche aussi sa durée, mise à jour en direct pendant que la session est en cours (marquée "(en cours)"), figée en NVS à l'arrêt (front ON→OFF). Bug d'affichage corrigé au passage : `formatDuration()` n'affichait que h/min, arrondissant les tests courts à "0h 00min" — les secondes sont maintenant affichées.
- **Correction 2026-09-03 (comptage conditionné à la sécurité) :** le comptage (cumul + historique) ne se déclenche désormais que si la découpeuse est ON **et** que la sécurité autorise réellement (`safetyGreen` = LED verte fixe allumée, rouge éteinte — donc hors bypass maintenance aussi). Avant ce correctif, un signal découpeuse ON pendant que la sécurité bloquait (bain en défaut, surchauffe, etc.) aurait compté à tort du temps d'usure alors que le tube ne peut pas physiquement fonctionner dans cet état. `updateInterlock()` doit désormais s'exécuter avant `updateDecoupeuseTimer()` dans `loop()` — ne pas réinverser cet ordre.
- **Validé sur banc le 2026-09-03** : sans sondes DS18B20 (sécurité bloquée en permanence, fail-safe), la découpeuse ON ne déclenche plus aucune entrée ni cumul — comportement attendu confirmé. RAZ du compteur testé OK. Test du cas "comptage réel pendant sécurité autorisée" impossible tant que les DS18B20 ne sont pas branchées.

**Why:** le tube laser CO2 est un consommable dont la durée de vie s'exprime en heures d'utilisation ; sans compteur, aucun moyen de savoir quand une baisse de puissance est due à l'usure du tube.

**How to apply:** ne pas resynchroniser trop souvent la flash (usure NVS) — le compromis "sauvegarde à l'arrêt + toutes les 5 min en continu" est un choix déjà tranché, à ne pas remplacer par une écriture à chaque cycle de 2s.

## Extension envisagée — module de supervision de la chaîne de sécurité laser

> Proposé le 2026-09-04. **Statut : étude seulement. Rien n'est câblé, le firmware n'est pas modifié.** En attente de réponses matérielles — la découpeuse laser n'est pas sous la main pour l'instant.

### Idée

Réutiliser l'ESP32 déjà en place (régulation + interlock thermique) pour en faire aussi un **point unique de surveillance de toute la chaîne de sécurité** de la découpeuse : ramener l'état de chaque contact (capot, débit-contact, switch « laser ») dans l'ESP32, l'afficher sur le site web, et pouvoir dire *lequel* est ouvert quand la machine est bloquée.

### État existant sur la machine

La découpeuse a déjà une **chaîne de sécurité câblée en dur** : `SW LASER` + `Flow` (débit-contact) + `SW DOOR` (capot) **en série**, contacts secs, un seul ouvert = tout s'arrête. Cette chaîne tire la ligne de protection du MYJG vers la masse.

### Doc MYJG retrouvée (NAS, 2026-09-04)

- `/mnt/nas-documents/_CH/FabLab/LaserRouge/Materiel/MYJG/Manuel_MYJG_FR.pdf` (+ `.odt` + version anglaise `Manuel_MYJG.pdf`) — dupliquée aussi dans `.../FabLab/temp/2025/MYJG/` et `.../FabLab/Laser/LaserRouge/`.
- Schéma de câblage main : `/mnt/nas-documents/_CH/FabLab/Laser/LaserRouge/Shema.pdf` (montre la chaîne `SW LASER` + `Flow` + `SW DOOR` sur le bornier de commande ; le bloc 24 V n'alimente que la partie mouvement).
- Photos borniers : `/mnt/nas-documents/_CH/FabLab/LaserRouge/Images/` (`MYJG.png`, `Bornier.svg`, `Switch.png`…).

**Bornier de commande MYJG** (2ᵉ banque, 6 bornes) : `H  L  P  G  IN  5V`

| Borne | Rôle |
|---|---|
| L | *laser signal* — défaut « No Laser signal » ; à tirer vers `G` pour autoriser |
| P | *water protection* — défaut « No water protection » ; à tirer vers `G` pour autoriser |
| G | masse de référence de la commande |
| 5V | 5 V pour signaux numériques |
| H / IN | connexion actif-haut / entrée décalage PWM-potentiomètre |

### Tension de la ligne de sécurité

**~5 V, référencée à la masse `G` du MYJG, courant très faible** (pull-up interne, quelques mA au plus). **Pas** du 24 V — le bloc 24 V du schéma n'alimente que la partie mouvement (carte BAZYX + driver DM542 + moteur NEMA). Pas du secteur.

⚠️ Les manuels du NAS sont titrés MYJG-**80W**/60W et une photo dit 100W — le brochage `H/L/P/G/IN/5V` est identique sur toute la gamme, l'info tient. **À confirmer au multimètre sur l'unité réelle** : tension `P`–`G` et `L`–`G` contacts ouverts, avant de dimensionner l'isolation.

### Architecture retenue (si le projet se fait)

**La chaîne câblée reste intacte.** L'ESP32 ne fait que **lire** chaque contact ; son relais de sécurité reste **un maillon série de plus**, jamais la seule grille. Un ESP32 planté (relais collé) ne peut alors pas autoriser le tir capot ouvert — la chaîne série physique tient toujours.

Option écartée : ESP32 comme seule grille (imposerait un *readback* obligatoire du relais + watchdog — trop critique pour un tube 110 W).

### Raccordement — NE PAS dériver les contacts direct sur GPIO

5 V sur la masse du MYJG vs 3,3 V sur la masse ESP32 : un contact ouvert enverrait ~5 V sur une broche 3,3 V, et il faudrait bonder les masses à côté d'une alim 28 kV. **Isoler chaque contact** :

1. **Pôle sec libre par appareil** — si le débit-contact (souvent inverseur), le micro-switch capot et le switch laser ont un contact inutilisé : câblage direct vers l'ESP32 en `INPUT_PULLUP`, chaîne MYJG intouchée. Le plus simple — **à vérifier en premier**.
2. **Sinon : un PC817 par nœud**, alimenté depuis les bornes `5V`/`G` du MYJG (pas en parasitant le maigre pull-up de `P`). LED de l'opto sur un nœud de la chaîne, côté transistor vers un GPIO `INPUT_PULLUP`. Isolation galvanique complète — même logique que le relais Finder utilisé pour la détection découpeuse. ~3-4 PC817 + résistances.

Dans les deux cas, sémantique fail-safe habituelle : contact fermé = LOW = condition OK ; ouvert / fil coupé = HIGH = défaut.

### Budget broches ESP32

✅ **Mis à jour 2026-09-05** (passage à l'OLED I2C, voir plus haut) : budget détendu. GPIO16/17 restent indisponibles sur cette carte, GPIO19 est pris par le 1-Wire et GPIO21/22 par l'OLED, mais l'abandon de l'écran SPI intégré libère 4 broches propres supplémentaires. Propres en `INPUT_PULLUP` (non strapping, non input-only) : **GPIO 4, 18, 23, 32** → 4 entrées, suffit pour `SW LASER` + `Flow` + `SW DOOR` + `P` sans avoir besoin des broches input-only. GPIO 34/35/36/39 restent en réserve si besoin (pull-up externe 10kΩ déjà prévu de toute façon). GPIO 5 en réserve seulement (strapping). **GPIO 12 à éviter** (strapping, pull-up interne = HIGH au boot = risque de blocage).

Filtrage au ras de chaque broche : **10 kΩ vers 3,3 V + 100 nF vers GND** (lignes qui côtoient du 230 V et une alim HT) — même précaution que la correction GPIO33/GPIO34.

### Affichage web (à ajouter)

- État par contact : `Capot : FERMÉ / OUVERT`, `Débit : OK / NUL`, `Switch laser : OK / OUVERT`.
- Ligne de synthèse : `Autorisation sécurité : OUI / NON — cause : capot ouvert | débit nul | switch laser | surchauffe bain | sonde HS | bypass`.
- Anti-rebond asymétrique : passage en défaut immédiat, retour en OK seulement après ~1-2 s stables.
- Bonus : réutiliser le tampon circulaire NVS de la découpeuse pour horodater les N derniers déclenchements sécurité (« pourquoi le laser a coupé »).

### En attente avant d'aller plus loin

- [ ] Mesurer `P`–`G` et `L`–`G` (contacts ouverts) sur le MYJG réel.
- [ ] Vérifier si capot / débit-contact / switch laser ont un pôle sec libre.
- [ ] Trancher l'architecture (surveillance seule vs. participation active) — a priori surveillance seule + relais maillon série.
- [ ] Découpeuse laser pas sous la main pour l'instant.

---

*Fichier de suivi projet — mis à jour au fil de l'avancement.*
