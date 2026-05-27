#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include "LittleFS.h"
#include <time.h>
#include <algorithm>
#include <esp_task_wdt.h>

#define MAX_RECORDS 100
#define GRADE_COUNT 11
#define WDT_TIMEOUT 15
#define TOUCH_THRESHOLD 30 // Threshold para sa Pin 4

// --- WIFI CREDENTIALS ---
const char* ssid = "ESP32_Attendance_System"; 
const char* password = "@YattendanceSystem"; 

const char* router_ssid = "HONOR";
const char* router_pass = "12345678";

LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);
HardwareSerial QRSerial(2);

// ================= SYSTEM STATE =================
bool systemEnabled = true;   // FIX: true na by default para consistent sa backlight
bool loggedIn = false;
bool adminQR_Detected = false;
String lastDateInSystem = "";
unsigned long lastScanMillis = 0;

// ================= LABELS =================
String txt_Dashboard = "MARANATHA DASHBOARD";
String txt_Login     = "MARANATHA ADMIN LOGIN";
String txt_Username  = "Username";
String txt_Pass      = "Password";
String txt_BtnSignIn = "SIGN IN";
String txt_Stats     = "STATISTICS";
String txt_Logout    = "LOGOUT";
String txt_Back      = "BACK";
String txt_Clear     = "RESET ALL";
String txt_SaveCSV   = "SAVE CSV";

struct Record {
  String name;
  String section;
  String date;
  String time;
};

Record records[GRADE_COUNT][MAX_RECORDS];
int recordCount[GRADE_COUNT] = {0};
String gradeNames[GRADE_COUNT] = {"K1","G1","G2","G3","G4","G5","G6","G7","G8","G9","G10"};

// ================= UTILITIES =================

String getDate() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "0000-00-00";
  char buffer[11];
  strftime(buffer, 11, "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String getTimeOnly() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) return "00:00 AM";
  char buffer[12];
  strftime(buffer, 12, "%I:%M %p", &timeinfo);
  return String(buffer);
}

void saveToFile() {
  if (LittleFS.usedBytes() > (LittleFS.totalBytes() * 0.9)) return;
  File file = LittleFS.open("/attendance.txt", "w");
  if (!file) return;
  file.println("LAST_DATE:" + lastDateInSystem);
  for (int g = 0; g < GRADE_COUNT; g++) {
    file.println("G:" + String(g) + "|C:" + String(recordCount[g]));
    for (int i = 0; i < recordCount[g]; i++) {
      file.println(records[g][i].name + "|" + records[g][i].section + "|" + records[g][i].date + "|" + records[g][i].time);
    }
  }
  file.close();
}

void loadFromFile() {
  if (!LittleFS.exists("/attendance.txt")) return;
  File file = LittleFS.open("/attendance.txt", "r");
  if (!file) return;
  int currentG = -1;
  while (file.available()) {
    String line = file.readStringUntil('\n'); line.trim();
    if (line.startsWith("LAST_DATE:")) {
      lastDateInSystem = line.substring(10);
    } else if (line.startsWith("G:")) {
      int pipe = line.indexOf('|');
      if(pipe != -1) {
        currentG = line.substring(2, pipe).toInt();
        if(currentG < GRADE_COUNT) recordCount[currentG] = 0;
      }
    } else if (currentG != -1 && currentG < GRADE_COUNT && line.length() > 0) {
      int p1 = line.indexOf("|");
      int p2 = line.indexOf("|", p1 + 1);
      int p3 = line.lastIndexOf("|");
      if (p1 != -1 && p2 != -1 && p3 != -1 && recordCount[currentG] < MAX_RECORDS) {
        records[currentG][recordCount[currentG]].name = line.substring(0, p1);
        records[currentG][recordCount[currentG]].section = line.substring(p1 + 1, p2);
        records[currentG][recordCount[currentG]].date = line.substring(p2 + 1, p3);
        records[currentG][recordCount[currentG]].time = line.substring(p3 + 1);
        recordCount[currentG]++;
      }
    }
  }
  file.close();
}

// ================= UI HELPERS & HANDLERS =================
String getCommonHead() {
  return R"rawliteral(
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="https://unpkg.com/flickity@2/dist/flickity.min.css">
    <script src="https://unpkg.com/flickity@2/dist/flickity.pkgd.min.js"></script>
    <style>
      :root { --accent: #2ecc71; --glass: rgba(0, 30, 10, 0.75); --danger: #e74c3c; --nature: linear-gradient(135deg, #134e5e 0%, #71b280 100%); }
      @keyframes crack { 0% { transform: translate(2px, 1px); } 20% { transform: translate(-2px, -1px); } 40% { transform: translate(-2px, 2px); } 60% { transform: translate(2px, 1px); } 100% { transform: translate(0,0); } }
      .crack-anim { animation: crack 0.15s linear; }
      body { margin: 0; font-family: 'Segoe UI', sans-serif; background: var(--nature); background-attachment: fixed; height: 100vh; display: flex; justify-content: center; align-items: center; color: white; overflow: hidden; }
      .content-box { position: relative; z-index: 2; background: var(--glass); padding: 30px; border-radius: 20px; border: 1px solid rgba(255, 255, 255, 0.2); width: 85%; max-width: 600px; text-align: center; box-shadow: 0 10px 30px rgba(0,0,0,0.5); backdrop-filter: blur(10px); }
      h2 { color: var(--accent); letter-spacing: 2px; text-transform: uppercase; margin-bottom: 25px; }
      input { width: 100%; padding: 14px; border-radius: 10px; border: none; background: rgba(255,255,255,0.9); color: #333; box-sizing: border-box; margin-bottom: 10px; outline:none; }
      .btn-action { width: 100%; padding: 14px; background: var(--accent); color: white; font-weight: bold; border: none; border-radius: 10px; cursor: pointer; }
      .dash-controls { display: flex; flex-wrap: wrap; justify-content: center; gap: 10px; margin-top: 20px; }
      .btn-outline { padding: 10px 20px; border-radius: 8px; text-decoration: none; font-weight: bold; font-size: 13px; border: 1px solid; transition: 0.3s; cursor: pointer; background: transparent; }
      .power-btn { width: 100%; padding: 15px; border-radius: 12px; font-weight: bold; cursor: pointer; border: none; margin-bottom: 20px; }
      .on { background: #2ecc71; color: white; }
      .off { background: #e74c3c; color: white; }
      .search-box { width: 100%; padding: 10px; margin-bottom: 15px; border-radius: 8px; border: 1px solid var(--accent); background: rgba(255,255,255,0.2); color: white; outline: none; }
      table { width: 100%; border-collapse: collapse; margin-top: 10px; }
      th { background: var(--accent); padding: 10px; color: white; font-size: 14px; }
      td { padding: 10px; border-bottom: 1px solid rgba(255,255,255,0.1); font-size: 13px; }
      .stat-card { background: rgba(255,255,255,0.1); padding: 10px; border-radius: 10px; margin: 5px; min-width: 80px; display: inline-block; }
    </style>
  )rawliteral";
}

void handleGrade() {
  if(!server.hasArg("g") || !loggedIn) { server.sendHeader("Location","/"); server.send(303); return; }
  int g = server.arg("g").toInt();
  if(g >= GRADE_COUNT) return;
  String page = "<!DOCTYPE html><html><head>" + getCommonHead() + "</head><body><div class='content-box'><h2>" + gradeNames[g] + "</h2>";
  page += "<input type='text' id='searchInput' class='search-box' onkeyup='searchFunc()' placeholder='Search Name...'>";
  page += "<div style='max-height:300px; overflow-y:auto;'><table id='attTable'><thead><tr><th>NAME</th><th>SECTION</th><th>TIME</th></tr></thead><tbody>";
  for(int i=0; i<recordCount[g]; i++) {
    page += "<tr><td>" + records[g][i].name + "</td><td>" + records[g][i].section + "</td><td>" + records[g][i].time + "</td></tr>";
  }
  page += "</tbody></table></div><br><a href='/' class='btn-outline' style='color:var(--accent); border-color:var(--accent);'>" + txt_Back + "</a>";
  page += R"javascript(<script>function searchFunc(){ var input=document.getElementById('searchInput'), filter=input.value.toUpperCase(), table=document.getElementById('attTable'), tr=table.getElementsByTagName('tr'); for(var i=1;i<tr.length;i++){ var td=tr[i].getElementsByTagName('td')[0]; if(td){ var val=td.textContent||td.innerText; tr[i].style.display=val.toUpperCase().indexOf(filter)>-1?'':'none'; } } }</script>)javascript";
  page += "</div></body></html>";
  server.send(200, "text/html", page);
}

void handleStats() {
  if(!loggedIn) { server.sendHeader("Location","/"); server.send(303); return; }
  String page = "<!DOCTYPE html><html><head>" + getCommonHead() + "</head><body><div class='content-box'><h2>" + txt_Stats + "</h2><div style='display:flex; flex-wrap:wrap; justify-content:center;'>";
  int total = 0;
  for(int i=0; i<GRADE_COUNT; i++) {
    page += "<div class='stat-card'><b>" + gradeNames[i] + "</b><br>" + String(recordCount[i]) + "</div>";
    total += recordCount[i];
  }
  page += "</div><h3 style='margin-top:20px;'>TOTAL: " + String(total) + "</h3><br><a href='/' class='btn-outline' style='color:var(--accent); border-color:var(--accent);'>" + txt_Back + "</a></div></body></html>";
  server.send(200, "text/html", page);
}

void handleCSV() {
  if(!loggedIn) { server.sendHeader("Location","/"); server.send(303); return; }
  String csv = "Grade,Name,Section,Date,Time\n";
  for (int g = 0; g < GRADE_COUNT; g++) {
    for (int i = 0; i < recordCount[g]; i++) {
      csv += gradeNames[g] + "," + records[g][i].name + "," + records[g][i].section + "," + records[g][i].date + "," + records[g][i].time + "\n";
    }
  }
  String filename = "Attendance_" + getDate() + ".csv";
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
  server.send(200, "text/csv", csv);
}

void handleCheckQR() {
  if (adminQR_Detected) {
    loggedIn = true;
    adminQR_Detected = false;
    server.send(200, "text/plain", "YES");
  } else {
    server.send(200, "text/plain", "NO");
  }
}

void handleRoot() {
  String head = getCommonHead();
  if(!loggedIn) {
    String page = "<!DOCTYPE html><html><head>" + head + "<title>Login</title></head><body><div id='loginBox' class='content-box'><h2>" + txt_Login + "</h2>";
    page += "<form action='/login' method='POST' onsubmit='return validateWithPin()'>";
    page += "<input type='text' name='user' placeholder='" + txt_Username + "' oninput='triggerCrack()' required>";
    page += "<div style='position:relative'><input type='password' id='pass' name='pass' placeholder='" + txt_Pass + "' oninput='triggerCrack()' required>";
    page += "<span style='position:absolute;right:15px;top:10px;color:#333;cursor:pointer;font-size:20px;' onclick='togglePass()'>👁</span></div>";
    page += "<button class='btn-action'>" + txt_BtnSignIn + "</button></form>";
    page += "<script>";
    page += "function triggerCrack(){ const b=document.getElementById('loginBox'); b.classList.remove('crack-anim'); void b.offsetWidth; b.classList.add('crack-anim'); }";
    page += "function togglePass(){ let p = prompt('Enter PIN:'); if(p=='0827'){ let x=document.getElementById('pass'); x.type=(x.type==='password')?'text':'password'; } else { alert('Wrong PIN'); } }";
    page += "function validateWithPin(){ let p = prompt('Enter Admin PIN:'); if(p == '0827') return true; else { alert('Wrong Admin PIN'); return false; } }";
    page += "setInterval(function(){ fetch('/checkQR').then(r=>r.text()).then(s=>{if(s==='YES') window.location.reload();}); }, 1000);</script></div></body></html>";
    server.send(200, "text/html", page);
  } else {
    String page = "<!DOCTYPE html><html><head>" + head + "<title>Dashboard</title></head><body><div class='content-box'><h2>" + txt_Dashboard + "</h2>";
    page += (systemEnabled) ? "<button onclick='toggleSys(\"off\")' class='power-btn off'>SYSTEM ON (OFF)</button>" : "<button onclick='toggleSys(\"on\")' class='power-btn on'>SYSTEM OFF (ON)</button>";
    page += "<div class='main-carousel' data-flickity='{ \"cellAlign\": \"center\", \"contain\": true, \"prevNextButtons\": false }'>";
    for(int i=0; i<GRADE_COUNT; i++) page += "<a href='/grade?g="+String(i)+"' style='text-decoration:none; color:white;'><div style='width:140px; height:110px; background:rgba(255,255,255,0.15); border-radius:15px; display:flex; justify-content:center; align-items:center; margin-right:15px;'>"+gradeNames[i]+"</div></a>";
    page += "</div><div class='dash-controls'>";
    page += "<a href='/stats' class='btn-outline' style='color:#3498db; border-color:#3498db;'>" + txt_Stats + "</a>";
    page += "<a href='/download' class='btn-outline' style='color:var(--accent); border-color:var(--accent);'>" + txt_SaveCSV + "</a>";
    page += "<button onclick='resetAll()' class='btn-outline' style='color:var(--danger); border-color:var(--danger);'>" + txt_Clear + "</button>";
    page += "<a href='/logout' class='btn-outline' style='color:white; border-color:white;'>" + txt_Logout + "</a>";
    page += "</div><script>function toggleSys(s){ if(confirm(\"Change System?\")) window.location.href='/power?state='+s; } function resetAll(){ if(confirm(\"Bura lahat?\")) window.location.href='/resetAll'; }</script></div></body></html>";
    server.send(200, "text/html", page);
  }
}

void handleLogin() {
  if(server.hasArg("user") && server.hasArg("pass")) {
    String u = server.arg("user");
    String p = server.arg("pass");
    if ((u == "yule" && p == "admin123") || (u == "MaranathaAdmin" && p == "adminLogin2")) {
      loggedIn = true;
    }
  }
  server.sendHeader("Location","/");
  server.send(303);
}

void handleLogout() { loggedIn = false; server.sendHeader("Location","/"); server.send(303); }
void handleResetAll() { if(loggedIn) { for(int i=0; i<GRADE_COUNT; i++) recordCount[i] = 0; saveToFile(); } server.sendHeader("Location","/"); server.send(303); }
void handlePower() { if(loggedIn) { systemEnabled = (server.arg("state") == "on"); if(systemEnabled) lcd.backlight(); else lcd.noBacklight(); } server.sendHeader("Location","/"); server.send(303); }


// ================= CORE SETUP =================

void setup() {
  Serial.begin(115200);
  QRSerial.begin(9600, SERIAL_8N1, 16, 17);
  QRSerial.setTimeout(10);

  esp_task_wdt_config_t twdt_config = { .timeout_ms = WDT_TIMEOUT * 1000, .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, .trigger_panic = true };
  esp_task_wdt_deinit();             // FIX: i-deinit muna para hindi mag-error
  esp_task_wdt_init(&twdt_config);
  esp_task_wdt_add(NULL);

  lcd.init();
  lcd.backlight();                   // FIX: naka-ON na agad sa boot
  
  if(!LittleFS.begin(true)) while(1);
  loadFromFile();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);
  WiFi.begin(router_ssid, router_pass);
  
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 15) {
    delay(500);
    retry++;
  }

  WiFi.setSleep(WIFI_PS_NONE);
  configTime(8 * 3600, 0, "pool.ntp.org");

  server.on("/", handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/logout", handleLogout);
  server.on("/grade", handleGrade);
  server.on("/download", handleCSV);
  server.on("/resetAll", handleResetAll);
  server.on("/power", handlePower);
  server.on("/checkQR", handleCheckQR);
  server.on("/stats", handleStats);
  server.begin();
  
  lcd.clear();
  lcd.print("SYSTEM READY");         // FIX: consistent sa systemEnabled = true
}

// ================= CORE LOOP =================

void loop() {
  esp_task_wdt_reset();
  server.handleClient();

  // --- SECRET TOUCH BYPASS ---
  int touchVal = touchRead(4);
  if (touchVal < TOUCH_THRESHOLD && touchVal > 1) {
    unsigned long tStart = millis();
    while(touchRead(4) < TOUCH_THRESHOLD) {
      esp_task_wdt_reset();
      if(millis() - tStart > 2000) {
        adminQR_Detected = true;
        lcd.clear(); lcd.print("BIO-AUTH: OK");
        delay(2000);
        break;
      }
      delay(50);
    }
  }

  // --- AUTO RESET DAILY ---
  String today = getDate();
  if (today != "0000-00-00") {
      if (lastDateInSystem == "") lastDateInSystem = today;
      else if (today != lastDateInSystem) {
          for(int i=0; i<GRADE_COUNT; i++) recordCount[i] = 0;
          lastDateInSystem = today;
          saveToFile();
      }
  }

  // --- QR SCANNER LOGIC ---
  if(QRSerial.available()){
    if(millis() - lastScanMillis < 3000) {
      while(QRSerial.available()) QRSerial.read();
      return;
    }

    String data = QRSerial.readStringUntil('\n');
    data.trim();
    if(data.length() < 5) return;

    if(data.startsWith("Admin UK")) {
      lastScanMillis = millis();
      
      if(data == "Admin UK 007" || data == "Admin UK 000") {
          systemEnabled = !systemEnabled;
          lcd.clear();
          if(systemEnabled) { lcd.backlight(); lcd.print("SYSTEM: ON"); }
          else { lcd.noBacklight(); lcd.print("SYSTEM: OFF"); }
      }
      else if(data == "Admin UK 001") {
          adminQR_Detected = true;
          lcd.clear(); lcd.print("Welcome Mr.Yule");
      }
      else if(data == "Admin UK 002") {
          lcd.clear(); lcd.print("IP ADDRESS:");
          lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString() == "0.0.0.0" ? WiFi.softAPIP() : WiFi.localIP());
      }
      else if(data == "Admin UK 003") {
          lcd.clear(); lcd.print("DATE (PH):");
          lcd.setCursor(0,1); lcd.print(getDate());
      }
      else if(data == "Admin UK 005") {
          lcd.clear(); lcd.print("TIME (PH):");
          lcd.setCursor(0,1); lcd.print(getTimeOnly());
      }
      else if(data == "Admin UK 006") {
          lcd.clear(); lcd.print("THESIS DEFENDED");
          lcd.setCursor(0,1); lcd.print("!!!!!");
      }
      else if(data == "Admin UK 004") {
          lcd.clear(); lcd.print("DIAGNOSTIC...");
          delay(1000);
          lcd.clear(); lcd.print(WiFi.status()==WL_CONNECTED ? "WIFI:OK " : "WIFI:FAIL ");
          lcd.setCursor(0,1); lcd.print("HEAP:"); lcd.print(ESP.getFreeHeap()/1024); lcd.print("KB");
      }

      delay(3000);
      if(systemEnabled) { lcd.clear(); lcd.print("SYSTEM READY"); }
      else { lcd.clear(); lcd.print("SYSTEM OFF"); }
      return;
    }

    // --- STUDENT SCAN ---
    if(systemEnabled) {
      int lastS = data.lastIndexOf(" ");
      int secS = data.substring(0, lastS).lastIndexOf(" ");
      if(lastS < 0 || secS < 0) return;

      String gradeStr = data.substring(lastS + 1);
      String section  = data.substring(secS + 1, lastS);
      String name     = data.substring(0, secS);
      
      int gIdx = -1;
      for(int i=0; i<GRADE_COUNT; i++) { if(gradeStr.equalsIgnoreCase(gradeNames[i])) gIdx = i; }

      if(gIdx != -1 && recordCount[gIdx] < MAX_RECORDS){
        bool dup = false;
        for(int i=0; i<recordCount[gIdx]; i++) {
          if(records[gIdx][i].name.equalsIgnoreCase(name) && records[gIdx][i].date == today) dup = true;
        }
        
        lastScanMillis = millis();
        lcd.clear();
        if(dup) { lcd.print("ALREADY SCANNED"); } 
        else {
          records[gIdx][recordCount[gIdx]] = {name, section, today, getTimeOnly()};
          recordCount[gIdx]++;
          saveToFile();
          lcd.print("SCAN SUCCESSFUL");
          lcd.setCursor(0,1); lcd.print(name.substring(0,16));
        }
        delay(2000);
        lcd.clear(); lcd.print("SYSTEM READY");
        while(QRSerial.available()) QRSerial.read();
      }
    }
  }