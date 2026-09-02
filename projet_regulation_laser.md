# Projet — Régulation température bain refroidissement tube laser 110W

> Démarré 2026-09-02

---

## Objectif

Réguler à **22°C** l'eau d'un bain tampon de 30L qui refroidit le circuit de retour d'un tube laser CO2 110W, via un ESP32/ESP8266 (NodeMCU).

## Principe

- **Bac 30L** avec deux serpentins distincts :
  - Serpentin 1 : circuit du compresseur/chiller (refroidissement du bain)
  - Serpentin 2 : circuit de retour d'eau du tube laser (chauffe le bain, à refroidir)
- **Canne chauffante** dans le bac, pour compenser en hiver si la température descend sous la consigne.
- **ESP32/ESP8266** pilote compresseur ET canne chauffante en tout-ou-rien via optocoupleurs **MOC3023** + triacs **BTA16-600BW** (voir [inventaire_composants.md](inventaire_composants.md)).
- **2× sondes DS18B20** commandées — usage prévu : une dans le bain (régulation), une sur le retour laser (surveillance delta de température / détection anomalie de circulation).

## À faire / en attente

- [x] **Mesurer les specs du compresseur** — fait le 2026-09-02 : démarrage ~7A, fonctionnement 1,4A. Dimensionnement BTA16-600BW validé (voir ci-dessous).
- [x] **Choisir ESP32 ou ESP8266** — décidé le 2026-09-02 : ESP8266 (D1 Mini ou NodeMCU au choix, même GPIO mapping, disponibles en stock). Voir plan de brochage ci-dessous.
- [ ] Câblage détaillé MOC3023 → BTA16-600BW pour chaque charge (compresseur / canne), avec résistance de gâchette (180-330Ω à préciser).
- [ ] Firmware : logique de régulation + protections.

## Plan de brochage ESP8266 (D1 Mini / NodeMCU — mapping identique)

| Broche (silkscreen) | GPIO | Fonction |
|---|---|---|
| D5 | GPIO14 | MOC3023 #1 → compresseur |
| D6 | GPIO12 | MOC3023 #2 → canne chauffante |
| D7 | GPIO13 | Bus 1-Wire (DS18B20 ×2) + pull-up 4,7kΩ |
| D1 / D2 | GPIO5 / GPIO4 | Libres (extension future, ex. écran I2C) |

**Broches à éviter** (strapping au boot) : D3 (GPIO0), D4 (GPIO2), D8 (GPIO15) — un état imposé par un module externe sur ces broches au démarrage peut empêcher l'ESP8266 de booter correctement.

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
