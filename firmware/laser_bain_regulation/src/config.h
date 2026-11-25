#pragma once

// ---- Broches (voir projet_regulation_laser.md pour le schéma complet) ----
#define PIN_COMPRESSOR   13  // Canal 1 compresseur : GPIO13 -> R_LED 180 Ohm -> MOC3023 broche 1 (anode) -> BTA16
#define PIN_HEATER       14  // Canal 2 canne chauffante : GPIO14 -> R_LED 220 Ohm -> MOC3023 broche 1 (anode) -> BTA16
#define PIN_RELAY        27  // Transistor -> relais sécurité MYJG (HIGH = bobine alimentée = contact fermé = autorisé)
#define PIN_LED_ROUGE    26
#define PIN_LED_VERTE    25
#define PIN_ONEWIRE      19  // Bus 1-Wire, DS18B20 x2 (+ pull-up 4,7kOhm) -- GPIO16 non disponible sur cette carte (non sorti sur le connecteur)
#define PIN_DECOUPEUSE   33  // pull-up INTERNE (INPUT_PULLUP) ; contact ferme à la masse = ON -> lecture LOW ; débranché = HIGH = OFF (fail-safe sans composant externe)
#define PIN_DEBIT        18  // Contacteur de débit 新达 (ILS NO, 2 fils) entre GPIO18 et GND ; INPUT_PULLUP (+ 10k/100nF externes)
                             // débit présent = ILS fermé = LOW ; débit nul / fil coupé = HIGH = BLOQUE (fail-safe)

// ---- Écran OLED SSD1306 0,96" 128x64, I2C (carte 2026-09-05, ESP32 nu sans écran intégré) ----
#define PIN_OLED_SDA     21
#define PIN_OLED_SCL     22
#define OLED_I2C_ADDR    0x3C
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64

// Adresses ROM des DS18B20 — relevées au banc (voir moniteur série au démarrage).
// Adressage par ROM et non par index : getTempCByIndex() refait une recherche sur le
// bus à chaque appel, une sonde débranchée décalerait l'index de l'autre (inversion
// silencieuse bain/laser).
// Tant qu'elles valent ces valeurs factices, les sondes sont lues "absentes" :
// régulation coupée + sécurité BLOQUÉE (fail-safe).
// Relevées au banc le 2026-11-25 (sonde bain chauffée à la main : 17,7 -> 31,3 °C).
#define ADDR_SONDE_BAIN   {0x28, 0x30, 0x4F, 0xD3, 0x00, 0x00, 0x00, 0xFC}
#define ADDR_SONDE_LASER  {0x28, 0x4F, 0xE3, 0xC9, 0x00, 0x00, 0x00, 0x71}

// Plage de température plausible ; hors plage = sonde considérée en défaut.
// (85,0 °C = valeur de reset à la mise sous tension d'un DS18B20 : exclue.)
#define PROBE_MIN_VALID_C  -10.0f
#define PROBE_MAX_VALID_C   80.0f

// ---- Régulation thermique (hystérésis à deux bandes séparées) ----
#define COOL_ON_THRESHOLD_C   22.5f
#define COOL_OFF_THRESHOLD_C  22.0f
#define HEAT_ON_THRESHOLD_C   21.0f
#define HEAT_OFF_THRESHOLD_C  21.5f

// ---- Anti-cycling compresseur ----
#define COMPRESSOR_MIN_OFF_MS  (4UL * 60UL * 1000UL)  // 4 min mini entre deux démarrages
#define COMPRESSOR_MIN_ON_MS   (2UL * 60UL * 1000UL)  // 2 min mini avant un arrêt

// ---- Sécurité interlock ----
// Au-delà de ce seuil, le refroidissement est jugé défaillant -> interlock ouvert.
#define SAFETY_MAX_BATH_TEMP_C  26.0f
// Eau laser : seuil au-delà duquel le tube est jugé insuffisamment refroidi.
// Sonde collée sur le DÉPART (eau entrant dans le tube) : proche du bain (~22 °C).
// 25 °C décidé le 2026-11-25 (30 °C prévu pour une sonde en sortie de tube).
// À valider en eau sous charge réelle : écart bain -> départ via le serpentin.
#define SAFETY_MAX_LASER_TEMP_C  25.0f
// Hystérésis de ré-autorisation après surchauffe (bain et laser) :
// blocage au-dessus du seuil, ré-autorisation seulement sous (seuil - hystérésis).
#define SAFETY_TEMP_HYST_C        1.0f
// Débit : coupure IMMÉDIATE dès que l'ILS s'ouvre ; ré-autorisation seulement après
// un débit stable pendant ce délai (évite qu'une palette qui papillonne fasse claquer le relais).
#define FLOW_STABLE_MS          5000UL

// ---- Bypass maintenance (bypass TOTAL, voir projet_regulation_laser.md) ----
#define BYPASS_TIMEOUT_MS  (15UL * 60UL * 1000UL)  // 15 min, désactivation auto

// ---- Timing général ----
#define REGULATION_INTERVAL_MS   2000UL   // cycle lecture sondes + régulation + interlock
#define WIFI_CONNECT_TIMEOUT_MS  15000UL
#define LED_BLINK_PERIOD_MS      300UL    // clignotement LED rouge pendant le bypass

// ---- Compteur d'utilisation découpeuse (temps ON cumulé + historique) ----
// Sert à suivre l'usure du tube laser ; RAZ manuelle lors d'un changement de tube.
#define DECOUPEUSE_LOG_SIZE      10       // nombre de connexions (fronts OFF->ON) conservées
#define DECOUPEUSE_AUTOSAVE_MS   (5UL * 60UL * 1000UL)  // sauvegarde flash périodique si session longue
#define NTP_SERVER_1             "pool.ntp.org"
#define NTP_SERVER_2             "time.google.com"
#define TZ_PARIS                 "CET-1CEST,M3.5.0,M10.5.0/3"

// ---- Test watchdog (temporaire) ----
// #define TEST_WATCHDOG   // simule un blocage de loop() 90 s après le boot — NE JAMAIS LAISSER ACTIF
