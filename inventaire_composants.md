# Inventaire Composants Électroniques

> Identifiés au microscope Dino-Lite — 2026-07-01

---

## SN75175N — Récepteur RS-422/RS-485 quadruple

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Texas Instruments |
| Boîtier | DIP-16 |
| Alimentation | 5V |
| Canaux | 4 récepteurs différentiels |
| Plage entrée différentielle | ±7V |
| Sorties | TTL/LSTTL compatibles |

**Brochage (DIP-16) :**

| Broche | Fonction |
|--------|----------|
| 1, 4, 9, 12 | Sorties Y |
| 2, 3, 10, 11 | Entrées A+ / B− (différentiel) |
| 5, 6, 13, 14 | Enables (actif bas) |
| 8 | GND |
| 16 | VCC (+5V) |

**Usage :** réception de signaux série longue distance, bus industriels RS-485/RS-422. Souvent associé au SN75174 (émetteur) ou SN75176 (transceiver).

---

## BTA16-600BW — TRIAC 16A / 600V

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | STMicroelectronics |
| Boîtier | D²PAK (TO-263), isolé, CMS |
| Courant RMS | 16A |
| Tension de blocage | 600V |
| Courant de gâchette | 50mA typique |
| Date fab. | 2023, semaine 05 |

**Brochage (TO-263, 3 broches) :**

| Broche | Fonction |
|--------|----------|
| 1 | MT1 (Main Terminal 1) |
| 2 | Gate (gâchette) |
| 3 + patte centrale | MT2 |

**Usage :** commande de charges AC secteur 230V (moteurs, résistances de chauffage, éclairage). Utilisé dans les dimmers et relais statiques AC.

---

## MOC3023 — Optocoupleur sortie TRIAC (sans ZCD)

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | ON Semiconductor |
| Boîtier | DIP-6 |
| Isolation | 5 300V crête |
| Tension TRIAC interne | 400V |
| Courant LED déclenchement | 15mA typique |
| Zero Crossing Detector | Non |

**Brochage (DIP-6) :**

| Broche | Fonction |
|--------|----------|
| 1 | Anode LED |
| 2 | Cathode LED |
| 3 | N.C. |
| 4 | MT2 (TRIAC interne) |
| 5 | N.C. |
| 6 | MT1 / Gate (TRIAC interne) |

**Usage :** isolation galvanique MCU ↔ secteur 230V, driver pour TRIAC de puissance. S'associe typiquement avec le BTA16-600BW pour réaliser un gradateur ou relais statique.

---

## SN74HC595N — Registre à décalage 8 bits série→parallèle

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Texas Instruments |
| Boîtier | DIP-16 |
| Alimentation | 2V – 6V |
| Sorties | 8 parallèles, 35mA/broche |
| Interface | SPI 3 fils (DATA, CLK, LATCH) |
| Cascade | Oui (Q7' → entrée suivant) |

**Brochage (DIP-16) :**

| Broche | Fonction |
|--------|----------|
| 1–7, 15 | QB–QH, QA (sorties parallèles) |
| 8 | GND |
| 9 | Q7' (sortie série pour cascade) |
| 10 | /SRCLR (reset, actif bas) |
| 11 | SRCLK (horloge décalage) |
| 12 | RCLK (latch / horloge stockage) |
| 13 | /OE (enable sortie, actif bas) |
| 14 | SER (entrée série DATA) |
| 16 | VCC |

**Usage :** extension de sorties depuis microcontrôleur (3 broches GPIO → 8 sorties), commande de LEDs, relais, afficheurs 7 segments. Cascadable à l'infini.

---

## PC817 — Optocoupleur simple canal, sortie transistor

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Sharp / générique |
| Boîtier | DIP-4 |
| Isolation | 5 000V crête |
| CTR | 50% – 300% |
| Courant LED | 1 – 50mA |
| VCE max | 35V |
| Date fab. | 2018, semaine 27 |

**Brochage (DIP-4) :**

| Broche | Fonction |
|--------|----------|
| 1 | Anode LED |
| 2 | Cathode LED |
| 3 | Émetteur transistor |
| 4 | Collecteur transistor |

**Usage :** isolation galvanique généraliste, feedback d'alimentations à découpage (flyback), protection des entrées de microcontrôleurs contre des tensions externes.

---

## L7805CV — Régulateur linéaire +5V / 1,5A

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | STMicroelectronics |
| Boîtier | TO-220 |
| Tension de sortie | +5V fixe |
| Courant max | 1,5A |
| Tension d'entrée | 7V – 35V |
| Dropout | ~2V |
| Date fab. | Mars 2018 |
| Protections | Surchauffe, court-circuit |

**Brochage (TO-220, vue face marquée) :**

| Broche | Fonction |
|--------|----------|
| 1 (gauche) | Entrée VIN |
| 2 (centre) | Masse GND |
| 3 (droite) | Sortie VOUT (+5V) |

**Usage :** alimentation 5V stabilisée depuis une source 7–35V (typiquement 9V, 12V ou 24V). Pour alimenter circuits logiques, microcontrôleurs, capteurs.

---

## ST3232EB (U1) — Module convertisseur RS-232 3,3V

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | STMicroelectronics |
| Boîtier | SOIC-16 (marquage CT252) |
| Alimentation | 3,3V |
| Fonction | Émetteur/récepteur RS-232 duplex (équiv. MAX3232) |
| Date fab. | Semaine 20 (marquage CHN 520) |

**Connecteur module (header 4 broches) :**

| Broche | Fonction |
|--------|----------|
| 1 | VCC |
| 2 | TXD |
| 3 | RXD |
| 4 | GND |

**Bornier vis (3 points) :** TXD, RXD, GND (côté ligne RS-232 terrain).

**Usage :** conversion niveaux logiques TTL 3,3V ↔ RS-232, interfaçage microcontrôleur avec équipement série RS-232 (PC, instrumentation).

---

## GP-04S (M.C Marine) — Antenne GPS active, sortie NMEA0183 RS232

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | M.C Marine |
| Alimentation | +12V |
| Sortie | NMEA0183 sur RS232 (single-ended, réf. masse commune) |
| Baud rate | 4800 (à confirmer, essayer 9600 si rien ne vient) |
| Documentation constructeur | Aucune disponible — brochage identifié par mesure/info ancien propriétaire |

**Câblage (6 fils) :**

| Fil | Fonction |
|-----|----------|
| Rouge | +12V (alimentation) |
| Noir | GND (masse alim + signal) |
| Tresse | GND (blindage — relier à Noir en un seul point) |
| Vert | TX (données NMEA, GPS → récepteur) |
| Blanc | RX (commandes, récepteur → GPS, optionnel) |
| Bleu | Inconnu (spare / alarme antenne ?) — non connecté |

**Interfaçage :** via module [ST3232EB (U1)](#st3232eb-u1--module-convertisseur-rs-232-33v) — Vert (TX GPS) → bornier RXD du module, Blanc (RX GPS) → bornier TXD du module (optionnel). Sortie TTL du module vers UART Arduino/ESP, parsing avec bibliothèque TinyGPS++, affichage sur LCD I2C.

**Usage :** projet banc de test GPS marine — récupération position/satellites via Arduino/ESP + LCD.

---

## 12864B V2.0 — Écran LCD graphique 128×64 (ST7920)

| Paramètre | Valeur |
|-----------|--------|
| Marquage PCB | 12864B V2.0 |
| Contrôleur | ST7920 (probable, cohérent avec le marquage et le connecteur 20 broches) |
| Résolution | 128×64 points |
| Interface | Parallèle 20 broches, utilisable en mode série 3 fils (PSB au niveau bas) |
| Particularité | Potentiomètre de contraste intégré au PCB |

**Câblage utilisé (mode série, PSB → GND) :**

| Broche écran | Fonction | Vers (Arduino Mega 2560) |
|--------------|----------|---------------------------|
| PSB | Sélection mode (bas = série) | GND |
| RS | CS | Pin 10 |
| R/W | SCLK | Pin 11 |
| E | MOSI | Pin 12 |
| RST | Reset (optionnel) | Pin 13 (ne pas relier au RESET de la carte — bloque l'upload) |
| D0-D7 | Non utilisées en mode série | Non connectées |
| VSS / VDD / V0 / A / K | Alimentation, contraste, rétroéclairage | GND / +5V / potentiomètre / +5V / GND |

**Usage :** bibliothèque U8g2 (`U8G2_ST7920_128X64_F_SW_SPI`) — affichage des données GPS (position, satellites/HDOP, heure UTC) dans le projet banc GPS GP-04S.

---

## RAMPS 1.3 + RepRapDiscount Full Graphic Smart Controller

| Paramètre | Valeur |
|-----------|--------|
| Shield | RAMPS 1.3 (compatible 1.4/1.5/1.6, brochage EXP1/EXP2 identique) |
| Contrôleur écran | ST7920, LCD graphique 128×64 |
| Interface écran | Série 3 fils (PSB déjà câblé à GND sur le shield) |
| Hôte | Arduino Mega 2560 (pins 22-53 occupées par le shield) |

**Brochage écran (EXP1/EXP2 → Mega, standard Marlin/RAMPS) :**

| Fonction | Broche Mega |
|----------|-------------|
| Clock (E) | 23 |
| Data (R/W) | 17 |
| CS (RS) | 16 |
| Reset | Non connecté (`U8X8_PIN_NONE`) |

**Accès aux broches Serial1 (D18/D19) malgré le shield :** disponibles via les connecteurs d'endstop **Z-MIN** (D18, TX1) et **Z-MAX** (D19, RX1), chacun un connecteur 3 broches Signal/GND/+5V — pratique pour brancher un module UART externe (ex. GPS via ST3232) sans souder sur la carte.

**Alimentation externe 12V :** disponible en continu (non commutée) sur les bornes à vis d'entrée d'alimentation principale de la RAMPS — vérifier la tension réelle au multimètre avant de piquer dessus (peut être 12V ou 24V selon l'alimentation utilisée).

**Usage :** projet banc GPS GP-04S — écran de contrôle réutilisé depuis un kit imprimante 3D, piloté par bibliothèque U8g2 (`U8G2_ST7920_128X64_F_SW_SPI`), GPS alimenté et connecté via les connecteurs endstop Z.

---

## MP1584EN — Module convertisseur buck (step-down) ajustable

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Monolithic Power Systems |
| Marquage CI | MP1584EN D5808297 |
| Boîtier | SOIC-8 |
| Tension d'entrée | Jusqu'à 24V (typique 4,5V-28V) |
| Courant de sortie | 3A max |
| Sortie | Ajustable via trimmer (potentiomètre bleu) |
| Composants clés | Inductance blindée 15µH ("150"), diode Schottky SS34 |

**Brochage module :**

| Borne | Fonction |
|-------|----------|
| VIN- | Masse entrée |
| VIN+ | +Alimentation entrée (jusqu'à 24V) |
| VOUT-/VOUT+ | Sortie régulée (côté opposé, à ajuster via trimmer) |

**Usage :** conversion 24V → 5V pour alimentation ESP8266 (banc d'essai WindS400) — variante plus performante des modules "Mini360" (MP2307), même principe d'ajustement par trimmer.

---

## BC212L — Transistor PNP faible bruit

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Fairchild Semiconductor |
| Marquage | BC212L (+ code date/lot "13C") |
| Boîtier | TO-92 |
| Type | PNP, usage général / basse fréquence, faible bruit |
| Vceo max | ~50V |
| Ic max | ~100mA |
| hFE (grade L) | Élevé, typiquement 100-800 |

**Brochage (TO-92, face plate vers soi, à confirmer au multimètre) :**

| Broche | Fonction |
|--------|----------|
| 1 (gauche) | Émetteur |
| 2 (centre) | Base |
| 3 (droite) | Collecteur |

**Usage :** amplification faible signal, préampli audio, complémentaire du BC182L (NPN) pour montages push-pull.

---

## PCF8574T — Module adaptateur I2C pour LCD (backpack)

| Paramètre | Valeur |
|-----------|--------|
| Marquage CI | PCF8574T ABX919 |
| Fabricant | NXP (ou compatible) |
| Fonction | Expandeur d'E/S I2C 8 bits, pilotage LCD HD44780 (16x2, 20x4) |
| Adressage | Cavaliers A0/A1/A2 (adresse I2C configurable, plage 0x20-0x27 ou 0x38-0x3F selon variante) |
| Réglage contraste | Trimmer bleu intégré (P103) |
| Connecteur hôte | GND, VCC, SDA, SCL |

**Usage :** transforme un écran LCD alphanumérique parallèle (6+ fils) en interface I2C 2 fils (SDA/SCL) — pratique pour économiser des broches sur microcontrôleur (Arduino, ESP8266/ESP32).

---

## Module capteur à effet Hall (A3144 + LM393) — sortie A0/D0

| Paramètre | Valeur |
|-----------|--------|
| Élément Hall | A3144 (marquage "44E 506"), boîtier TO-92 |
| Comparateur | LM393 (marquage "LM393 PGCV"), boîtier SOIC-8 |
| Silkscreen PCB | "Sensor" (A0/GND/VCC/D0) et "Power" |
| Réglage seuil | Trimmer/potentiomètre rond intégré (ajuste le seuil de bascule du LM393) |

**Connecteur (header 4 broches) :**

| Broche | Fonction |
|--------|----------|
| A0 | Sortie analogique — tension brute (buffée) proportionnelle au champ magnétique perçu par le A3144 |
| GND | Masse |
| VCC | Alimentation |
| D0 | Sortie numérique tout-ou-rien — sortie du LM393, bascule 0/1 quand le signal A0 dépasse le seuil réglé par le trimmer |

**Fonctionnement :** pas de cavalier de sélection — les deux sorties A0 et D0 sont actives simultanément et en permanence, indépendantes l'une de l'autre.

**Usage :** détection de champ magnétique (proximité, comptage de tours, fin de course sans contact). A0 pour une mesure d'intensité, D0 pour un seuil de détection binaire directement exploitable par un GPIO microcontrôleur.

---

## VS1838B — Récepteur infrarouge intégré

| Paramètre | Valeur |
|-----------|--------|
| Marquage | VS 1838B |
| Équivalent | TSOP1838 / TSOP4838 (variante générique) |
| Boîtier | TO-92 modifié, blindage métallique |
| Fréquence porteuse | 38 kHz |
| Alimentation | 2,7V – 5,5V |
| Sortie | Numérique, signal déjà démodulé |

**Brochage (3 broches) :**

| Broche | Fonction |
|--------|----------|
| 1 | OUT (signal démodulé) |
| 2 | GND |
| 3 | VCC |

**Usage :** réception de télécommande infrarouge (protocoles NEC, RC5, Sony, etc.). Bibliothèque `IRremote` très répandue sur Arduino/ESP pour le décodage.

---

## DS18B20 — Sonde de température numérique 1-Wire

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Maxim Integrated (ex-Dallas Semiconductor) |
| Boîtier | TO-92 (version nue) ou sonde inox étanche avec câble |
| Alimentation | 3,0V – 5,5V |
| Mode parasite power | Oui (alimentation via la ligne data seule, sans fil VCC séparé) |
| Plage de mesure | −55°C à +125°C |
| Précision | ±0,5°C sur la plage −10°C à +85°C (dégradée hors de cette plage) |
| Résolution | Configurable 9 à 12 bits (0,5°C / 0,25°C / 0,125°C / 0,0625°C) |
| Temps de conversion | 750ms max en 12 bits (plus rapide si résolution réduite) |
| Interface | 1-Wire (bus série sur un seul fil data) |
| Identifiant | Code ROM 64 bits unique gravé en usine — plusieurs sondes cohabitent sur le même bus |

**Câblage (3 fils, version sonde étanche) :**

| Fil | Fonction |
|-----|----------|
| Rouge | VCC (3,3-5V) |
| Noir | GND |
| Jaune (ou blanc) | DATA (bus 1-Wire) |

**Résistance de tirage (pull-up) :** obligatoire sur DATA, **4,7kΩ entre DATA et VCC** — sans elle, aucune communication.

**Usage :** mesure de température précise multi-points sur un seul GPIO (adressage individuel par ROM code). Bibliothèques `OneWire` + `DallasTemperature` sur Arduino/ESP32/ESP8266. Utilisé dans le [projet de régulation du bain de refroidissement tube laser 110W](projet_regulation_laser.md) — une sonde dans le bain, une sur le retour d'eau laser.

---

## Module relais 1 canal BESTEP (relais Songle SRD-03VDC-SL-C)

> Identifié au Dino-Lite — 2026-09-03

| Paramètre | Valeur |
|-----------|--------|
| Marque module | BESTEP, « 1 Channel Relay module — High/Low level trigger » |
| Relais | Songle **SRD-03VDC-SL-C** |
| Tension bobine | **3V DC** |
| Type de contact | **SPDT** (`SL-C`) — COM / NO / NC sur bornier à vis 3 points |
| Pouvoir de coupure | 10A 250VAC · 10A 125VAC · 10A 30VDC · 10A 28VDC |
| Conso bobine | ≈ 0,36W → **~120mA** sous 3V (bobine ~25Ω) |
| Onboard | transistor de commande, diode de roue libre, LED d'état relais, LED PWR |
| Sélection déclenchement | cavalier jaune **High / Low level trigger** |
| Isolation galvanique | probablement absente sur ce modèle 1 canal (pas de séparation JD-VCC / VCC) |

**Connecteur commande (header 3 broches) :** VCC, IN (signal), GND.

**Fail-safe (à valider au banc) :** régler le cavalier pour que IN bas ou flottant → bobine **non** alimentée → contact ouvert. Câbler la charge sur **COM + NO**.

**Usage :** candidat pour le **relais de sécurité MYJG** du [projet régulation bain laser 110W](projet_regulation_laser.md) — bobine 3V compatible rail 3,3V ESP32, remplace le circuit discret transistor + diode de roue libre (le projet autorise « module relais ou transistor »). Réserves : conso bobine ~120mA sur le régulateur 3,3V de la carte (plus que les ~75mA prévus au discret) ; commutation « sèche » d'une boucle interlock bas niveau → oxydation de contact possible à long terme, d'où l'intérêt de câbler le contact NC vers un GPIO libre pour détecter un contact collé.

---

## Finder 55.34.8.230.0040 — Relais embrochable 4 inverseurs (4RT), bobine 230V AC

> Identifié au Dino-Lite — 2026-09-03

| Paramètre | Valeur |
|-----------|--------|
| Fabricant | Finder, série 55 (relais industriel embrochable) |
| Référence | **55.34.8.230.0040** |
| Configuration | **4 contacts inverseurs** (4 CO / 4RT) — `.34` = 4 pôles 7A |
| Tension bobine | **230V AC** (`.8` = bobine AC 50/60Hz), bornes **A1 / A2** |
| Pouvoir de coupure | **7A / 250V AC** par contact |
| Indication / test | indicateur mécanique d'état + **bouton test** (bouton carré en façade) ; LED selon variante |
| Montage | sur support embrochable série 94 (non fourni ici), clip de maintien |

**Repérage des contacts (double numérotation en façade — n° relais / n° borne support) :**

| Pôle | Commun | NO | NC |
|---|---|---|---|
| 1 | 9 | 5 | 1 |
| 2 | 10 | 6 | 2 |
| 3 | 11 | 7 | 3 |
| 4 | 12 | 8 | 4 |

Bobine : bornes 13 (A1) / 14 (A2).

**Usage :** commutation multipôle sous secteur (jusqu'à 4 circuits 230V / 7A simultanés) — distribution, séquençage, report d'état. **Bobine à alimenter en 230V AC** : ne se pilote pas depuis un GPIO ; il faut un étage secteur en amont (contact d'un petit relais 3V/5V, sortie triac type MOC3023 + BTA16, ou contact sec d'automate).

**Pour le [projet régulation bain laser 110W](projet_regulation_laser.md) :** **pas adapté au relais de sécurité MYJG** — ce rôle exige une bobine basse tension pilotée en fail-safe direct depuis un GPIO 3,3V (non alimentée = sécurité active). Mettre du 230V sur la bobine ajoute un étage de puissance et des modes de défaillance sur la chaîne de sécurité. Réservé à d'éventuels besoins de commutation secteur multipôle ailleurs dans le coffret.

---

*Fichier généré avec le microscope USB Dino-Lite via oeil-atelier-mcp — dernière mise à jour 2026-09-03 (ajout modules relais BESTEP SRD-03VDC-SL-C et Finder 55.34.8.230.0040).*
