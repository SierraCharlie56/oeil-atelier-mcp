// ─────────────────────────────────────────────────────────────────────────────
//  WindS400 Advansea — Banc d'essai girouette
//  Plateforme : ESP8266 NodeMCU v2
//  Signal     : PWM brut fil Bleu via pont diviseur 10kΩ/15kΩ → GPIO4
//  Protocole  : période fixe ~1280 µs, DC 100% = 0°, DC 0% = 360°
//  Interface  : page web + compas animé, rafraîchissement 500 ms
// ─────────────────────────────────────────────────────────────────────────────

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── WiFi AP ───────────────────────────────────────────────────────────────────
const char* AP_SSID = "WindS400-Banc";
const char* AP_PASS = "wind1234";

// ── GPIO ─────────────────────────────────────────────────────────────────────
const int DIR_PIN = D2;  // fil Bleu girouette (via pont div 10k/15k → 2,9V max)
const int SPD_PIN = D4;  // fil Vert anémomètre (pull-up 10kΩ externe → 3,3V)
                         // ⚠ D4=GPIO2 doit être HIGH au boot — OK si pull-up branché

// ── Calibration PWM (auto-min/max) ───────────────────────────────────────────
// Initialisés aux valeurs mesurées — se raffinent automatiquement à l'usage.
// Faire un tour complet de girouette après le boot pour calibrer.
// Rappel : GET /reset_calib pour remettre à zéro sans reflasher.
// Toute mesure >= PERIOD_US est un parasite et sera rejetée.
const unsigned long PERIOD_US = 1280;  // µs — filtre anti-parasite
unsigned long pw_min =  99;
unsigned long pw_max = 900;

// ── État girouette ────────────────────────────────────────────────────────────
struct {
  float         angle  = 0;
  unsigned long pw_us  = 0;
  bool          valid  = false;
  unsigned long ts     = 0;
} dir;

// ── Anémomètre (comptage par interruption) ────────────────────────────────────
// 8 fentes optiques → 8 fronts descendants par tour (FALLING)
// Calibration m/s : à déterminer expérimentalement (SPD_K = facteur)
// Calibration 29/06/2026 — sèche-cheveux + anémomètre de référence, 8 points 2.8–6.4 m/s.
// Points retenus : 4.8, 5.7, 6.4 m/s (les plus stables). Erreur max ±3%.
// SPD_K = m/s × 7.5 / tr/min  (7.5 = 60s / 8 fentes)
const float           SPD_K      = 0.031f; // m/s par Hz impulsions
volatile unsigned long spd_count = 0;     // compteur brut (ISR)
float                 spd_hz     = 0;     // fréquence courante (Hz)
float                 spd_ms     = 0;     // vitesse vent (m/s, provisoire)
unsigned long         spd_last_count = 0;
unsigned long         spd_last_ts   = 0;

void IRAM_ATTR onSpdPulse() { spd_count++; }

ESP8266WebServer server(80);

// ── Lecture PWM → angle (moyenne de N échantillons) ──────────────────────────
// Retourne l'angle en degrés, met à jour dir.pw_us avec la valeur brute.
float readAngle(int samples = 5) {
  long  sum = 0;
  int   ok  = 0;
  for (int i = 0; i < samples; i++) {
    unsigned long pw = pulseIn(DIR_PIN, HIGH, 3000);
    if (pw > 0 && pw < PERIOD_US) { sum += pw; ok++; }
  }
  if (ok == 0) return -1;
  float pw_avg = (float)sum / ok;
  dir.pw_us = (unsigned long)pw_avg;
  if (pw_avg < pw_min) pw_min = (unsigned long)pw_avg;
  if (pw_avg > pw_max) pw_max = (unsigned long)pw_avg;
  if (pw_max <= pw_min) return -1;
  float angle = (pw_avg - pw_min) * 360.0f / (pw_max - pw_min);
  if (angle <     0) angle =   0;
  if (angle > 359.9f) angle = 359.9f;
  return angle;
}

// ── JSON pour /data ───────────────────────────────────────────────────────────
String buildJSON() {
  unsigned long age = (millis() - dir.ts) / 1000;
  float rpm = spd_hz * 60.0f / 8.0f;
  return "{\"angle\":"  + String(dir.angle, 1) +
         ",\"pw_us\":"  + String(dir.pw_us) +
         ",\"pw_min\":" + String(pw_min) +
         ",\"pw_max\":" + String(pw_max) +
         ",\"spd_hz\":" + String(spd_hz, 2) +
         ",\"rpm\":"    + String(rpm, 1) +
         ",\"spd_ms\":" + String(spd_ms, 2) +
         ",\"valid\":"  + (dir.valid ? "true" : "false") +
         ",\"age\":"    + String(age) + "}";
}

// ── Page HTML embarquée ───────────────────────────────────────────────────────
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WindS400 — Girouette</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:Arial,sans-serif;background:#1a1a2e;color:#eee;padding:20px;text-align:center}
  h1{color:#4fc3f7;font-size:1.5em;margin-bottom:4px}
  .sub{color:#78909c;font-size:.85em;margin-bottom:16px}
  .badge{display:inline-block;padding:4px 14px;border-radius:20px;font-size:.8em;margin-bottom:16px}
  .ok{background:#1b5e20;color:#69f0ae}
  .ko{background:#b71c1c;color:#ff8a80}
  .row{display:flex;flex-wrap:wrap;justify-content:center;gap:16px}
  .card{background:#16213e;border-radius:12px;padding:20px;min-width:180px}
  .big{font-size:3em;font-weight:bold;color:#4fc3f7;line-height:1}
  .lbl{font-size:.8em;color:#546e7a;margin-top:6px}
  .age{font-size:.75em;color:#37474f;margin-top:12px}
  canvas{display:block;margin:0 auto}
</style>
</head>
<body>
<h1>WindS400 — Girouette</h1>
<p class="sub">Banc d'essai · Signal PWM brut fil Bleu</p>
<div id="badge" class="badge ko">En attente...</div>
<div class="row">
  <div class="card">
    <canvas id="c" width="210" height="210"></canvas>
    <div class="lbl">Compas</div>
  </div>
  <div class="card" style="min-width:140px">
    <div class="big" id="deg">---</div>
    <div class="lbl">Degrés (relatif)</div>
    <div class="age" id="age"></div>
  </div>
  <div class="card" style="min-width:140px">
    <div class="big" id="ms">---</div>
    <div class="lbl">m/s</div>
    <div style="font-size:1.4em;font-weight:bold;color:#81d4fa;margin-top:14px" id="rpm">---</div>
    <div class="lbl">tr/min</div>
    <div style="font-size:1em;color:#546e7a;margin-top:8px" id="hz">---</div>
    <div class="lbl">Hz</div>
  </div>
</div>
<script>
const cv=document.getElementById('c'),ctx=cv.getContext('2d'),cx=105,cy=105,R=95;

function draw(a){
  ctx.clearRect(0,0,210,210);
  ctx.beginPath();ctx.arc(cx,cy,R,0,2*Math.PI);
  ctx.fillStyle='#0d1b2a';ctx.fill();
  ctx.strokeStyle='#263238';ctx.lineWidth=2;ctx.stroke();
  for(let i=0;i<360;i+=10){
    const r=(i-90)*Math.PI/180,big=i%30===0;
    ctx.beginPath();
    ctx.moveTo(cx+(R-2)*Math.cos(r),cy+(R-2)*Math.sin(r));
    ctx.lineTo(cx+(R-(big?13:6))*Math.cos(r),cy+(R-(big?13:6))*Math.sin(r));
    ctx.strokeStyle=big?'#78909c':'#37474f';ctx.lineWidth=big?2:1;ctx.stroke();
  }
  [['N',0,'#ef5350'],['E',90,'#cfd8dc'],['S',180,'#cfd8dc'],['O',270,'#cfd8dc']].forEach(([l,d,c])=>{
    const r=(d-90)*Math.PI/180;
    ctx.fillStyle=c;ctx.font='bold 13px Arial';ctx.textAlign='center';ctx.textBaseline='middle';
    ctx.fillText(l,cx+(R-22)*Math.cos(r),cy+(R-22)*Math.sin(r));
  });
  if(a==null){
    ctx.beginPath();ctx.arc(cx,cy,5,0,2*Math.PI);ctx.fillStyle='#37474f';ctx.fill();
    return;
  }
  const rad=(a-90)*Math.PI/180,L=R-28,perp=rad+Math.PI/2;
  ctx.beginPath();
  ctx.moveTo(cx-0.3*L*Math.cos(rad),cy-0.3*L*Math.sin(rad));
  ctx.lineTo(cx+L*Math.cos(rad),cy+L*Math.sin(rad));
  ctx.strokeStyle='#4fc3f7';ctx.lineWidth=3;ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(cx+L*Math.cos(rad),cy+L*Math.sin(rad));
  ctx.lineTo(cx+(L-14)*Math.cos(rad)+7*Math.cos(perp),cy+(L-14)*Math.sin(rad)+7*Math.sin(perp));
  ctx.lineTo(cx+(L-14)*Math.cos(rad)-7*Math.cos(perp),cy+(L-14)*Math.sin(rad)-7*Math.sin(perp));
  ctx.closePath();ctx.fillStyle='#4fc3f7';ctx.fill();
  ctx.beginPath();ctx.arc(cx,cy,5,0,2*Math.PI);ctx.fillStyle='#80deea';ctx.fill();
}
draw(null);

async function refresh(){
  try{
    const d=await(await fetch('/data')).json();
    const ok=d.valid&&d.age<5;
    const badge=document.getElementById('badge');
    badge.className='badge '+(ok?'ok':'ko');
    badge.textContent=ok?'Signal PWM reçu ✓':d.age>5?'Signal perdu ('+d.age+'s)':'En attente...';
    if(ok){
      draw(d.angle);
      document.getElementById('deg').textContent=Math.round(d.angle)+'°';
      document.getElementById('age').textContent='Mis à jour il y a '+d.age+'s';
      document.getElementById('ms').textContent=d.spd_ms.toFixed(1);
      document.getElementById('rpm').textContent=Math.round(d.rpm);
      document.getElementById('hz').textContent=d.spd_hz.toFixed(2)+' Hz';
    }else{
      draw(null);
    }
  }catch(e){
    document.getElementById('badge').textContent='Erreur réseau';
  }
}
refresh();
setInterval(refresh,500);
</script>
</body>
</html>
)rawliteral";

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(DIR_PIN, INPUT);
  pinMode(SPD_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(SPD_PIN), onSpdPulse, FALLING);
  spd_last_ts = millis();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[WiFi] AP démarré — IP : ");
  Serial.println(WiFi.softAPIP());  // 192.168.4.1

  server.on("/", []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/data", []() { server.send(200, "application/json", buildJSON()); });
  server.on("/reset_calib", []() {
    pw_min = 9999; pw_max = 0;
    server.send(200, "text/plain", "Calibration remise a zero — tournez la girouette a fond dans chaque sens.");
    Serial.println("[CALIB] reset");
  });
  server.begin();
  Serial.println("[HTTP] Serveur démarré");
  Serial.println("[DIR]  Lecture PWM sur D2 (fil Bleu via pont div 10k/15k)");
  Serial.println("[SPD]  Comptage impulsions sur D4 (fil Vert, pull-up 10k)");
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Girouette
  float a = readAngle(5);
  if (a >= 0) {
    dir.angle = a;
    dir.valid = true;
    dir.ts    = millis();
  }

  // Anémomètre : calcul fréquence toutes les secondes
  unsigned long now = millis();
  if (now - spd_last_ts >= 1000) {
    unsigned long elapsed = now - spd_last_ts;
    unsigned long cnt;
    noInterrupts();
    cnt = spd_count - spd_last_count;
    spd_last_count = spd_count;
    interrupts();
    spd_hz  = cnt * 1000.0f / elapsed;
    spd_ms  = spd_hz * SPD_K;
    spd_last_ts = now;
    Serial.printf("[DIR] %.1f°  [SPD] %.2f Hz  %.1f tr/min\n",
                  dir.angle, spd_hz, spd_hz * 60.0f / 8.0f);
  }

  server.handleClient();
}
