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
- **2× sondes DS18B20** commandées — usage prévu : une dans le bain (régulation), une sur le retour laser (surveillance delta de température / détection anomalie de circulation).
- **Relais de sécurité** en série sur la ligne de sécurité du MYJG (autorise/bloque le fonctionnement du laser selon l'état du refroidissement).
- **LED Rouge** (défaut, bloque le relais) et **LED Verte** (OK, autorise le relais) — indicateurs de sécurité physiques, indépendants de l'écran/du site web.
- **Entrée** lisant l'état du bouton ON/OFF de la découpeuse (contact mis à la masse).
- **Afficheur température entrée laser** : utilise l'écran intégré de la carte ESP32 (voir ci-dessous) plutôt qu'un module séparé.
- **Site web embarqué** (protégé par mot de passe) : lecture des paramètres/E-S en temps réel + bouton de forçage de fermeture du relais.

## À faire / en attente

- [x] **Mesurer les specs du compresseur** — fait le 2026-09-02 : démarrage ~7A, fonctionnement 1,4A. Dimensionnement BTA16-600BW validé (voir ci-dessous).
- [x] **Choisir la plateforme** — décidé le 2026-09-02 : **ESP32-WROOM-32** (carte de dev en stock avec écran 1,9" intégré, cf. identification ci-dessous). Nécessaire vu le nombre d'E/S (relais sécurité, 2 LED, 2 MOC3023, 1-Wire, entrée découpeuse) — l'ESP8266 aurait obligé à mobiliser les broches de strapping boot.
- [x] **Sémantique du bouton "forcer fermeture" du site web** — décidé le 2026-09-02 : **bypass total**. Usage : opérations de maintenance nécessitant la sécurité MYJG fermée (ex. vérifier la continuité de la chaîne de sécurité — porte + autres capots en série avec ce relais — indépendamment de l'état du refroidissement). Voir garde-fous ci-dessous.
- [x] **Câblage détaillé MOC3023 → BTA16-600BW** — fait le 2026-09-02, voir schéma et valeurs ci-dessous.
- [ ] Circuit de commande du relais de sécurité (transistor + diode de roue libre, ou module relais tout fait — un GPIO ne peut pas piloter la bobine directement).
- [ ] Firmware : logique de régulation + protections + site web sécurisé.

## Carte ESP32 identifiée

Carte de dev **ESP32-WROOM-32** (puce CH340) avec écran intégré **ST7789 170×320**, dont la moitié est physiquement cassée — viewport logiciel limité à **170×200** (retrouvé dans un ancien projet `esp_lcd_1.9`, config exacte récupérée depuis `User_Setup.h` de TFT_eSPI).

**Broches déjà fixées par l'écran (ne pas réutiliser) :**

| Fonction écran | GPIO |
|---|---|
| Backlight | 32 |
| MOSI | 23 |
| SCLK | 18 |
| CS | 15 |
| DC | 2 |
| RST | 4 |

⚠️ L'écran étant endommagé, il sert uniquement d'**affichage informatif** (température courante) — jamais pour une indication de sécurité. Les LED rouge/verte restent seules garantes du statut autorisé/bloqué.

## Plan de brochage ESP32 (hors broches écran)

| Fonction | GPIO | Note |
|---|---|---|
| MOC3023 #1 (compresseur) | 13 | |
| MOC3023 #2 (canne chauffante) | 14 | |
| Relais sécurité MYJG | 27 | via module relais ou transistor, jamais en direct |
| LED Rouge | 26 | |
| LED Verte | 25 | |
| Bus 1-Wire (DS18B20 ×2) | 16 | + pull-up 4,7kΩ |
| Entrée état découpeuse ON/OFF | 34 | input-only, pull-down externe requis |

Large marge disponible (GPIO 5, 12, 17, 19, 21, 22, 33, 35, 36, 39 libres) — aucune broche de strapping boot mobilisée pour ces signaux, contrairement à l'option ESP8266 envisagée initialement.

## Câblage détaillé MOC3023 → BTA16-600BW (×2 : compresseur + canne chauffante)

Isolation galvanique basse tension (ESP32) / secteur (230V) assurée par le MOC3023 — à respecter physiquement dans le coffret (pas de piste/fil basse tension à proximité du côté secteur).

```
                              AC LINE (Phase, 230V)
                                      │
                                   [ CHARGE ]   (compresseur ou canne)
                                      │
                    ┌─────────────────┴─────────────────┐
                    │                                     │
              BTA16 MT2 (pin3 + patte centrale) ───── MOC3023 pin4 (MT2 interne)
                    │                                     │
              BTA16 Gate (pin2) ◄──── R_G ──────────  MOC3023 pin6 (MT1/Gate interne)
                    │              330-360Ω 1W
                    │
              BTA16 MT1 (pin1)
                    │
                    └───────────────────────────────────────► NEUTRE

   ESP32 GPIO (13 ou 14) ──R_LED (180Ω)── MOC3023 pin1 (anode LED)
                                            MOC3023 pin2 (cathode) ── GND ESP32
```

**Valeurs :**

| Résistance | Valeur | Rôle |
|---|---|---|
| R_LED (côté ESP32) | 180Ω, 1/4W | Limite le courant LED du MOC3023 à ~11,7mA sous 3,3V — marge au-dessus du IFT max garanti (10mA) |
| R_G (côté secteur, gâchette) | 330-360Ω, 1W, tenue 350V+ | Valeur standard datasheet pour 230V (180Ω pour 120V, 330-360Ω pour 240V) |

**Nomenclature par canal :** 1× MOC3023, 1× BTA16-600BW, 1× R_LED 180Ω 1/4W, 1× R_G 330-360Ω 1W.

## Sécurité — logique fail-safe du relais

Le relais de la ligne de sécurité MYJG doit être **ouvert par défaut** (non alimenté = sécurité active) : toute coupure de courant, crash logiciel, reset ou watchdog doit laisser le circuit en position "arrêt", jamais fermé par défaut. Le circuit de commande (transistor/module relais) doit être conçu pour qu'un GPIO flottant ou à l'état bas au démarrage n'active pas le relais.

## Bouton "forcer fermeture" (site web) — bypass total

Le relais thermique n'est qu'un maillon de la chaîne de sécurité MYJG (en série avec la porte et d'autres capots). Le bouton de forçage sert à fermer ce relais pour tester le reste de la chaîne en maintenance, indépendamment de l'état du refroidissement. C'est un **bypass total** : il ignore même un dépassement réel de seuil de température.

**Garde-fous à implémenter côté firmware :**

- **Pas de persistance au reboot** — l'état bypass ne doit jamais être sauvegardé en flash ; à chaque redémarrage de l'ESP32, on repart toujours en mode protégé.
- **Timeout automatique** — le bypass se désactive tout seul après un délai (ex. 10-15 min), pour ne pas rester actif indéfiniment après la maintenance.
- **Indicateur visible pendant le bypass** — ex. LED rouge clignotante (motif différent du "défaut" fixe) tant que le bypass est actif, pour ne jamais confondre "sécurité OK" et "sécurité forcée".

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

## Composants utilisés (voir fiches détaillées dans l'inventaire)

- [MOC3023](inventaire_composants.md#moc3023--optocoupleur-sortie-triac-sans-zcd) — driver optocoupleur TRIAC
- [BTA16-600BW](inventaire_composants.md#bta16-600bw--triac-16a--600v) — TRIAC de puissance
- DS18B20 (2×) — sondes température 1-Wire, commandées, fiche donnée en conversation (à ajouter à l'inventaire si besoin)

---

*Fichier de suivi projet — mis à jour au fil de l'avancement.*
