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

*Fichier généré avec le microscope USB Dino-Lite via oeil-atelier-mcp*
