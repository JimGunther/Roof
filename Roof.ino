/*************************************************************************************
* Roof.ino: MiS Roof ESP32 Code (Weather Station) 2nd PRODUCTION VERSION w/o Meteo   *
*                                                                                    *
* Version: 2.5                                                                       *
* Last updated: 07/08/2025 21:12                                                     *
* Author: Jim Gunther                                                                *
*                                                                                    *
*                                                                                    *
**************************************************************************************/
// NB USE ESP32 Dev Module as BOARD!!
//#include <WiFi.h>
#include <WiFiClient.h>  //OTA
#include <WebServer.h>   //OTA
#include <ESPmDNS.h>     //OTA
#include <Update.h>      //OTA
//#include <PubSubClient.h>

/* CHECK VERSIONS SOON!
library WiFi at version 3.2.0 
library Networking at version 3.2.0 
library WebServer at version 3.2.0 
library FS at version 3.2.0 
library ESPmDNS at version 3.2.0 
library Update at version 3.2.0 
library PubSubClient at version 2.8 
library Wire at version 3.2.0 
library Adafruit BMP085 Unified at version 1.1.3 
library Adafruit Unified Sensor at version 1.1.15 
library Adafruit AHTX0 at version 2.0.5
library Adafruit BusIO at version 1.17.1 
library SPI at version 3.2.0 
library BH1750 at version 1.3.0
 */

// Other includes
#include "Config.h"
#include "Komms.h"
#include "Tom.h"

// Class instantiation
WebServer server(80);  // OTA
Komms kom;
Tom cabinBoy;

//===================================================================================================================
// MQTT stuff

WiFiClient espClient;
PubSubClient qtClient(espClient);
int nwkIx;
bool bNewMQTT = false;
unsigned int icLength;
byte bqticBuf[QT_LEN];
char qticBuf[QT_LEN];
char qtogBuf[QT_LEN];
char mqttServer[IP_LEN];
char versionBuf[52];

// Array and variables initiation
unsigned long loopStart;
int loopCount = 0;
int rptIntvl = 120;  // default value
int wdInterval = 6; // default value
int maxGust;
unsigned long rebootTime;

/******************************************************************************************************/

void qtCallback(char* topic, byte* message, unsigned int length) {
  for (int i = 0; i < length; i++) {
    qticBuf[i] = (char)message[i];
  }
  bNewMQTT = true;
}

/*********************************************************************************************************************
qtSetup(): sets up MQTT connection
parameters: nwkIx: int: index no. of wifi network (0-2) Shed, Jim, Richard
returns: boolean True for success, False for failure
**********************************************************************************************************************/
bool qtSetup(int nwkIx) {
  switch (nwkIx) {
    case 0:
      strcpy(mqttServer, SHED_IP);
      break;
    case 1:
      strcpy(mqttServer, JIM_IP);
      break;
    case 2:
      strcpy(mqttServer, RICH_IP);
      break;
  }

  qtClient.setServer(mqttServer, 1883);
  qtClient.setCallback(qtCallback);
  return qtReconnect();
}

/*********************************************************************************************************************
qtReconnect(): does the actual connection to MQTT broker (3 attempts)
parameters: none
returns: boolean: True for success, False for failure
**********************************************************************************************************************/
bool qtReconnect() {

  // Loop until we're reconnected
  int numTries = 0;

  while (!qtClient.connected() && (numTries++ < 3)) {
    fn_RedLed(ON);
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (qtClient.connect("misRoof")) {
      Serial.println("connected");
      qtClient.loop();
      fn_RedLed(OFF);
    } else {
      Serial.print("failed, rc=");
      Serial.print(qtClient.state());
      Serial.println(" try again in 3 seconds");
      // Wait 3 seconds before retrying
      delay(3000);
    }
  }

  return qtClient.connected();
}
/********************************************************************************************************************
publishMQTT(): post CSV data, including initial character and CSV-style requests to Shed
parameters:
  topic: int: one of TOPIC_INIT, TOPIC_VALS, TOPIC_WD
  txt: string containing text to publish: NB MUST start with comma and no final comma
returns: void
*********************************************************************************************************************/
void publishMQTT(const char* topic, const char* txt) {
  qtClient.publish(topic, txt, false);
}

/********************************************************************************************************************
postMessage(): post message verbatim, just adding '$' to the front
parameters:
  txt: string containing message
returns: void
*********************************************************************************************************************/
void postMessage(const char* txt) {
  char buf[MESSBUF_LEN];
  buf[0] = '$';
  strcpy(buf + 1, txt);
  qtClient.publish(TOPIC_MESS, buf, false);
}

const char* host = "esp32";  // OTA

// BEGIN OTA BLOCK 1============================================================================================================

/*
 * Login page
 */
const char* loginIndex =
  "<form name='loginForm'>"
  "<table width='20%' bgcolor='A09F9F' align='center'>"
  "<tr>"
  "<td colspan=2>"
  "<center><font size=4><b>ESP32 Login Page</b></font></center>"
  "<br>"
  "</td>"
  "<br>"
  "<br>"
  "</tr>"
  "<td>Username:</td>"
  "<td><input type='text' size=25 name='userid'><br></td>"
  "</tr>"
  "<br>"
  "<br>"
  "<tr>"
  "<td>Password:</td>"
  "<td><input type='Password' size=25 name='pwd'><br></td>"
  "<br>"
  "<br>"
  "</tr>"
  "<tr>"
  "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
  "</tr>"
  "</table>"
  "</form>"
  "<script>"
  "function check(form)"
  "{"
  "if(form.userid.value=='mis' && form.pwd.value=='mispwd')"
  "{"
  "window.open('/serverIndex')"
  "}"
  "else"
  "{"
  " alert('Error Password or Username')/*displays error message*/"
  "}"
  "}"
  "</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
  "<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
  "<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
  "<input type='file' name='update'>"
  "<input type='submit' value='Update'>"
  "</form>"
  "<div id='prg'>progress: 0%</div>"
  "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
  "},"
  "error: function (a, b, c) {"
  "}"
  "});"
  "});"
  "</script>";

// END OTA BLOCK 1 =============================================================================================================

void setup() {
//Setup code here, to run once:

  Serial.begin(115200);
  pinMode(LEDPin, OUTPUT);
  //pinMode(RedPin, OUTPUT);

  //fn_RedLed(ON);
  kom.begin();
  cabinBoy.startup();

  tryMQTTStart();

  // OTA START BLOCK 2 =========================================================================================
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) {  //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on(
    "/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    },
    []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        /* flashing firmware to ESP*/
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {  //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });
  server.begin();

  // OTA END BLOCK 2 ============================================================================================
  int hrTime = 0; // default to 00:00 if no RPi

  qtClient.subscribe(TOPIC_SETUP, 1);
  loopCount = 1;  // changed from 0 to suppress 0,0,... first entry
  hrTime = requestRptInterval();
  rebootTime = millis() + 1000 * 3600 * (24 - hrTime); // milliseconds

  qtClient.unsubscribe(TOPIC_SETUP);
  logStartupSuccess();
  //fn_RedLed(OFF);
}

void loop() {
  // put your main code here, to run repeatedly:
  server.handleClient();  // OTA
  loopStart = millis();
  qtClient.loop();  // keep MQTT going
  
  // Loop timing zones start here ----------------------------------------------------------------

  // ZONE 1: EVERY LOOP (1/4 sec) -----------------------------------------------------------------
  cabinBoy.catchMaxGust();                          // 4 times/sec to catch gusts
  bool bOK = cabinBoy.updateMeteo(loopCount, rptIntvl);  // updateMeteo uses loopCount to decide when to update each Meteo
  if (!bOK) {
    Serial.println("Meteo update error!");
  }
  // END ZONE 1 -----------------------------------------------------------------------------------
  //WD ZONE n: EVERY n LOOPS  ---------------------------------------------------------------------
  if ((loopCount % wdInterval) == 0) {
    cabinBoy.catchWD();  // records the wind direction (currently evey 1.5 seconds)
  }
  // END ZONE n

  // ZONE 12: EVERY 12 LOOPS (3 secs) -------------------------------------------------------------
  if ((loopCount % ZONE12) == 0) {
    if (!qtReconnect()) {  // checks connection and attempts retry if none
      postMessage("MQTT failed");
    }
    blink();  // "normal" 3 second blink
  }
// END ZONE 12 ----------------------------------------------------------------------------------
  // ZONE SLOWEST: EVERY rptInterval LOOPS (DEFAULT 30 secs)
  if (loopCount == 0) {
    if (millis() > rebootTime) {
      esp_restart();
    }
    publishToShed();
    kom.checkWifi();
  }
  //Loop timing zones end here--------------------------------------------------------------------

  loopTimer();
  loopCount = (loopCount + 1) % rptIntvl;
  // END OF LOOP
  size_t heapSz = xPortGetFreeHeapSize();
  
}

// START GLOBAL FUNCTIONS =====================================================================================
/*
a. Operate Red LED
*/
void fn_RedLed(int state) {
  digitalWrite(RedPin, state);
}

/*****************************************************************************************************
b. tryMQTTStart(): attempts to start MQTT connection: reboots ESP after 10 failed attempts
parameters: none
returns: void
*****************************************************************************************************/
void tryMQTTStart() {
  int qtCount = 0;
  while (!qtSetup(kom.getNwkIx())) {
    fn_RedLed(ON);
    Serial.println("MQTT setup failed. No MQTT comms. Check RPi is powered and running.");
    digitalWrite(LEDPin, !digitalRead(LEDPin));
    delay(800);
    if (qtCount++ == 11) esp_restart();
  }
  fn_RedLed(OFF);
}

/*****************************************************************************************************
c. requestRptInterval(): sends message to Shed to prompt repeat Interval value and wait to get current hour by return
parameters: none
returns: int: current hour
*****************************************************************************************************/
int requestRptInterval() {
  // format and post initial request string
  int pos, hr;
  char buf[ICBUF_LEN];
  char* p;
  strcpy(buf, "Hello!");
  publishMQTT(TOPIC_INIT, buf);
  char ch;
  bool bContinue = true;
  int loops = 0;
  while (bContinue && (loops < INIT_WAIT)) {
    qtClient.loop();
    if (bNewMQTT) {  // 'I' == initial Shed information message
      ch = qticBuf[0];
      p = strchr(qticBuf, ';');
      pos = p - qticBuf;
      *p = '\0';
      rptIntvl = atoi(qticBuf + 1);
      wdInterval = rptIntvl / WD_DIVISOR;
      hr = atoi(qticBuf + pos + 1);
      Serial.print("Rpt Intvl: ");
      Serial.println(rptIntvl);
      Serial.println(hr);
      bNewMQTT = false;
      bContinue = false;
    }
    loops++;
    delay(100);  // Review this interval once Python code is written!
  }
  return hr;
}

/*****************************************************************************************************
d. logStartupSuccess(): sends message reporting startup is complete and successful
parameters: none
returns: void
*****************************************************************************************************/
void logStartupSuccess() {
  // Log a startup success message
  strcpy(versionBuf, "RoofN ver ");
  strcpy(versionBuf + 10, __DATE__);
  versionBuf[21] = ' ';
  strcpy(versionBuf + 22, __TIME__);
  strcpy(versionBuf + 30, ": Roof setup done.");
  postMessage(versionBuf);
  versionBuf[30] = '\0';  // truncate for all later uses
}

// LOOP SUPPORT FUNCTIONS ==============================================================================
/******************************************************************************************************
1. loopTimer(): aims to keep loop time to 1/4 second intervals: logs message if code runs longer
parameters: none
returns: void
******************************************************************************************************/
void loopTimer() {
  // End-of-loop timing code
  unsigned long loopEnd = millis();
  if ((loopEnd - loopStart) > LOOP_TIME) {  // Code took > LOOP_TIME
    char mBuf[MESSBUF_LEN];
    sprintf(mBuf, "Long loop time %u ms", loopEnd - loopStart);
    postMessage(mBuf);  // log all long loops (> LOOP_TIME)
  } else {              // wait until LOOP_TIME has elapsed
    while (millis() - loopStart < LOOP_TIME) {
      delay(1);
    }
  }
}

/************************************************************************************************************
2. blink(): controls 3-seocond LED blinking (daylight only)
parameters: none
returns: void
************************************************************************************************************/
void blink() {
  int val = cabinBoy.getLight4Blink();
  if (val > MIN_DAYLIGHT) {             // daytime
    digitalWrite(LEDPin, !digitalRead(LEDPin));  // blink
  } else {                                       // nighttime
    digitalWrite(LEDPin, LOW);
  }
}

/*************************************************************************************************************
3. publishToShed(): gets the 2 csv strings and publishes on MQTT
parameters: none
returns: void
**************************************************************************************************************/
void publishToShed() {
  char valsCSV[VALSBUF_LEN];
  char wdCSV[WDBUF_LEN];
  int numWDs = cabinBoy.makeWDCSV(wdCSV);
  int msgLength = cabinBoy.makeValsCSV(valsCSV);
  // msgLength == 0 if all entries are zero
  if (msgLength > 0){
    publishMQTT("nws/wd", wdCSV);
    publishMQTT("nws/vals", valsCSV);
  }
  else {
    Serial.println("All Zeros");
  }
  cabinBoy.resetAll();
}
