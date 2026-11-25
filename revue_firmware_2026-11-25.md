# Revue firmware régulation bain laser — 2026-11-25

> **Destinataire :** Claude (MCP oeil-atelier, ch-Linux-Mint).
> **Origine :** revue faite dans claude.ai le 2026-11-25, à partir du dépôt GitHub (docs) et des fichiers `main.cpp`, `config.h` fournis par Christian.
> **Contexte nouveau :** tube laser désormais **150 W crête / 130 W continu** (et non plus 110 W) ; chaleur à évacuer ≈ 550-750 W en continu.

## Consignes de travail

1. Implémenter les points **1, 2 et 3** ci-dessous, **et rien d'autre** (pas de refactoring annexe).
2. Compiler depuis la **copie locale** `/home/ch/PlatformIO-local/laser_bain_regulation/` (pas depuis le NAS : `.pio` corrompu sur SMB), puis synchroniser la copie NAS.
3. Ajouter une entrée datée dans `projet_regulation_laser.md` (section Firmware + « À faire »), et corriger la mention « tube 110W » → « 150 W crête / 130 W continu ».
4. Committer **ce fichier** avec les modifications, message de commit explicite par point.
5. Ne pas toucher `secrets.h`.

---

## Point 1 — Inversion silencieuse des sondes DS18B20 (critique)

### Problème

`readSensors()` utilise `sensors.getTempCByIndex(IDX_SONDE_BAIN / IDX_SONDE_LASER)`. Dans DallasTemperature, `getTempCByIndex()` **refait une recherche sur le bus à chaque appel** : l'index dépend des sondes présentes à cet instant.

### Scénario de défaillance

La sonde **bain** (index 0) se débranche ou casse → il ne reste qu'une sonde sur le bus → **l'index 0 désigne désormais la sonde laser**.
- `bathTemp` reçoit la température de l'eau laser : valeur plausible, **aucun défaut détecté**.
- La régulation pilote le compresseur/la canne sur la mauvaise sonde.
- L'interlock valide sur la mauvaise sonde.
- Seule `laserTemp` passe à NaN, et elle n'est aujourd'hui surveillée par rien (voir point 2).

### Modification attendue

Adresser chaque sonde par son **adresse ROM** (64 bits, unique et fixe).

**`config.h`** — remplacer `IDX_SONDE_BAIN` / `IDX_SONDE_LASER` par :

```cpp
// Adresses ROM des DS18B20 — relevées au banc (voir moniteur série au démarrage).
// Tant qu'elles valent ces valeurs factices, les sondes sont lues "absentes" :
// régulation coupée + sécurité BLOQUÉE (fail-safe).
#define ADDR_SONDE_BAIN   {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define ADDR_SONDE_LASER  {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

// Plage de température plausible ; hors plage = sonde considérée en défaut.
// (85,0 °C = valeur de reset à la mise sous tension d'un DS18B20 : exclue.)
#define PROBE_MIN_VALID_C  -10.0f
#define PROBE_MAX_VALID_C   80.0f
```

**`main.cpp`** :

```cpp
DeviceAddress addrBain  = ADDR_SONDE_BAIN;
DeviceAddress addrLaser = ADDR_SONDE_LASER;

// Lecture d'une sonde par adresse ; NAN si absente, CRC faux ou valeur hors plage.
float readProbe(DeviceAddress addr) {
  float t = sensors.getTempC(addr);
  if (t == DEVICE_DISCONNECTED_C) return NAN;
  if (t < PROBE_MIN_VALID_C || t > PROBE_MAX_VALID_C) return NAN;
  return t;
}

void readSensors() {
  sensors.requestTemperatures();
  bathTemp  = readProbe(addrBain);
  laserTemp = readProbe(addrLaser);
  decoupeuseOn = (digitalRead(PIN_DECOUPEUSE) == LOW);
}
```

**Aide au relevé des adresses** — dans `setup()`, à la place du simple comptage actuel :

```cpp
void printSensorAddresses() {
  sensors.requestTemperatures();
  int n = sensors.getDeviceCount();
  Serial.printf("DS18B20 detectees : %d\n", n);
  DeviceAddress a;
  for (int i = 0; i < n; i++) {
    if (!sensors.getAddress(a, i)) continue;
    Serial.printf("  Sonde %d : {", i);
    for (int k = 0; k < 8; k++) Serial.printf("0x%02X%s", a[k], k < 7 ? ", " : "");
    Serial.printf("}  T = %.2f C\n", sensors.getTempC(a));
  }
  Serial.printf("Sonde BAIN  configuree : %s\n", sensors.isConnected(addrBain)  ? "PRESENTE" : "ABSENTE");
  Serial.printf("Sonde LASER configuree : %s\n", sensors.isConnected(addrLaser) ? "PRESENTE" : "ABSENTE");
}
```

### Test au banc

1. Flasher avec les adresses factices → vérifier : **BLOQUÉ**, LED rouge, « ---» sur l'OLED pour les deux sondes, adresses réelles imprimées sur le moniteur série.
2. Chauffer une sonde à la main pour identifier laquelle est le bain, laquelle est le laser ; reporter les adresses dans `config.h`, reflasher.
3. **Test clé :** débrancher la sonde **bain** seule → attendu : bain « --- », laser toujours correcte, **BLOQUÉ**, compresseur et canne coupés. Puis l'inverse (sonde laser seule débranchée) → BLOQUÉ (via point 2).
4. Noter les adresses ROM dans `projet_regulation_laser.md` (étiqueter physiquement les câbles des sondes).

---

## Point 2 — Sonde laser intégrée à l'interlock + hystérésis des seuils + cause du blocage

### Problème

`updateInterlock()` ne regarde que la sonde **bain** (validité + `SAFETY_MAX_BATH_TEMP_C`) et le débit. La sonde laser n'a aucun effet.

### Scénario de défaillance

Échange insuffisant au serpentin (encrassement, bain non brassé, débit faible mais ILS encore fermé, découpe longue à 130 W) : le bain reste dans la plage, l'eau du tube monte, le laser reste autorisé. Ou : sonde laser HS, et personne ne le voit.

Accessoirement : sans hystérésis, une température qui oscille autour de 26,0 °C fait **claquer le relais de sécurité** à chaque cycle de 2 s.

### Modification attendue

**`config.h`** :

```cpp
// Eau laser : seuil au-delà duquel le tube est jugé insuffisamment refroidi.
// Valeur à valider au banc selon l'emplacement réel de la sonde (sortie tube
// ou retour vase) : sortie tube ≈ entrée + 2-3 °C à 130 W et 3-4 L/min.
#define SAFETY_MAX_LASER_TEMP_C  30.0f
// Hystérésis de ré-autorisation après surchauffe (bain et laser) :
// blocage au-dessus du seuil, ré-autorisation seulement sous (seuil - hystérésis).
#define SAFETY_TEMP_HYST_C        1.0f
```

**`main.cpp`** — nouvelles variables d'état :

```cpp
bool bathOverTemp  = false;   // verrouillées avec hystérésis
bool laserOverTemp = false;
const char* blockReason = "";  // cause lisible du blocage (OLED + web), "" = OK
```

**`updateInterlock()`** remplacée par :

```cpp
// Surchauffe avec hystérésis (état conservé tant que la sonde est en défaut :
// le défaut sonde bloque de toute façon).
static void updateOverTemp(float t, float maxC, bool &flag) {
  if (isnan(t)) return;
  if (t > maxC) flag = true;
  else if (t < maxC - SAFETY_TEMP_HYST_C) flag = false;
}

void updateInterlock() {
  unsigned long now = millis();

  if (bypassActive && (now - bypassStartMs > BYPASS_TIMEOUT_MS)) {
    bypassActive = false;
  }

  updateOverTemp(bathTemp,  SAFETY_MAX_BATH_TEMP_C,  bathOverTemp);
  updateOverTemp(laserTemp, SAFETY_MAX_LASER_TEMP_C, laserOverTemp);

  // Ordre = priorité d'affichage de la cause.
  if      (isnan(bathTemp))  blockReason = "SONDE BAIN HS";
  else if (isnan(laserTemp)) blockReason = "SONDE LASER HS";
  else if (bathOverTemp)     blockReason = "SURCHAUFFE BAIN";
  else if (laserOverTemp)    blockReason = "SURCHAUFFE LASER";
  else if (!flowPresent)     blockReason = "PAS DE DEBIT";
  else if (!flowOk)          blockReason = "DEBIT: STABILISATION";
  else                       blockReason = "";

  bool safe = (blockReason[0] == '\0');

  relayClosed = bypassActive ? true : safe;
  safetyGreen = relayClosed && !bypassActive;

  digitalWrite(PIN_RELAY, relayClosed ? HIGH : LOW);
}
```

**`updateDisplay()`** — remplacer le bloc de la dernière ligne (y = 52) par :

```cpp
  if (bypassActive)            display.print("BYPASS ACTIF");
  else if (blockReason[0])     display.print(blockReason);
```

**`handleRoot()`** — juste après la ligne « Securite MYJG : AUTORISE/BLOQUE », ajouter :

```cpp
  if (!relayClosed && blockReason[0]) {
    html += "<p>Cause : <span class='bad'>" + String(blockReason) + "</span></p>";
  }
```

Et mettre à jour le texte de confirmation du bypass : « ignore TOUTES les protections (sondes, surchauffe bain et laser, débit) ».

### Test au banc

1. Tout OK (sondes en place, ILS débit shunté par un fil) → AUTORISÉ, LED verte, pas de cause affichée.
2. Chauffer la sonde laser au-dessus de 30 °C → BLOQUÉ, « SURCHAUFFE LASER » sur OLED et web. Laisser refroidir : reste bloqué jusqu'à < 29 °C, puis ré-autorisé.
3. Idem sonde bain au-delà de 26 °C → « SURCHAUFFE BAIN », ré-autorisation sous 25 °C.
4. Débrancher la sonde laser → « SONDE LASER HS ».
5. Vérifier que le compteur d'usure ne compte pas pendant ces blocages (il dépend de `safetyGreen`, inchangé).

---

## Point 3 — Watchdog (critique)

### Problème

Aucun watchdog n'est actif sur `loop()` (sur le core Arduino ESP32, le watchdog de la tâche loop est **désactivé par défaut**).

### Scénario de défaillance

`loop()` se fige (bus I2C de l'OLED bloqué, bug, pile WiFi…) alors que le relais est fermé : `PIN_RELAY` **reste HIGH indéfiniment**. Plus aucune lecture de débit ni de température : le laser reste autorisé sans aucune surveillance. Le fail-safe actuel ne couvre que la perte d'alimentation et le reset.

### Modification attendue

**`main.cpp`** :

```cpp
#include <esp_system.h>

const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "mise sous tension";
    case ESP_RST_SW:       return "logiciel";
    case ESP_RST_PANIC:    return "PANIC (crash)";
    case ESP_RST_INT_WDT:  return "WATCHDOG (interruption)";
    case ESP_RST_TASK_WDT: return "WATCHDOG (tache)";
    case ESP_RST_WDT:      return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT (alim)";
    default:               return "autre";
  }
}
esp_reset_reason_t bootResetReason;
```

Dans `setup()` :
- **au tout début** (après `Serial.begin`) : `bootResetReason = esp_reset_reason();` + l'imprimer sur le série ;
- **à la toute fin** (après le WiFi, dont la connexion bloque jusqu'à 15 s) : `enableLoopWDT();`

Le core nourrit alors le watchdog à chaque retour de `loop()` ; délai par défaut du core Arduino : **5 s** (`CONFIG_ESP_TASK_WDT_TIMEOUT_S`) — **vérifier la valeur effective** dans le build (version du core utilisée par `platformio.ini`). Le pire tour de `loop()` normal est d'environ 0,8 s (`requestTemperatures()` bloquant ~750 ms en 12 bits) : marge suffisante.

Dans `handleRoot()` : afficher « Dernier redémarrage : <cause> », en rouge si watchdog, panic ou brownout.

**Option de test (temporaire)** — dans `config.h`, commenté par défaut :

```cpp
// #define TEST_WATCHDOG   // simule un blocage de loop() 90 s après le boot — NE JAMAIS LAISSER ACTIF
```

et en fin de `loop()` :

```cpp
#ifdef TEST_WATCHDOG
  if (millis() > 90000UL) { while (true) { } }
#endif
```

### Test au banc

1. Compiler avec `TEST_WATCHDOG`, mettre le système en état AUTORISÉ (sondes OK, ILS shunté) avant 90 s.
2. À 90 s : la boucle se fige → attendu en **≤ ~5 s** : reset, **relais ouvert**, LED rouge, puis redémarrage normal ; le web affiche « Dernier redémarrage : WATCHDOG (tache) ».
3. **Vérifier pendant le reset** que le module relais BESTEP reste bien **retombé** (GPIO27 en haute impédance pendant le reset/boot : contrôle du cavalier high/low, IN flottant = bobine non alimentée).
4. Recompiler **sans** `TEST_WATCHDOG` et reflasher. Confirmer dans le log de build que la macro n'est pas définie.

### Effet secondaire accepté

Un reset watchdog perd au plus 5 min de cumul du compteur d'usure (dernière sauvegarde NVS périodique). Sans importance.

---

## Hors périmètre (pour mémoire, à traiter plus tard)

- `requestTemperatures()` bloquant ~750 ms : débit non relu pendant ce temps (`setWaitForConversion(false)` + lecture différée).
- Capteur de niveau bas du vase d'expansion sur GPIO 4/23/32 (`INPUT_PULLUP` + 10 kΩ / 100 nF), intégré à l'interlock et coupant la pompe.
- Mot de passe du site web à définir solidement au déploiement (il commande le bypass total).
