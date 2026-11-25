#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <time.h>
#include <esp_system.h>

#include "config.h"
#include "secrets.h"

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);
WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Preferences prefs;

DeviceAddress addrBain  = ADDR_SONDE_BAIN;
DeviceAddress addrLaser = ADDR_SONDE_LASER;

// ---- État partagé ----
float bathTemp = NAN;
float laserTemp = NAN;
bool decoupeuseOn = false;
bool flowPresent = false;          // lecture brute ILS (LOW = débit)
bool flowOk = false;               // débit présent ET stable depuis FLOW_STABLE_MS
unsigned long flowSinceMs = 0;     // instant d'apparition du débit (front absent -> présent)

// ---- Compteur d'utilisation découpeuse (persisté en NVS) ----
struct DecoupeuseLog {
  uint32_t timestamps[DECOUPEUSE_LOG_SIZE];    // epoch secondes (0 = heure inconnue au moment du log)
  uint32_t durationsSec[DECOUPEUSE_LOG_SIZE];  // durée de la session ; mise à jour en direct tant qu'elle est en cours
  uint8_t count;                               // entrées valides (0..DECOUPEUSE_LOG_SIZE)
  uint8_t nextIdx;                             // prochain emplacement libre (tampon circulaire)
};
DecoupeuseLog decoupeuseLog;

uint32_t decoupeuseTotalSec = 0;   // cumul temps ON, en secondes
uint32_t decoupeuseSavedSec = 0;   // dernière valeur écrite en NVS (évite les écritures inutiles)
uint32_t decoupeuseSessionSec = 0; // durée de la session en cours (remise à 0 à chaque nouvelle connexion)
unsigned long decoupeuseLastSaveMs = 0;
bool decoupeuseEffectivePrevOn = false;  // état effectif (ON et sécurité verte) au cycle précédent

bool compressorOn = false;
bool heaterOn = false;
unsigned long lastCompressorOnMs = 0;
unsigned long lastCompressorOffMs = 0;

bool relayClosed = false;
bool safetyGreen = false;  // relais fermé ET hors bypass = LED verte fixe (voir updateInterlock)
bool bypassActive = false;
unsigned long bypassStartMs = 0;
bool bathOverTemp  = false;   // verrouillées avec hystérésis (voir updateOverTemp)
bool laserOverTemp = false;
const char* blockReason = "";  // cause lisible du blocage (OLED + web), "" = OK

// Cause du dernier redémarrage (watchdog, crash, brownout...) affichée sur le web.
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

unsigned long lastRegulationMs = 0;

// ---------------------------------------------------------------------------
// Sondes
// ---------------------------------------------------------------------------
// Lecture d'une sonde par adresse ROM ; NAN si absente, CRC faux ou valeur hors plage.
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

// Débit eau laser (lu à chaque tour de loop() : coupure sans attendre le cycle de 2 s)
void readFlow() {
  bool present = (digitalRead(PIN_DEBIT) == LOW);
  unsigned long now = millis();
  if (present && !flowPresent) flowSinceMs = now;
  flowPresent = present;
  // Asymétrique : perte de débit = flowOk faux tout de suite ; retour = après FLOW_STABLE_MS.
  flowOk = present && (now - flowSinceMs >= FLOW_STABLE_MS);
}

// ---------------------------------------------------------------------------
// Compteur d'utilisation découpeuse (temps ON cumulé + historique connexions)
// ---------------------------------------------------------------------------
bool timeIsSynced() {
  return time(nullptr) > 1700000000;  // ~2023 : simple test "NTP a déjà répondu au moins une fois"
}

void loadDecoupeuseState() {
  prefs.begin("decoup", false);
  decoupeuseTotalSec = prefs.getUInt("totalSec", 0);
  decoupeuseSavedSec = decoupeuseTotalSec;
  size_t n = prefs.getBytes("log", &decoupeuseLog, sizeof(decoupeuseLog));
  if (n != sizeof(decoupeuseLog)) {
    memset(&decoupeuseLog, 0, sizeof(decoupeuseLog));
  }
  prefs.end();
}

void saveDecoupeuseTotal() {
  if (decoupeuseTotalSec == decoupeuseSavedSec) return;  // rien de neuf, on épargne la flash
  prefs.begin("decoup", false);
  prefs.putUInt("totalSec", decoupeuseTotalSec);
  prefs.end();
  decoupeuseSavedSec = decoupeuseTotalSec;
  decoupeuseLastSaveMs = millis();
}

void saveDecoupeuseLog() {
  prefs.begin("decoup", false);
  prefs.putBytes("log", &decoupeuseLog, sizeof(decoupeuseLog));
  prefs.end();
}

void logDecoupeuseConnection() {
  uint32_t ts = timeIsSynced() ? (uint32_t)time(nullptr) : 0;
  decoupeuseLog.timestamps[decoupeuseLog.nextIdx] = ts;
  decoupeuseLog.durationsSec[decoupeuseLog.nextIdx] = 0;  // durée pas encore connue, session en cours
  decoupeuseLog.nextIdx = (decoupeuseLog.nextIdx + 1) % DECOUPEUSE_LOG_SIZE;
  if (decoupeuseLog.count < DECOUPEUSE_LOG_SIZE) decoupeuseLog.count++;
  saveDecoupeuseLog();
}

void updateDecoupeuseTimer() {
  // Le tube ne tourne "pour de vrai" que si la découpeuse est ON ET que la sécurité
  // autorise (LED verte fixe) — un signal découpeuse ON pendant que la sécurité bloque
  // ne doit pas compter dans l'usure du tube, appelé à updateInterlock() (doit être
  // exécuté avant, voir loop()).
  bool effectiveOn = decoupeuseOn && safetyGreen;

  if (effectiveOn && !decoupeuseEffectivePrevOn) {
    logDecoupeuseConnection();  // nouveau front OFF->ON (effectif) = nouvelle "connexion"
    decoupeuseSessionSec = 0;
  }

  if (effectiveOn) {
    decoupeuseTotalSec += REGULATION_INTERVAL_MS / 1000UL;
    decoupeuseSessionSec += REGULATION_INTERVAL_MS / 1000UL;
    // Mise à jour en RAM seulement (affichage live) — la flash n'est écrite qu'à la fin de la session.
    int curIdx = ((int)decoupeuseLog.nextIdx - 1 + DECOUPEUSE_LOG_SIZE) % DECOUPEUSE_LOG_SIZE;
    decoupeuseLog.durationsSec[curIdx] = decoupeuseSessionSec;
  }

  // Sauvegarde en flash : à la fin d'une session, ou périodiquement si elle dure longtemps.
  bool justTurnedOff = (!effectiveOn && decoupeuseEffectivePrevOn);
  bool periodicSave = effectiveOn && (millis() - decoupeuseLastSaveMs > DECOUPEUSE_AUTOSAVE_MS);
  if (justTurnedOff || periodicSave) {
    saveDecoupeuseTotal();
  }
  if (justTurnedOff) {
    saveDecoupeuseLog();  // fige la durée finale de la session qui vient de se terminer
  }

  decoupeuseEffectivePrevOn = effectiveOn;
}

void resetDecoupeuseTimer() {
  decoupeuseTotalSec = 0;
  prefs.begin("decoup", false);
  prefs.putUInt("totalSec", 0);
  prefs.end();
  decoupeuseSavedSec = 0;
  decoupeuseLastSaveMs = millis();
}

// ---------------------------------------------------------------------------
// Régulation thermique (hystérésis à deux bandes + anti-cycling compresseur)
// ---------------------------------------------------------------------------
void updateRegulation() {
  unsigned long now = millis();

  if (isnan(bathTemp)) {
    // Sonde bain en défaut : on ne régule pas à l'aveugle, on coupe tout.
    if (compressorOn) {
      compressorOn = false;
      lastCompressorOffMs = now;
      digitalWrite(PIN_COMPRESSOR, LOW);
    }
    if (heaterOn) {
      heaterOn = false;
      digitalWrite(PIN_HEATER, LOW);
    }
    return;
  }

  // Refroidissement (compresseur), avec anti-cycling min ON / min OFF.
  if (!compressorOn
      && bathTemp > COOL_ON_THRESHOLD_C
      && !heaterOn
      && (now - lastCompressorOffMs) > COMPRESSOR_MIN_OFF_MS) {
    compressorOn = true;
    lastCompressorOnMs = now;
    digitalWrite(PIN_COMPRESSOR, HIGH);
  } else if (compressorOn
      && bathTemp < COOL_OFF_THRESHOLD_C
      && (now - lastCompressorOnMs) > COMPRESSOR_MIN_ON_MS) {
    compressorOn = false;
    lastCompressorOffMs = now;
    digitalWrite(PIN_COMPRESSOR, LOW);
  }

  // Chauffage (canne) — jamais simultané avec le compresseur.
  if (!heaterOn && bathTemp < HEAT_ON_THRESHOLD_C && !compressorOn) {
    heaterOn = true;
    digitalWrite(PIN_HEATER, HIGH);
  } else if (heaterOn && bathTemp > HEAT_OFF_THRESHOLD_C) {
    heaterOn = false;
    digitalWrite(PIN_HEATER, LOW);
  }
}

// ---------------------------------------------------------------------------
// Interlock sécurité MYJG
// ---------------------------------------------------------------------------
// Surchauffe avec hystérésis (état conservé tant que la sonde est en défaut :
// le défaut sonde bloque de toute façon).
static void updateOverTemp(float t, float maxC, bool &flag) {
  if (isnan(t)) return;
  if (t > maxC) flag = true;
  else if (t < maxC - SAFETY_TEMP_HYST_C) flag = false;
}

void updateInterlock() {
  unsigned long now = millis();

  // Auto-révocation du bypass (jamais persisté, jamais permanent).
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
  // Etat "réellement autorisé" = LED verte fixe allumée (rouge éteinte) : sécurité vraiment
  // satisfaite, hors bypass maintenance (qui ferme le relais mais n'allume pas la verte).
  safetyGreen = relayClosed && !bypassActive;

  digitalWrite(PIN_RELAY, relayClosed ? HIGH : LOW);
}

// ---------------------------------------------------------------------------
// LED rouge/verte — seuls indicateurs de sécurité fiables (écran endommagé)
// ---------------------------------------------------------------------------
void updateLEDs() {
  if (bypassActive) {
    bool blink = (millis() / LED_BLINK_PERIOD_MS) % 2;
    digitalWrite(PIN_LED_ROUGE, blink ? HIGH : LOW);
    digitalWrite(PIN_LED_VERTE, LOW);
  } else if (relayClosed) {
    digitalWrite(PIN_LED_VERTE, HIGH);
    digitalWrite(PIN_LED_ROUGE, LOW);
  } else {
    digitalWrite(PIN_LED_VERTE, LOW);
    digitalWrite(PIN_LED_ROUGE, HIGH);
  }
}

// ---------------------------------------------------------------------------
// Écran (informatif uniquement — jamais utilisé pour la sécurité)
// ---------------------------------------------------------------------------
void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // OLED bicolore : bande JAUNE = lignes 0-15 (16 px), BLEU = lignes 16-63.
  // Statut sécurité en taille 2 (16 px de haut) centré dans la bande jaune —
  // informatif uniquement, les LED rouge/verte restent seules garantes
  // (voir updateLEDs()), jamais l'écran.
  const char* status = relayClosed ? "AUTORISE" : "BLOQUE";
  display.setTextSize(2);
  display.setCursor((SCREEN_WIDTH - (int)strlen(status) * 12) / 2, 0);
  display.print(status);

  // Zone bleue : températures puis cause du blocage / bypass.
  display.setTextSize(1);
  display.setCursor(0, 22);
  display.print("Bain:  ");
  if (isnan(bathTemp)) display.print("---"); else display.printf("%.1fC", bathTemp);

  display.setCursor(0, 36);
  display.print("Laser: ");
  if (isnan(laserTemp)) display.print("---"); else display.printf("%.1fC", laserTemp);

  display.setCursor(0, 54);
  if (bypassActive)            display.print("BYPASS ACTIF");
  else if (blockReason[0])     display.print(blockReason);

  display.display();
}

// ---------------------------------------------------------------------------
// Site web (authentifié, bypass = bypass total pour maintenance)
// ---------------------------------------------------------------------------
String formatDuration(uint32_t totalSec) {
  uint32_t h = totalSec / 3600;
  uint32_t m = (totalSec % 3600) / 60;
  uint32_t s = totalSec % 60;
  char buf[40];
  snprintf(buf, sizeof(buf), "%lu h %02lu min %02lu s", (unsigned long)h, (unsigned long)m, (unsigned long)s);
  return String(buf);
}

String formatTimestamp(uint32_t ts) {
  if (ts == 0) return "heure inconnue";
  time_t t = (time_t)ts;
  struct tm tmInfo;
  localtime_r(&t, &tmInfo);
  char buf[24];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &tmInfo);
  return String(buf);
}

bool checkAuth() {
  if (!server.authenticate(WEB_AUTH_USER, WEB_AUTH_PASSWORD)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() {
  if (!checkAuth()) return;

  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<title>Regulation bain laser</title>";
  html += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px}"
           ".ok{color:#4caf50}.bad{color:#f44336}.warn{color:#ff9800}"
           "button{padding:10px 20px;margin:5px 0;font-size:1em}</style></head><body>";

  html += "<h1>Regulation bain refroidissement laser</h1>";

  html += "<p>Temp&eacute;rature eau bain : ";
  html += isnan(bathTemp) ? "<span class='bad'>ERREUR SONDE</span>" : (String(bathTemp, 1) + " &deg;C");
  html += "</p>";

  html += "<p>Temp&eacute;rature eau laser : ";
  html += isnan(laserTemp) ? "<span class='bad'>ERREUR SONDE</span>" : (String(laserTemp, 1) + " &deg;C");
  html += "</p>";

  html += "<p>Compresseur : " + String(compressorOn ? "ON" : "OFF") + "</p>";
  html += "<p>Canne chauffante : " + String(heaterOn ? "ON" : "OFF") + "</p>";
  html += "<p>Debit eau laser : ";
  html += flowOk ? "<span class='ok'>OK</span>"
         : flowPresent ? "<span class='warn'>stabilisation...</span>"
         : "<span class='bad'>NUL (ou contacteur debranche)</span>";
  html += "</p>";
  html += "<p>Decoupeuse : " + String(decoupeuseOn ? "ON" : "OFF") + "</p>";

  html += "<p>Temps cumule decoupeuse, securite autorisee uniquement (usure tube laser) : ";
  html += formatDuration(decoupeuseTotalSec) + "</p>";
  html += "<form method='POST' action='/decoupeuse/raz' onsubmit=\"return confirm("
          "'RAZ du compteur decoupeuse - a faire uniquement lors du changement de tube laser. Confirmer ?');\">";
  html += "<button>RAZ compteur decoupeuse</button></form>";

  html += "<h3>10 dernieres mises en route decoupeuse</h3><ul>";
  if (decoupeuseLog.count == 0) {
    html += "<li>Aucune connexion enregistree</li>";
  } else {
    int curIdx = ((int)decoupeuseLog.nextIdx - 1 + DECOUPEUSE_LOG_SIZE) % DECOUPEUSE_LOG_SIZE;
    for (int i = 0; i < decoupeuseLog.count; i++) {
      int idx = ((int)decoupeuseLog.nextIdx - 1 - i + DECOUPEUSE_LOG_SIZE * 2) % DECOUPEUSE_LOG_SIZE;
      html += "<li>" + formatTimestamp(decoupeuseLog.timestamps[idx]);
      html += " - duree : " + formatDuration(decoupeuseLog.durationsSec[idx]);
      // "En cours" = session réellement comptée (découpeuse ON ET sécurité autorisée),
      // même état que celui utilisé par updateDecoupeuseTimer().
      if (idx == curIdx && decoupeuseEffectivePrevOn) html += " (en cours)";
      html += "</li>";
    }
  }
  html += "</ul>";

  html += "<p>Securite MYJG : <span class='" + String(relayClosed ? "ok" : "bad") + "'>";
  html += String(relayClosed ? "AUTORISE" : "BLOQUE") + "</span></p>";
  if (!relayClosed && blockReason[0]) {
    html += "<p>Cause : <span class='bad'>" + String(blockReason) + "</span></p>";
  }
  bool badReset = (bootResetReason == ESP_RST_INT_WDT || bootResetReason == ESP_RST_TASK_WDT
                   || bootResetReason == ESP_RST_WDT || bootResetReason == ESP_RST_PANIC
                   || bootResetReason == ESP_RST_BROWNOUT);
  html += "<p>Dernier redemarrage : <span class='" + String(badReset ? "bad" : "ok") + "'>";
  html += String(resetReasonStr(bootResetReason)) + "</span></p>";

  if (bypassActive) {
    unsigned long remainingMs = BYPASS_TIMEOUT_MS - (millis() - bypassStartMs);
    html += "<p class='warn'>&#9888; BYPASS MAINTENANCE ACTIF (protections thermiques et debit ignorees) ";
    html += "- desactivation auto dans " + String(remainingMs / 60000) + " min</p>";
    html += "<form method='POST' action='/bypass/off'><button>Desactiver le bypass</button></form>";
  } else {
    html += "<form method='POST' action='/bypass/on' onsubmit=\"return confirm("
            "'Forcer la fermeture du relais de securite pour maintenance ? "
            "Ceci ignore TOUTES les protections (sondes, surchauffe bain et laser, debit) pendant 15 min.');\">";
    html += "<button>Forcer fermeture (maintenance)</button></form>";
  }

  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleBypassOn() {
  if (!checkAuth()) return;
  bypassActive = true;
  bypassStartMs = millis();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleBypassOff() {
  if (!checkAuth()) return;
  bypassActive = false;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleDecoupeuseRaz() {
  if (!checkAuth()) return;
  resetDecoupeuseTimer();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/bypass/on", HTTP_POST, handleBypassOn);
  server.on("/bypass/off", HTTP_POST, handleBypassOff);
  server.on("/decoupeuse/raz", HTTP_POST, handleDecoupeuseRaz);
  server.begin();
}

// Aide au relevé des adresses ROM (à reporter dans config.h).
void printSensorAddresses() {
  sensors.requestTemperatures();
  int n = sensors.getDeviceCount();
  Serial.printf("DS18B20 detectees : %d\n", n);
  if (n < 2) {
    Serial.println("ATTENTION : moins de 2 sondes detectees sur le bus 1-Wire !");
  }
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

// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  bootResetReason = esp_reset_reason();
  Serial.printf("Cause du redemarrage : %s\n", resetReasonStr(bootResetReason));

  // États de repli sûrs avant même la config WiFi/écran : sécurité ouverte,
  // charges éteintes, LED rouge = "pas encore validé".
  pinMode(PIN_COMPRESSOR, OUTPUT); digitalWrite(PIN_COMPRESSOR, LOW);
  pinMode(PIN_HEATER, OUTPUT);     digitalWrite(PIN_HEATER, LOW);
  pinMode(PIN_RELAY, OUTPUT);      digitalWrite(PIN_RELAY, LOW);
  pinMode(PIN_LED_ROUGE, OUTPUT);  digitalWrite(PIN_LED_ROUGE, HIGH);
  pinMode(PIN_LED_VERTE, OUTPUT);  digitalWrite(PIN_LED_VERTE, LOW);
  pinMode(PIN_DECOUPEUSE, INPUT_PULLUP);  // pull-up interne : débranché = HIGH = OFF
  pinMode(PIN_DEBIT, INPUT_PULLUP);       // ILS ouvert / débranché = HIGH = pas de débit = BLOQUE

  loadDecoupeuseState();

  sensors.begin();
  printSensorAddresses();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connexion WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connecte, IP: %s\n", WiFi.localIP().toString().c_str());
    configTzTime(TZ_PARIS, NTP_SERVER_1, NTP_SERVER_2);  // horloge pour l'horodatage des connexions
  } else {
    Serial.println("\nWiFi non connecte (timeout) - la regulation/securite continue sans le site web.");
  }

  setupWebServer();

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    // Écran informatif uniquement (jamais utilisé pour la sécurité) : une panne
    // ici ne doit jamais bloquer la régulation/l'interlock, on log et on continue.
    Serial.println("Echec init OLED SSD1306 (informatif uniquement, sans impact securite)");
  }
  display.clearDisplay();
  display.display();

  // Watchdog sur loop() — activé en dernier, après le WiFi (connexion bloquante
  // jusqu'à 15 s). Le core le nourrit à chaque retour de loop() ; délai 5 s
  // (CONFIG_ESP_TASK_WDT_TIMEOUT_S, core 2.0.17, panic=reset). Un loop() figé
  // => reset => GPIO27 retombe => relais sécurité ouvert.
  enableLoopWDT();
}

void loop() {
  server.handleClient();

  // Débit + interlock à chaque tour : une perte de débit coupe le relais en quelques ms,
  // sans attendre le cycle de régulation (températures en cache, appel peu coûteux).
  readFlow();
  updateInterlock();

  unsigned long now = millis();
  if (now - lastRegulationMs >= REGULATION_INTERVAL_MS) {
    lastRegulationMs = now;
    readSensors();
    updateRegulation();
    updateInterlock();        // températures fraîches
    updateDecoupeuseTimer();  // après l'interlock : a besoin de safetyGreen à jour
    updateDisplay();
  }

  // Rafraîchi à chaque tour pour un clignotement fluide pendant le bypass.
  updateLEDs();

#ifdef TEST_WATCHDOG
  if (millis() > 90000UL) {
    Serial.printf("TEST_WATCHDOG : blocage de loop() a millis=%lu\n", millis());
    Serial.flush();
    while (true) { }
  }
#endif
}
