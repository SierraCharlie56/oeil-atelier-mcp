# Projet — Régulation température bain refroidissement tube laser 150 W crête / 130 W continu

> Démarré 2026-09-02

---

## Objectif

Réguler à **22°C** l'eau d'un bain tampon de 30L qui refroidit le circuit de retour d'un tube laser CO2 **150 W crête / 130 W continu** (et non plus 110 W, précisé le 2026-11-25 ; chaleur à évacuer ≈ 550-750 W en continu), via un **ESP32-WROOM-32**. Le système inclut aussi un **interverrouillage de sécurité** sur la ligne de sécurité du MYJG (alimentation du tube laser) : le laser ne peut fonctionner que si le refroidissement est opérationnel.

## Principe

- **Bac 30L** avec deux serpentins distincts :
  - Serpentin 1 : circuit du compresseur/chiller (refroidissement du bain)
  - Serpentin 2 : circuit de retour d'eau du tube laser (chauffe le bain, à refroidir)
- **Canne chauffante** dans le bac, pour compenser en hiver si la température descend sous la consigne.
- **ESP32-WROOM-32** pilote compresseur ET canne chauffante en tout-ou-rien via optocoupleurs **MOC3023** + triacs **BTA16-600BW** (voir [inventaire_composants.md](inventaire_composants.md)).
- **Alimentation de l'ESP32** : module secteur AC-DC **Hi-Link HLK-10M05** (230V AC → 5V DC, 2A, 10W) dans le coffret, protégé par fusible + varistance, pas de bloc USB externe (voir section dédiée ; décidé 2026-09-07 avec un HLK-PM01, passé au 10M05 le 2026-11-11).
- **2× sondes DS18B20** (reçues, testées sur banc le 2026-11-10) — usage prévu : une dans le bain (régulation), une sur le retour laser (surveillance delta de température / détection anomalie de circulation).
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
- [x] Vérifier le courant réel de la bobine du relais : **84 mA mesurés le 2026-11-11** (ampèremètre en continu, en série sur VCC du module BESTEP, relais collé, sous 3,3V ; total du module : bobine + LED d'état). Module utilisé : pas de R_B à ajuster.
- [x] **Firmware** — fait le 2026-09-03, compile sans erreur (RAM 13,9%, Flash 62,2%). Voir section ci-dessous.
- [ ] Éditer `src/secrets.h` avec les vrais identifiants WiFi/site web avant de flasher.
- [x] Calibrer l'ordre des sondes DS18B20 sur le bus (vérifier physiquement quel index = bain / laser) : **ordre vérifié correct sur banc le 2026-11-10**, `IDX_SONDE_BAIN`/`IDX_SONDE_LASER` de `config.h` inchangés. *(Remplacé le 2026-11-25 par l'adressage ROM, voir ligne suivante.)*
- [x] **Firmware revue 2026-11-25 (points 1-3) — flashé et validé au banc le 2026-11-25** (voir section Firmware, « Revue 2026-11-25 ») :
  - [x] Point 1 — flasher avec adresses factices (BLOQUÉ, « --- » sur les deux sondes, adresses imprimées sur le série) ; identifier bain/laser en chauffant une sonde à la main ; reporter les adresses ROM dans `config.h` et ci-dessous ; **étiqueter physiquement les câbles** ; reflasher ; débrancher la sonde bain seule → bain « --- », laser correcte, BLOQUÉ, compresseur et canne coupés ; puis la sonde laser seule → BLOQUÉ « SONDE LASER HS ».
  - [x] Point 2 (sauf compteur d'usure, seuil laser passé à 25 °C pour une sonde au départ) — ILS shunté → AUTORISÉ sans cause ; sonde laser > 30 °C → « SURCHAUFFE LASER » (OLED + web), ré-autorisation seulement < 29 °C ; sonde bain > 26 °C → « SURCHAUFFE BAIN », ré-autorisation < 25 °C ; compteur d'usure figé pendant ces blocages. **Valider le seuil `SAFETY_MAX_LASER_TEMP_C` (30 °C) selon l'emplacement réel de la sonde** (sortie tube ou retour vase).
  - [x] Point 3 — compiler avec `TEST_WATCHDOG`, mettre en AUTORISÉ avant 90 s → reset en ≤ ~5 s, relais ouvert, LED rouge, web « Dernier redémarrage : WATCHDOG (tache) » ; **vérifier que le module relais BESTEP reste retombé pendant le reset/boot** (GPIO27 haute impédance, cavalier high/low) ; recompiler **sans** `TEST_WATCHDOG` et reflasher.
- [ ] Hors périmètre de la revue 2026-11-25, à traiter plus tard : `requestTemperatures()` bloquant ~750 ms (débit non relu pendant ce temps → `setWaitForConversion(false)` + lecture différée) ; capteur de niveau bas du vase d'expansion (GPIO 4/23/32, `INPUT_PULLUP` + 10 kΩ / 100 nF) dans l'interlock et coupant la pompe ; mot de passe solide du site web au déploiement (il commande le bypass total).
- [x] **Tests basse tension sur banc — validés le 2026-11-10** : ESP32, OLED SSD1306, 2 sondes DS18B20, LED verte et rouge, module relais de sécurité, LED des 2 optocoupleurs MOC3023 (côté commande). Toute la partie basse tension fonctionne.
- [ ] Câblage et tests de la partie **secteur** : BTA16 + compresseur et canne, alimentation HLK-10M05 (pièces en commande), interlock sur la ligne MYJG réelle, site web en conditions réelles.
- [ ] **Alimentation ESP32 par module secteur Hi-Link HLK-10M05** (décidé 2026-09-07, PM01 → 10M05 le 2026-11-11) : **commandé le 2026-11-11** : HLK-10M05, fusible T 1A 250V Littelfuse TE5 `392 1100 0000` (+ support TE5 éventuel), varistance 10D471K, 2× 100nF céramique, 1000µF 10V low-ESR 105°C, 10µF. Puis câbler selon le schéma de la section « Alimentation de l'ESP32 » et vérifier VIN > 4,7V pendant un pic WiFi avec le relais fermé. Pas encore câblé.
- [ ] **(idée à l'étude — 2026-09-04)** Extension « supervision chaîne de sécurité MYJG » (capot + débit-contact + switch laser ramenés dans l'ESP32). Rien câblé, firmware non modifié — voir section dédiée en fin de fichier. En attente de réponses matérielles (découpeuse laser pas sous la main).
- [ ] **Contacteur de débit eau laser sur GPIO18** (ajouté le 2026-11-23, voir section dédiée) : firmware **flashé le 2026-11-23** (démarre OK, site web affiche la ligne Débit ; ILS pas encore câblé → « NUL », BLOQUE comme attendu). Reste : pull-up externe 10 kΩ + 100 nF au ras de GPIO18, test sur banc (ILS à la main), puis essai en eau (seuil + papillonnement).
- [x] **PCB finalisés et vérifiés le 2026-11-23** (3 cartes : automate, alimentation, affichage — voir section « PCB »). **DRC EasyEDA validée le 2026-11-24** (4 écarts d'isolement MOC acceptés). **Gerber vérifiés et PCB commandés le 2026-11-24.**
- [x] **OLED SSD1306 : chauffait fort sur le banc le 2026-11-25 — résolu le même jour.** Cause : **GND et VCC inversés** au recâblage en l'air. Ce module a l'ordre **GND – VCC – SCL – SDA** (de gauche à droite, face aux composants), alors que beaucoup de modules ont VCC en premier : toujours lire la sérigraphie. Module intact (1,3 MΩ / 0,8 MΩ entre GND et VCC selon le sens), reste froid une fois rebranché correctement, l'init I2C réussit. Probable cause de la déconnexion du hub USB à 15:25 (non prouvée).
- [ ] Tester le compteur d'usure figé pendant un blocage (GPIO33 à la masse + sonde chauffée).
- [ ] **Essais en eau avec relevés manuels** pour valider les plages théoriques (écart bain → départ laser sous charge, tenue du compresseur, blocages intempestifs) : fiche imprimable [fiche_releves_essais_eau.pdf](fiche_releves_essais_eau.pdf) (source HTML à côté), préparée le 2026-11-25.
- [ ] **`secrets.h` à refaire au déploiement** : le système sera installé **sur un autre site** (précisé le 2026-11-25) → nouveau WiFi (SSID + mot de passe) **et** nouveaux identifiants du site web. Le mot de passe web actuel du banc est identique au nom d'utilisateur par défaut : en choisir un solide, il commande le bypass total.
- [ ] Réception des PCB → montage, test du contacteur de débit sur banc, essais secteur avec lampe en charge.
- [x] **Changement de carte (2026-09-05)** : ESP32-WROOM-32 DevKit nu + écran OLED SSD1306 I2C 128×64 séparé, remplace la carte à écran ST7789 intégré endommagé. Firmware migré et recompilé (voir section Firmware). Fait : OLED câblé, carte flashée et affichage validé sur le matériel réel le 2026-11-10.

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

## Alimentation de l'ESP32 (Hi-Link HLK-10M05)

Décidé le 2026-09-07 (HLK-PM01), **passé au HLK-10M05 le 2026-11-11**. Installation fixe alimentée en permanence : on alimente l'ESP32 directement depuis le secteur du coffret avec un module AC-DC encapsulé **[Hi-Link HLK-10M05](inventaire_composants.md#hi-link-hlk-10m05--module-dalimentation-ac-dc-secteur--5v-10w)** (100-240V AC → 5V DC, **2A, 10W**, isolé), pas de bloc USB externe à perdre ou débrancher, tout dans l'IP54.

**Pourquoi le 10M05 plutôt que le [HLK-PM01](inventaire_composants.md#hi-link-hlk-pm01--module-dalimentation-ac-dc-secteur--5v-3w) en stock :** avec le PM01 (0,6A), la crête cumulée (pic WiFi + bobine relais en continu + un canal MOC) arrivait au niveau de sa limite (voir budget ci-dessous). Système de sécurité près de 30L d'eau : on prend la marge d'emblée (×3) au lieu de la valider au banc. ⚠️ Le 10M05 est **plus gros** que le PM01 : prévoir sa place dans le coffret.

**Schéma de câblage (mis à jour 2026-11-11) :**

```
 ENTRÉE SECTEUR (protection)                       HLK-10M05
                                                 ┌───────────┐
 Phase 230V ──[ F1 fusible T 1A 250V ]──┬────────┤ AC (L)    │
                                        │        │           │
                                  [ RV1 varistance            │
                                    10D471K ]    │           │
                                        │        │           │
 Neutre 230V ───────────────────────────┴────────┤ AC (N)    │
                                                 │           │
                                                 │       +Vo ├──┬──────┬────────── fil court ──────────┬──────┬──── VIN (5V) DevKit ESP32
                                                 │           │  │      │                               │      │     (→ AMS1117 embarqué : 3,3V)
                                                 │           │ C1     C2 (+)                          C3     C4 (+)
                                                 │           │ 100nF  1000µF 10V                      100nF  10µF
                                                 │           │ céra   électrolytique                  céra   (céra X5R/X7R
                                                 │           │  │      │ (−)                           │      │  ou électrolytique)
                                                 │       -Vo ├──┴──────┴──────────────────────────────┴──────┴──── GND DevKit ESP32
                                                 └───────────┘
                                                  ◄── au ras du HLK ──►               ◄── au pied de VIN/GND de l'ESP ──►
```

**Côté secteur (entrée HLK) :**

- **F1, fusible temporisé T 1A 250V**, en série sur la **phase**, **en premier** (avant la varistance), au plus près du module. Temporisé pour encaisser l'appel de courant à la mise sous tension (charge des condensateurs internes du module).
  - **Référence retenue : Littelfuse TE5 série 392, 1,00 A → `392 1100 0000`** (subminiature carré, temporisé, 250V, IEC 60127-3, pas 5,08mm, traversant). Pouvoir de coupure 25A @ 250V AC : adapté derrière le disjoncteur + différentiel 30mA de l'atelier. Déclassement en coffret chaud (~0,7A tenus en continu) très au-dessus des ~0,1A consommés par le HLK. Soudure au fer seulement (350°C ≤ 3 s, pas de refusion), ou sur support TE5 enfichable pour pouvoir le changer. Acheter du Littelfuse d'origine (Mouser/Farnell/RS) : c'est un organe de sécurité secteur, se méfier des copies.
- **RV1, varistance 10D471K** (Ø10mm, 470V, soit environ 300V AC en continu), **en parallèle entre L et N, après le fusible**. Elle écrête les surtensions (foudre, commutation du compresseur sur le même réseau). Placée après F1 : si elle claque en court-circuit en fin de vie, c'est le fusible qui saute, pas le disjoncteur de l'atelier.
- Fusible et varistance sont les protections recommandées par Hi-Link dans le circuit d'application du module.

**Côté 5V : deux groupes de condensateurs, tous en parallèle sur le rail +5V/GND :**

| Repère | Valeur | Emplacement | Rôle |
|---|---|---|---|
| C1 | 100nF céramique | au ras des broches +Vo/-Vo du HLK | parasites HF du découpage du module |
| C2 | **1000µF 10V** électrolytique, **low-ESR, 105°C** | au ras du HLK, à côté de C1 | réservoir : appels de courant (pics WiFi, collage du relais) |
| C3 | 100nF céramique | **au pied de VIN/GND du DevKit** | découplage HF local de l'ESP32 |
| C4 | 10µF (céramique X5R/X7R ou électrolytique ≥ 10V) | **au pied de VIN/GND du DevKit**, à côté de C3 | réservoir local : couvre l'inductance du fil HLK → ESP pendant les bursts WiFi (cause classique de resets) |

- **Polarité des électrolytiques** (C2, et C4 s'il est électrolytique) : (+), patte longue, côté +5V ; la bande du boîtier, côté GND.
- Les 100nF (C1, C3) au **plus près** des broches, boucle la plus petite possible, sinon ils ne servent à rien.
- 105°C pour C2 : il est collé au module, qui chauffe ; un 85°C vieillirait vite dans le coffret fermé.
- Si l'ESP est à quelques cm seulement du HLK, C3/C4 restent conseillés (peu coûteux).

- La masse **-Vo = GND ESP32** est un rail **SELV flottant**, isolé du secteur, cohérent avec la logique d'isolation du projet. Tout ce qui touche au secteur ou à la ligne MYJG passe déjà par un MOC3023, le relais Finder ou un PC817 : **rien ne change** côté interfaces.

**Budget de courant :**

| Consommateur | Courant | Note |
|---|---|---|
| ESP32-WROOM-32 | ~150-250mA moyen, **pics ~500mA** | bursts d'émission WiFi |
| Bobine relais BESTEP (rail 3,3V) | **84mA mesurés en continu** (2026-11-11 ; ~120mA estimés auparavant) | dès que la sécurité autorise, donc état normal de marche |
| 2 LED rouge/verte | ~10-20mA | |
| OLED SSD1306 | ~10-20mA | |
| LED de chaque MOC3023 | ~6-12mA / canal actif | compresseur et/ou canne |

Crête cumulée de l'ordre de **600-700mA** : à la limite du HLK-PM01 (0,6A), **large marge avec le HLK-10M05 (2A)**.

**Réserves :**

- La marge côté 5V ne règle pas le rail **3,3V** : tout ce qui est sur 3,3V (ESP32 + bobine relais + LED + OLED) passe par l'AMS1117 du DevKit (~1A max, dissipation (5−3,3) × I). Ça reste correct (~0,7W en crête), mais c'est lui le maillon limitant désormais.
- **À vérifier au banc quand même** : tension VIN pendant l'émission WiFi avec le relais fermé (doit rester > ~4,7V).
- **Ne pas cumuler USB (flash/debug) et HLK** : couper le secteur du coffret avant de brancher l'USB pour un reflash, pour ne pas faire se rencontrer les deux sources sur le rail 5V.

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
| Contacteur de débit eau laser (ILS NO) | 18 | `INPUT_PULLUP` + 10 kΩ/100 nF externes — ILS fermé (débit) = LOW = OK ; ouvert / fil coupé = HIGH = BLOQUE. Ajouté 2026-11-23, broche déjà sortie sur le header « Signaux Annexes » du schéma |
| Écran OLED SSD1306 — SDA | 21 | I2C, ajouté 2026-09-05 |
| Écran OLED SSD1306 — SCL | 22 | I2C, ajouté 2026-09-05 |

**Large marge disponible** (GPIO 4, 23, 32 libres — 18 pris par le débit le 2026-11-23 et propres ; + GPIO 5, 12 en réserve strapping ; + GPIO 34, 35, 36, 39 en entrée seule) — élargie par le passage à l'OLED I2C (2 broches) qui libère les 6 broches SPI de l'ancien écran intégré. Aucune broche de strapping boot mobilisée pour les signaux ci-dessus.

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

**Toutes en stock (2026-11-11) :** R_LED 180Ω 1/4W (compresseur) + R_LED 220Ω 1/4W (canne) + R_G **330Ω 1W** (compresseur) + R_G **1kΩ 1W** (canne). Ne pas unifier sur une seule valeur pour les deux canaux. R_G impérativement en 1W (tenue aux ~325V crête du secteur, une 1/4W standard ne tient en général que 250V). Vérifier au banc le démarrage du compresseur une fois câblé (absence de ronflement/hésitation).

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

**Copie versionnée sur GitHub depuis le 2026-11-25 :** [`firmware/laser_bain_regulation/`](firmware/laser_bain_regulation/) (dépôt **public**) — `platformio.ini`, `src/main.cpp`, `src/config.h`, `src/secrets.h.example`. **`secrets.h` n'est jamais versionné** (`.gitignore` du dépôt et du projet). La copie de travail reste `PlatformIO-local` : après chaque modification, recopier `platformio.ini` et `src/` (sauf `secrets.h`) vers le NAS **et** vers le dépôt.

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

**Revue 2026-11-25 (claude.ai, voir [revue_firmware_2026-11-25.md](revue_firmware_2026-11-25.md)) — points 1, 2 et 3 implémentés, compilés (RAM 14,4 %, Flash 64,6 %), flashés et validés au banc le 2026-11-25, copie NAS synchronisée.**

- **Point 1 — sondes adressées par ROM** (critique). `getTempCByIndex()` refait une recherche sur le bus à chaque appel : si la sonde bain se débranchait, l'index 0 désignait la sonde laser → régulation et interlock sur la mauvaise sonde, sans aucun défaut détecté. Remplacé par `ADDR_SONDE_BAIN` / `ADDR_SONDE_LASER` (8 octets) dans `config.h` + `readProbe()` (NaN si absente, CRC faux, ou hors plage `PROBE_MIN_VALID_C`..`PROBE_MAX_VALID_C` = -10..80 °C, ce qui exclut le 85,0 °C de reset). Adresses actuelles = **factices** → les deux sondes sont lues absentes → BLOQUÉ (fail-safe) tant qu'elles ne sont pas relevées. `printSensorAddresses()` imprime au boot les adresses présentes sur le bus et indique si les sondes configurées sont PRÉSENTES/ABSENTES.
  - **Adresses ROM relevées au banc le 2026-11-25** (sonde bain chauffée à la main : 17,7 → 31,3 °C, l'autre stable à 21,25 °C) — bain : `{0x28, 0x30, 0x4F, 0xD3, 0x00, 0x00, 0x00, 0xFC}` ; laser : `{0x28, 0x4F, 0xE3, 0xC9, 0x00, 0x00, 0x00, 0x71}`. Reportées dans `config.h`, reflashé : les deux sondes sont PRÉSENTES. ⚠️ Étiqueter physiquement les câbles.
  - **Hystérésis bain vérifiée au banc le 2026-11-25** (sonde bain refroidissant à l'air après chauffe à la main) : BLOQUÉ « SURCHAUFFE BAIN » à 29,4 puis 26,9 °C, encore BLOQUÉ au relevé suivant (~25,2 °C estimé, < 26 °C), ré-autorisé à 24,9 °C (< 25,0 °C). Conforme. Banc sans OLED (module débranché, chauffait fort : câblage VCC/GND à vérifier).
  - **Test clé point 1 validé au banc le 2026-11-25** : branche DQ de la sonde bain seule coupée → bain « ERREUR SONDE », **laser toujours lue (21,3 °C)**, BLOQUÉ « SONDE BAIN HS », compresseur et canne OFF. ⚠️ Premier essai : coupure faite sur le tronçon commun → 0 sonde sur le bus (BLOQUÉ quand même, fail-safe) ; la coupure doit se faire **après** l'embranchement des DQ, la 4,7 kΩ restant côté GPIO19.
  - **Test inverse validé le 2026-11-25** : branche DQ de la sonde laser seule coupée → laser « ERREUR SONDE », bain toujours lu (19,2 °C), BLOQUÉ « SONDE LASER HS » (point 2) ; la régulation continue normalement sur la sonde bain (canne ON car bain < 21 °C, compresseur OFF).
  - **Surchauffe laser + hystérésis validées le 2026-11-25** (sonde laser chauffée à la main jusqu'à 34,1 °C) : BLOQUÉ « SURCHAUFFE LASER » ; au refroidissement, encore BLOQUÉ à 30,0 / 29,6 / 29,3 °C, ré-autorisé à 28,9 °C (< 29,0 °C). Conforme. Priorité d'affichage vérifiée au passage : la sonde bain, chauffée aussi par manipulation, a affiché « SURCHAUFFE BAIN » tant qu'elle était en surchauffe (bain prioritaire), la cause est passée à « SURCHAUFFE LASER » dès la ré-autorisation du bain (< 25 °C). Reste du point 2 non testé : compteur d'usure figé pendant les blocages (nécessite l'entrée découpeuse GPIO33 à la masse).
  - **Watchdog validé au banc le 2026-11-25** (build `TEST_WATCHDOG`, système AUTORISÉ avant 90 s) : blocage de `loop()` à millis = 90001, déclenchement du TWDT **4,99 s** plus tard (chronométré à l'horloge du PC), redémarrage à 5,24 s, cause affichée « WATCHDOG (tache) » au boot. **Relais BESTEP vu retombé** pendant le reset (constaté par Christian), LED rouge puis retour à AUTORISÉ. ⚠️ L'horodatage `E (100736)` du log ESP est décalé d'environ 10 s par rapport à `millis()` : ne pas s'en servir pour mesurer le délai. Le bloc `TEST_WATCHDOG` imprime désormais `millis()` au moment du blocage (actif seulement en build de test). Reflashé **sans** `TEST_WATCHDOG` (chaîne absente du binaire, 110 s sans blocage).
  - **Libellés du site web modifiés le 2026-11-25** : « Température eau bain » et « Température eau laser » (au lieu de « Temperature bain » / « Temperature retour laser »). La sonde laser sera collée **sur le départ** (eau qui entre dans le tube), pas sur le retour.
  - **Affichage OLED réorganisé le 2026-11-25** (module bicolore : bande **jaune = 16 premières lignes de pixels**, le reste bleu) : AUTORISE / BLOQUE en taille 2 (16 px), centré dans la bande jaune ; en bleu dessous : `Bain:` (y=22), `Laser:` (y=36), cause du blocage ou BYPASS ACTIF (y=54). Validé visuellement par Christian sur le module réel.
  - ⚠️ **Seuil `SAFETY_MAX_LASER_TEMP_C` à revoir** : 30 °C avait été proposé pour une sonde en sortie de tube. Sur le départ, l'eau est proche de la température du bain (~22 °C) : 30 °C ne se déclencherait que si le bain était déjà très hors plage (cas déjà couvert par le seuil bain de 26 °C). **Décidé le 2026-11-25 : 25 °C** (blocage > 25 °C, ré-autorisation < 24 °C) — si la régulation tient le bain à 22 °C, le départ doit rester sous 25 °C. Flashé et vérifié (AUTORISÉ, laser 22,3 °C). ⚠️ À valider en eau sous charge réelle (découpe longue à 130 W) : si l'écart bain → départ via le serpentin dépasse ~2,5 °C, risque de blocage en pleine découpe (sûr mais gênant).
- **Point 2 — sonde laser dans l'interlock + hystérésis + cause.** Nouveau seuil `SAFETY_MAX_LASER_TEMP_C` = 30 °C (à valider au banc ; **passé à 25 °C le 2026-11-25**, sonde au départ) ; hystérésis `SAFETY_TEMP_HYST_C` = 1 °C sur les deux seuils (blocage au-dessus du seuil, ré-autorisation sous seuil − 1 °C : plus de claquement du relais autour de 26,0 °C). Sonde laser HS = BLOQUÉ. Variable `blockReason` (priorité : SONDE BAIN HS > SONDE LASER HS > SURCHAUFFE BAIN > SURCHAUFFE LASER > PAS DE DEBIT > DEBIT: STABILISATION) affichée sur la dernière ligne de l'OLED et sur le web (« Cause : … »). Texte de confirmation du bypass mis à jour. Compteur d'usure inchangé (dépend toujours de `safetyGreen`).
- **Point 3 — watchdog sur `loop()`** (critique). Avant : si `loop()` se figeait relais fermé, GPIO27 restait HIGH indéfiniment, laser autorisé sans surveillance. Maintenant `enableLoopWDT()` en fin de `setup()` (après le WiFi bloquant ≤ 15 s). **Vérifié dans le core utilisé** (framework-arduinoespressif32 3.20017 = core 2.0.17) : `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5` et `CONFIG_ESP_TASK_WDT_PANIC=y` → un `loop()` bloqué > 5 s provoque bien un **reset** (pas seulement un message). Pire tour de `loop()` normal ≈ 0,8 s. Cause du dernier redémarrage (`esp_reset_reason()`) imprimée sur le série et affichée sur le web, en rouge si watchdog/panic/brownout. Option de test `TEST_WATCHDOG` (commentée dans `config.h`, **ne jamais laisser active**) : fige `loop()` 90 s après le boot. Effet accepté : un reset watchdog perd au plus 5 min de cumul du compteur d'usure.

**Logique implémentée :**

- Régulation hystérésis à deux bandes + anti-cycling compresseur (4 min mini entre démarrages, 2 min mini avant arrêt) — voir `updateRegulation()`.
- Interlock sécurité : relais fermé uniquement si sonde bain valide ET température < `SAFETY_MAX_BATH_TEMP_C` (26°C par défaut) — voir `updateInterlock()`. *Depuis le 2026-11-25 : + sonde laser valide et < `SAFETY_MAX_LASER_TEMP_C` (25 °C depuis le 2026-11-25), hystérésis 1 °C, débit OK (2026-11-23).*
- Bypass maintenance total, jamais persisté en flash (variable RAM uniquement), timeout auto 15 min, LED rouge clignotante distincte pendant l'activation.
- LED rouge/verte pilotées uniquement par l'état réel de sécurité (jamais par l'écran, endommagé).
- Site web (`/`, `/bypass/on`, `/bypass/off`) : page auto-rafraîchie (5s), bouton bypass avec confirmation JS explicite.
- Écran : affichage informatif température/statut uniquement.

**Avant de flasher :**

1. Éditer `src/secrets.h` (SSID/mot de passe WiFi + mot de passe site web).
2. Renseigner `ADDR_SONDE_BAIN`/`ADDR_SONDE_LASER` dans `config.h` (depuis le 2026-11-25, remplace `IDX_SONDE_*`) — adresses imprimées sur le moniteur série au boot ; identifier laquelle est le bain en réchauffant une sonde à la main.

## PCB — 3 cartes (vérifiés le 2026-11-23)

Projet EasyEDA Pro `Refroidissement_bain_laser` (`~/Documents/EasyEDA-Pro/projects/`). Vérification faite par lecture directe d'un export `.epro2` (le `.eprj2` « live » garde ses modifications récentes dans un historique chiffré, illisible hors EasyEDA → **toujours exporter un `.epro2`** pour une vérification : il contient un `.epru` texte). Netlist reconstruite par géométrie (schéma) et par pastilles (PCB), puis comparée au firmware. Version validée : export du 2026-11-23 16:38.

### Découpage

| Carte | Contenu | Liaisons |
|---|---|---|
| **Automate** | ESP32 DevKit 30 broches sur 2 barrettes femelles 15 broches (rangées à 25,4 mm), module relais BESTEP, connecteurs capteurs | CN1 (6 br.) → alim ; CN4 (6 br.) → affichage |
| **Alimentation** | HLK-10M05 + fusible + varistance, 2 × MOC3023 + BTA16, borniers secteur | CN1 (6 br.) ← automate |
| **Affichage** | OLED SSD1306, LED verte/rouge + 150 Ω | U1 (6 br.) ← automate |

**Connecteurs inter-cartes (JST ZR)** — même brochage des deux côtés, **câbles droits obligatoires (broche 1 → broche 1)** ; un câble « inversé » mettrait VIN sur GND :
- CN1 automate ↔ alim : 1-2 = VIN-ESP32, 3 = D13 (compresseur), 4 = D14 (canne), 5-6 = GND.
- CN4 automate ↔ affichage : 1 = GND, 2 = 3,3 V, 3 = SCL, 4 = SDA, 5 = D25 (LED verte), 6 = D26 (LED rouge).

### Carte automate — connecteurs

| Connecteur | Brochage | GPIO |
|---|---|---|
| CN2, CN3 (DS18B20) | 1 GND, 2 DQ, 3 3,3 V ; pull-up R1 4,7 kΩ | 19 |
| CN5 (découpeuse) | 1 GND, 2 signal ; R2 10 kΩ + C1 100 nF | 33 |
| CN7 (débit ILS) | 1 signal, 2 GND ; R3 10 kΩ + C4 100 nF | 18 |
| CN6 (ligne MYJG) | 1 COM, 2 NO du relais (NC non utilisé) | relais sur 27 |
| VIN | C3 100 nF + C2 10 µF au pied de l'ESP32 | — |

Module relais alimenté en 3,3 V par le régulateur de l'ESP32 (84 mA mesurés).

### Carte alimentation — valeurs finales par voie

| Voie | Commande | R_LED (MOC) | MOC | R_G (gâchette) | Triac | Bornier |
|---|---|---|---|---|---|---|
| **Compresseur** | D13 | **R5 = 180 Ω** 1 W (PR01) | U1 | **R3 = 330 Ω** 3 W (MOR03) | Q1 | **P3 « COMP »** |
| **Canne** | D14 | **R4 = 220 Ω** 1/4 W (MFR-25FBF52-220R) | U3 | **R2 = 1 kΩ** 3 W (ROX3) | Q2 | **P2 « CHAUF »** |

- Montage MOC : broche 6 directement sur MT2, broche 4 → R_G → gâchette (résistance déplacée côté gâchette par rapport à la fiche technique — même boucle série, électriquement équivalent).
- HLK-10M05 : +VO → VIN-ESP32, −VO → GND ; C1 100 nF + C2 1000 µF en sortie.
- Entrée P1 : **L sur P1.2 → fusible T 1 A (U4) → HLK**, varistance 10D471K **après** le fusible (entre la sortie du fusible et N).
- ⚠️ **Les triacs commutent le neutre** (MT2 sur N, charges entre L et MT1) : une charge à l'arrêt reste reliée à la phase → **couper le secteur avant d'intervenir sur le compresseur ou la canne**, pas seulement la commande. (Avec une prise française, le sens phase/neutre n'est de toute façon pas garanti.)
- Pas de fusible propre aux charges sur la carte : protection par le disjoncteur amont + différentiel 30 mA.

### Points de fabrication (carte alimentation)

- **Pistes** : puissance 1,35–2,3 mm, gâchettes/côté MOC 0,6 mm, GND/VIN 0,6 mm. Règles de piste par classe de nets (`POWER` 1,0/2,5/5,0 mm, `SECTEUR_SIGNAL`, `BASSE_TENSION`) ; attention, un réglage net par net dans Net Rule l'emporte sur celui de la classe. Le nom de règle « 2oz » n'est qu'un nom : l'épaisseur de cuivre se choisit à la commande (**1 oz suffit**).
- **BTA16** (pattes mesurées 0,9 × 0,5 mm) : trous **oblongs 1,2 × 0,8 mm**, grand côté dans le sens de l'entraxe ; pastilles 1,8 × 2,0 mm (0,3 mm de cuivre mini, 0,74 mm entre pastilles).
- **Fente de 4 mm sous les MOC3023** (x 66,5–70,5 mm, y 35–68 mm), ~0,9 mm des pastilles des supports, aucune piste ne la traverse.
- **Isolement 230 V / BT : 5,82 mm mini** = limite du boîtier MOC3023 (broche 6 ↔ broche 1), rallongé par la fente. La DRC (règle 6 mm entre classes) le signale : accepté (voir « DRC EasyEDA »).
- Pas de dissipateur sur les BTA16 : OK pour le compresseur (1,4 A) ; à prévoir si la canne dépasse ~500 W (languette isolée → dissipateur commun possible).

### DRC EasyEDA (validée le 2026-11-24)

Règles en place :
- Règle de piste par classe (voir ci-dessus). **GND et VIN-ESP32 sont dans la classe `Basse_tension`** : les pistes existantes gardent leur largeur de 0,6 mm. Les connecteurs ont 2 fils par signal parce que l'AWG28 seul ne passe pas assez de courant.
- **Isolement : règle Net-Net `ISOLEMENT_230V` de 6 mm**, uniquement entre les classes `POWER`/`SECTEUR_SIGNAL` et `Basse_tension`. Elle ne doit pas aller dans le tableau Safe Spacing par défaut, sinon elle s'applique partout.
- **Net Length Tolerance neutralisée** sur `Basse_tension` : la règle par défaut (±25,4 mm) créait de fausses erreurs, et cette vérification ne sert à rien sur cette carte.

Résultat final : **4 erreurs, toutes acceptées** :

| Objets | Distance | Justification |
|---|---|---|
| U1_6 ↔ U1_1 et U3_6 ↔ U3_1 (même boîtier) | 5,82 mm | Écart des pattes du MOC3023, impossible à agrandir ; la fente rallonge le chemin en surface. |
| Pistes e1834 / e1837 (`$1N2026`, broche 6) ↔ U1_1 / U3_1 | 5,905 mm | Moins pénalisant que l'écart interne du boîtier. Option : faire partir la piste tout droit vers le côté secteur sur 1–2 mm avant de tourner. |

Avant de commander, vérifier dans la vue du fabricant : fente de 4 mm bien découpée, trous oblongs des BTA16 métallisés, contours des 3 cartes fermés. Réglages : 1,6 mm, 1 oz, HASL sans plomb.

### Gerber et commande (2026-11-24)

Les fichiers sont à côté du projet EasyEDA, dans `~/Documents/EasyEDA-Pro/projects/` : `Gerber_PCB1_2026-11-24.zip` (automate), `Gerber_PCB-alimentation_2026-11-24.zip`, `Gerber_PCB-affichage_2026-11-24.zip`. Commande passée le 2026-11-24.

Contrôle de la carte alimentation : contour fermé de 110 × 80 mm ; fente de 4 × 33 mm sous les MOC3023, dessinée dans le contour (donc découpée) ; 6 trous oblongs BTA16 de 0,8 × 1,2 mm dans le perçage des trous métallisés ; vias de 0,3 mm ; trou de fixation de 4,4 mm non métallisé.

Contrôle des cartes automate et affichage (2026-11-24) :
- **Automate** : contour fermé de 150 × 66 mm ; 4 trous de fixation de Ø4,2 mm à 3,5 mm des coins (cuivre le plus proche à ≥ 2,7 mm) ; 4 trous de module de Ø3,0 mm en rectangle de 19,2 × 44 mm ; 78 trous métallisés de 0,7 à 2,0 mm ; 4 vias.
- **Affichage** : contour fermé de 74,9 × 39,9 mm ; 4 trous de fixation de Ø4,4 mm (cuivre le plus proche à ≥ 2,7 mm) ; 4 trous de Ø2,3 mm pour l'OLED 0,96" (23,3 × 23,8 mm) ; 18 trous métallisés.

**⚠ Précautions de montage :**
- **Automate** : les 2 trous de Ø3,0 mm du bas (y = 55 mm) sont à **0,5 mm d'une piste côté soudure**. Une rondelle ou un écrou M3 (Ø5,5 mm environ) appuierait sur le vernis au-dessus de la piste. → Vis ou rondelles **en nylon**, ou entretoise fine côté soudure, ou tête de vis côté composants.
- **Affichage** : le trou OLED en haut à droite (x = 52,2 mm) est à **0,6 mm d'une piste côté soudure** (un écrou M2 déborde d'environ 0,85 mm). → Écrou ou rondelle **en nylon**.
- **Affichage** : les 4 trous de Ø4,4 mm ne forment pas un rectangle exact (67,4 × 33,0 mm d'un côté, 66,8 × 31,5 mm de l'autre). → Tracer les perçages du boîtier **à partir de la carte reçue**.

### Défauts trouvés et corrigés pendant la revue (2026-11-23)

D13/D14 inversés entre les deux CN1 ; sortie HLK inversée (+VO sur GND) ; LED de l'affichage montées à l'envers ; pistes secteur 0,25–0,5 mm ; pastilles BTA16 plus petites que le trou ; varistance avant le fusible ; pistes secteur puis GND passant sous les MOC (isolement 3,15–4,76 mm) ; valeurs du schéma ne correspondant plus à la sérigraphie ; R5 liée à une référence fournisseur 1 Ω ; trous BTA16 trop serrés (1,0 × 0,6 puis 0,9 mm rond).

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
- **Module d'alimentation secteur (Hi-Link HLK-10M05)** : entrée au potentiel réseau : fusible temporisé T 1A en série sur la phase au plus près du module, varistance 10D471K entre L et N en aval du fusible, broches AC capotées/inaccessibles, distances d'isolement respectées. La masse -Vo (= GND ESP32) reste un rail SELV flottant, jamais reliée à la terre ni au secteur.

## Composants utilisés (voir fiches détaillées dans l'inventaire)

- [MOC3023](inventaire_composants.md#moc3023--optocoupleur-sortie-triac-sans-zcd) — driver optocoupleur TRIAC
- [BTA16-600BW](inventaire_composants.md#bta16-600bw--triac-16a--600v) — TRIAC de puissance
- [Module relais 1 canal BESTEP (Songle SRD-03VDC-SL-C)](inventaire_composants.md#module-relais-1-canal-bestep-relais-songle-srd-03vdc-sl-c) — candidat relais de sécurité MYJG (bobine 3V, SPDT)
- [Relais Finder 55.34.8.230.0040](inventaire_composants.md#finder-553482300040--relais-embrochable-4-inverseurs-4rt-bobine-230v-ac) — isolation 230V pour la détection ON/OFF découpeuse (voir section dédiée) **Embase à commander : Finder 94.04SMA** (bornes à vis, rail DIN 35 mm ou fixation vissée sur panneau, livrée avec l'étrier métallique de maintien contre les vibrations). Option : module de protection de bobine Finder série 99.02 en 230V AC (RC + LED), enfichable dans l'embase.
- [Hi-Link HLK-10M05](inventaire_composants.md#hi-link-hlk-10m05--module-dalimentation-ac-dc-secteur--5v-10w) (à commander, 2026-11-11) : alimentation secteur 230V AC → 5V DC 2A de l'ESP32, avec fusible T 1A + varistance 10D471K en entrée (voir section « Alimentation de l'ESP32 »). Remplace le [HLK-PM01](inventaire_composants.md#hi-link-hlk-pm01--module-dalimentation-ac-dc-secteur--5v-3w) prévu initialement (marge de courant insuffisante).
- Écran OLED SSD1306 0,96" 128×64 I2C — ajouté 2026-09-05, remplace l'écran ST7789 intégré endommagé (à ajouter à l'inventaire si besoin)
- [DS18B20](inventaire_composants.md#ds18b20--sonde-de-température-numérique-1-wire) (2×) : sondes température 1-Wire, reçues et testées sur banc le 2026-11-10

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
- **Ligne GPIO33 dans le coffret** (nœud haute impédance qui côtoie du 230V) : ajouter un **pull-up externe 10 kΩ vers 3,3V** + **100 nF vers GND** au ras de la broche pour éviter les LOW parasites par couplage capacitif — même logique que la correction GPIO34→GPIO33. *10 kΩ et 100 nF commandés le 2026-11-11.*
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

## Contacteur de débit eau laser (GPIO18) — ajouté le 2026-11-23

### Identification (microscope Dino-Lite + ohmmètre, 2026-11-23)

- Étiquette : marque **新达 (Xinda)**, « **水流增压开关** » (interrupteur de débit pour surpresseur), n° `2391279-8` (lot/série). La flèche jaune indique le **sens obligatoire de passage de l'eau**.
- Démonté côté eau (côté électrique moulé dans la résine) : **palette noire pivotante portant un petit aimant + ressort de rappel inox**. Aucun axe ne traverse de joint : l'aimant agit à travers la paroi sur un ILS moulé → très bonne étanchéité, palette moins sujette au collage par le tartre qu'un piston.
- Mesures : **2 fils**, **0 Ω palette basculée**, **34,4 MΩ au repos** (= ouvert, limite du calibre) → **ILS nu, sans électronique, normalement ouvert (fermé = débit)**. Sémantique fail-safe correcte : pas de débit, fil coupé ou contacteur débranché = ouvert = BLOQUE.

### Intégration retenue : condition de l'interlock de l'ESP32

Décidé le 2026-11-23 : le contacteur est lu par une entrée libre de l'ESP32 (**GPIO18**) et intégré à la décision AUTORISE/BLOQUE, au même titre que la température du bain — il concerne la circulation de l'eau de refroidissement du laser, donc la régulation.

```
   GPIO18 (INPUT_PULLUP) ──┬── 10kΩ ──── 3,3V
                           ├── 100nF ─── GND      (les deux au ras de la broche)
                           └── ILS ───── GND      (2 fils, polarité indifférente)
```

- ILS câblé **entre GPIO18 et GND de l'ESP32 uniquement** — il n'est **pas** dans la chaîne MYJG (un même contact ne peut pas servir aux deux circuits sans casser l'isolation). C'est le relais de sécurité (GPIO27) qui transmet le blocage à la ligne `P`/`L`→`G`.
- Le débit-contact `Flow` d'origine de la machine reste en place dans la chaîne câblée (voir extension ci-dessous) : les deux se cumulent.

### Logique firmware (`PIN_DEBIT`, `FLOW_STABLE_MS` dans `config.h`)

- `readFlow()` + `updateInterlock()` appelés **à chaque tour de `loop()`** (et plus seulement toutes les 2 s) : une perte de débit coupe le relais en quelques ms.
- **Anti-rebond asymétrique** : coupure **immédiate** à l'ouverture de l'ILS ; ré-autorisation seulement après **5 s de débit continu** (`FLOW_STABLE_MS`) — évite qu'une palette qui papillonne près du seuil fasse claquer le relais.
- `safe = sonde bain OK && bain ≤ 26 °C && débit OK`. Le **bypass maintenance ignore aussi le débit** (bypass total, textes du site web mis à jour).
- Régulation thermique (compresseur/canne) **non affectée** par le débit.
- Affichage : site web `Debit eau laser : OK / stabilisation... / NUL (ou contacteur debranche)` ; OLED ligne du bas `PAS DE DEBIT` ou `Debit: stabilisation` (hors bypass).
- Compilé et **flashé le 2026-11-23** (RAM 14,4 %, Flash 64,5 %). ⚠️ Flash à 460800 bauds (`upload_speed` de `platformio.ini`) en échec ce jour-là (liaison série instable) → flashé avec esptool à 115200.

### À vérifier sur banc puis en eau

- [ ] Banc : ILS débranché → BLOQUE + « PAS DE DEBIT » ; palette basculée → AUTORISE après ~5 s ; relâchée → BLOQUE immédiat.
- [ ] En eau : contact franc au débit nominal de la pompe, ouverture si on pince le tuyau (seuil fixé par le ressort, palette raccourcissable si besoin).
- [ ] Papillonnement près du seuil ; ajuster `FLOW_STABLE_MS` si nécessaire.
- [ ] Respecter le sens de la flèche au montage ; vérifier le retour au repos dans la position d'installation.
- [ ] Filtre dans le circuit : l'aimant baigné peut piéger des particules métalliques.

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

Option écartée : ESP32 comme seule grille (imposerait un *readback* obligatoire du relais + watchdog — trop critique pour un tube 150 W crête / 130 W continu).

### Raccordement — NE PAS dériver les contacts direct sur GPIO

5 V sur la masse du MYJG vs 3,3 V sur la masse ESP32 : un contact ouvert enverrait ~5 V sur une broche 3,3 V, et il faudrait bonder les masses à côté d'une alim 28 kV. **Isoler chaque contact** :

1. **Pôle sec libre par appareil** — si le débit-contact (souvent inverseur), le micro-switch capot et le switch laser ont un contact inutilisé : câblage direct vers l'ESP32 en `INPUT_PULLUP`, chaîne MYJG intouchée. Le plus simple — **à vérifier en premier**.
2. **Sinon : un PC817 par nœud**, alimenté depuis les bornes `5V`/`G` du MYJG (pas en parasitant le maigre pull-up de `P`). LED de l'opto sur un nœud de la chaîne, côté transistor vers un GPIO `INPUT_PULLUP`. Isolation galvanique complète — même logique que le relais Finder utilisé pour la détection découpeuse. ~3-4 PC817 + résistances.

Dans les deux cas, sémantique fail-safe habituelle : contact fermé = LOW = condition OK ; ouvert / fil coupé = HIGH = défaut.

### Budget broches ESP32

✅ **Mis à jour 2026-09-05** (passage à l'OLED I2C, voir plus haut) : budget détendu. GPIO16/17 restent indisponibles sur cette carte, GPIO19 est pris par le 1-Wire et GPIO21/22 par l'OLED, mais l'abandon de l'écran SPI intégré libère 4 broches propres supplémentaires. Propres en `INPUT_PULLUP` (non strapping, non input-only) : **GPIO 4, 18, 23, 32** → 4 entrées, suffit pour `SW LASER` + `Flow` + `SW DOOR` + `P` sans avoir besoin des broches input-only. ⚠️ **2026-11-23 : GPIO18 pris par le contacteur de débit eau laser** → restent 4, 23, 32 ; une 4ᵉ entrée éventuelle devra prendre une broche input-only (34/35/36/39, pull-up externe obligatoire). GPIO 34/35/36/39 restent en réserve si besoin (pull-up externe 10kΩ déjà prévu de toute façon). GPIO 5 en réserve seulement (strapping). **GPIO 12 à éviter** (strapping, pull-up interne = HIGH au boot = risque de blocage).

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
