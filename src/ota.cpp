#include "ota.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "config.h"

static WebServer* s_srv = nullptr;
static OtaStatus  s_st;
static bool       s_first = true;

static const char OTA_PASS[] = "auxnjoy1";

static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AUX-N Touch Joystick - Update</title>
<style>
body{font-family:sans-serif;background:#101216;color:#eee;margin:0;padding:20px}
.card{max-width:460px;margin:auto;background:#24282f;border-radius:12px;padding:20px}
h1{font-size:20px;margin:0 0 4px;color:#ff9600}small{color:#9aa}
input[type=file]{margin:16px 0;width:100%}
button{width:100%;padding:14px;font-size:17px;border:0;border-radius:8px;background:#2f7a45;color:#fff}
button:disabled{background:#555}
.bar{height:22px;background:#333;border-radius:6px;overflow:hidden;margin-top:16px}
.fill{height:100%;width:0;background:#4c4}
#msg{margin-top:12px;min-height:1.4em}
</style></head><body><div class="card">
<h1>AUX-N Touch Joystick</h1><small>Firmware: v%VER% &nbsp; %BOARD%</small>
<p>Select the <b>_APP_OTA.bin</b> file / Válaszd ki az <b>_APP_OTA.bin</b> fájlt:</p>
<input type="file" id="f" accept=".bin">
<button id="b" onclick="up()">Upload / Feltöltés</button>
<div class="bar"><div class="fill" id="p"></div></div>
<div id="msg"></div>
</div><script>
function up(){
 var f=document.getElementById('f').files[0],m=document.getElementById('msg');
 if(!f){m.textContent='No file / Nincs fájl';return;}
 if(f.name.indexOf('FULL')>=0){m.textContent='Use the _APP_OTA.bin, not the FULL file! / Az _APP_OTA.bin kell, nem a FULL!';return;}
 var x=new XMLHttpRequest(),d=new FormData();d.append('fw',f);
 document.getElementById('b').disabled=true;
 x.upload.onprogress=function(e){if(e.lengthComputable)document.getElementById('p').style.width=(e.loaded*100/e.total)+'%';};
 x.onload=function(){m.textContent=x.status==200?'OK - restarting / újraindul...':('Error / Hiba: '+x.responseText);
  if(x.status!=200)document.getElementById('b').disabled=false;};
 x.onerror=function(){m.textContent='Connection error / Kapcsolat hiba';document.getElementById('b').disabled=false;};
 x.open('POST','/update?size='+f.size);x.send(d);
}
</script></body></html>)HTML";

static void fail(const char* why) {
  s_st.state = OtaState::Error;
  snprintf(s_st.msg, sizeof(s_st.msg), "%s", why);
  Serial.printf("[OTA] HIBA: %s\n", why);
}

static void handleRoot() {
  String page = FPSTR(PAGE);
  page.replace("%VER%", FW_VERSION);
  page.replace("%BOARD%", BOARD_NAME);
  s_srv->send(200, "text/html", page);
  s_st.lastActivityMs = millis();
}

static void handleUpload() {
  HTTPUpload& up = s_srv->upload();
  s_st.lastActivityMs = millis();
  if (up.status == UPLOAD_FILE_START) {
    s_st.received = 0;
    s_st.total = (uint32_t)s_srv->arg("size").toInt();
    s_first = true;
    s_st.state = OtaState::Receiving;
    snprintf(s_st.msg, sizeof(s_st.msg), "%s", up.filename.c_str());
    Serial.printf("[OTA] feltöltés: %s (%lu bájt)\n", up.filename.c_str(), (unsigned long)s_st.total);
    if (!Update.begin(s_st.total > 0 ? s_st.total : UPDATE_SIZE_UNKNOWN)) fail(Update.errorString());
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (s_st.state != OtaState::Receiving) return;
    if (s_first) {
      s_first = false;
      if (up.currentSize == 0 || up.buf[0] != 0xE9) {   // ESP alkalmazás kép fejléce
        Update.abort();
        fail("Not an app image (use _APP_OTA.bin)");
        return;
      }
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      Update.abort();
      fail(Update.errorString());
      return;
    }
    s_st.received += up.currentSize;
  } else if (up.status == UPLOAD_FILE_END) {
    if (s_st.state != OtaState::Receiving) return;
    if (Update.end(true)) {
      s_st.state = OtaState::Done;
      Serial.printf("[OTA] kész, %lu bájt, újraindítás\n", (unsigned long)s_st.received);
    } else {
      fail(Update.errorString());
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    fail("Upload aborted");
  }
}

static void handleUpdateDone() {
  if (s_st.state == OtaState::Done) s_srv->send(200, "text/plain", "OK");
  else s_srv->send(500, "text/plain", s_st.msg);
}

void otaStart() {
  if (s_srv) return;
  s_st = OtaStatus();
  WiFi.mode(WIFI_AP);
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  snprintf(s_st.ssid, sizeof(s_st.ssid), "AUXN-Joy-%02X%02X", mac[4], mac[5]);
  snprintf(s_st.pass, sizeof(s_st.pass), "%s", OTA_PASS);
  WiFi.softAP(s_st.ssid, s_st.pass);
  s_srv = new WebServer(80);
  s_srv->on("/", HTTP_GET, handleRoot);
  s_srv->on("/update", HTTP_POST, handleUpdateDone, handleUpload);
  s_srv->onNotFound(handleRoot);
  s_srv->begin();
  s_st.state = OtaState::Ready;
  s_st.lastActivityMs = millis();
  Serial.printf("[OTA] WiFi BE: %s / %s, http://%s\n", s_st.ssid, s_st.pass, WiFi.softAPIP().toString().c_str());
}

void otaStop() {
  if (!s_srv) return;
  if (s_st.state == OtaState::Receiving) Update.abort();
  s_srv->stop();
  delete s_srv;
  s_srv = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  s_st.state = OtaState::Off;
  Serial.println("[OTA] WiFi KI");
}

void otaLoop() {
  if (!s_srv) return;
  s_srv->handleClient();
  uint8_t n = WiFi.softAPgetStationNum();
  if (n != s_st.clients) {
    s_st.clients = n;
    s_st.lastActivityMs = millis();
  }
  if (n > 0) s_st.lastActivityMs = millis();   // amíg van csatlakozott eszköz, nem jár le
}

const OtaStatus& otaStatus() { return s_st; }
