#include <AsyncEventSource.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include <AsyncWebSynchronization.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <StringArray.h>
#include <WebAuthentication.h>
#include <WebHandlerImpl.h>
#include <WebResponseImpl.h>


#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <Preferences.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <FS.h>
#include <SPIFFS.h>
#include <time.h>

#include "defines.h"
#include "Credentials.h"
#include "dynamicParams.h"

#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

//#include <BluetoothSerial.h>
//#include <ESPAsync_WiFiManager_Lite.h>

/*Bluetooth*/
//BluetoothSerial SerialBT;

/* GPS
   This sample code demonstrates the normal use of a TinyGPSPlus (TinyGPSPlus) object.
   It requires the use of SoftwareSerial, and assumes that you have a
   4800-baud serial GPS device hooked up on pins 4(rx) and 3(tx).
*/
static const int RXPin = 16, TXPin = 17; //RXPin = 22, TXPin = 21;
static const uint32_t GPSBaud = 4800;
// The TinyGPSPlus object
TinyGPSPlus gps;
// The serial connection to the GPS device
SoftwareSerial ss(RXPin, TXPin, true); //
int start=0;
String nmea ="OSM Logger---------------------------------------------------------------------------------------------------------------------------------------";


/*WifiManager*/
#define USE_DYNAMIC_PARAMETERS      false

//bool LOAD_DEFAULT_CONFIG_DATA = true;
//ESP_WM_LITE_Configuration defaultConfig;
ESPAsync_WiFiManager_Lite* ESPAsync_WiFiManager; 
WiFiClient CUBEclient;

/*NTP*/
const char* ntpServer ="de.pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0; //3600;
struct tm   tmLast;
unsigned long myRuntime=0;
unsigned long cMillis = 0;

/*OTA & WEB*/
AsyncWebServer server(80);
AsyncWebServer CUBEserver(80);
const char* AsyncUser = "admin";
const char* AsyncPWD = "!parsival";
AsyncWebSocket ws("/ws");

// env variable for Rest API
bool PowerBankState=0;

/*Rest API*/
StaticJsonDocument<250> jsonDocument;
char DisplayTime[10];
char buffer[250];
void create_json(char *tag, float value, char *unit) {  
  jsonDocument.clear();
  jsonDocument["type"] = tag;
  jsonDocument["value"] = value;
  jsonDocument["unit"] = unit;
  serializeJson(jsonDocument, buffer);  
}
 
void add_json_object(char *tag, float value, char *unit) {
  JsonObject obj = jsonDocument.createNestedObject();
  obj["type"] = tag;
  obj["value"] = value;
  obj["unit"] = unit; 
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "application/json", "{\"message\":\"Not found\"}");
}

void getPowerbank() {
  create_json("powerbank",PowerBankState,"Toggle");
  //server.send(200, "application/json", buffer);
  server.on("/powerbank", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buffer);  
  });
  CUBEserver.on("/powerbank", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buffer);  
  });
}

void setupgps() {
  ss.begin(GPSBaud,SWSERIAL_8N1); //, SWSERIAL_8N1, RXPin, TXPin);

  Serial.println(F("An extensive example of many interesting TinyGPSPlus features"));
  Serial.print(F("Testing TinyGPSPlus library v. ")); Serial.println(TinyGPSPlus::libraryVersion());
  //Serial.println(F("by Mikal Hart"));
  Serial.println();
  Serial.println(F("Sats HDOP  Latitude   Longitude   Fix  Date       Time     Date Alt    Course Speed Card  Distance Course Card  Chars Sentences Checksum"));
  Serial.println(F("           (deg)      (deg)       Age                      Age  (m)    --- from GPS ----  ---- to London  ----  RX    RX        Fail"));
  Serial.println(F("----------------------------------------------------------------------------------------------------------------------------------------"));
}

void parsegps() {
  static const double LONDON_LAT = 51.01847, LONDON_LON = 6.04530;
  String gnss_data = "";

  double longitude,latitude;
  while (ss.available() >0 ) {
    if ( gps.encode(ss.read()) ) {
      displayInfo();
      Serial.println("G");
    }
  }
/*
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS detected: check wiring."));
    while(true);
  }

*/
/*
  if (Serial.available()){
    BT.write(Serial.read());
  }

  if(BT.available()){
    Serial.write(BT.read());
  }
*/
  if(gps.charsProcessed() >= 10) {
    Serial.println();
    printInt(gps.satellites.value(), gps.satellites.isValid(), 5);
    printFloat(gps.hdop.hdop(), gps.hdop.isValid(), 6, 1);
    printFloat(gps.location.lat(), gps.location.isValid(), 11, 6);
    printFloat(gps.location.lng(), gps.location.isValid(), 12, 6);
    printInt(gps.location.age(), gps.location.isValid(), 5);
    printDateTime(gps.date, gps.time);
    printFloat(gps.altitude.meters(), gps.altitude.isValid(), 7, 2);
    printFloat(gps.course.deg(), gps.course.isValid(), 7, 2);
    printFloat(gps.speed.kmph(), gps.speed.isValid(), 6, 2);
    printStr(gps.course.isValid() ? TinyGPSPlus::cardinal(gps.course.deg()) : "*** ", 6);
  
    unsigned long distanceKmToLondon =
      (unsigned long)TinyGPSPlus::distanceBetween(
        gps.location.lat(),
        gps.location.lng(),
        LONDON_LAT, 
        LONDON_LON) / 1000;
    printInt(distanceKmToLondon, gps.location.isValid(), 9);

    double courseToLondon =
      TinyGPSPlus::courseTo(
        gps.location.lat(),
        gps.location.lng(),
        LONDON_LAT, 
        LONDON_LON);

    printFloat(courseToLondon, gps.location.isValid(), 7, 2);

    const char *cardinalToLondon = TinyGPSPlus::cardinal(courseToLondon);

    printStr(gps.location.isValid() ? cardinalToLondon : "*** ", 6);

    printInt(gps.charsProcessed(), true, 6);
    printInt(gps.sentencesWithFix(), true, 10);
    printInt(gps.failedChecksum(), true, 9);
    Serial.println();
  
    smartDelay(1000);

    if (millis() > 5000 && gps.charsProcessed() < 10)
      Serial.println(F("No GPS data received: check wiring"));
  }
/*
  else
  {
    if strlen(gnss_data>0){
      Serial.printf("%s\n", gnss_data);
    }
  }
*/
}

// This custom version of delay() ensures that the gps object
// is being "fed".
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do 
  {
    while (ss.available())
      gps.encode(ss.read());
  } while (millis() - start < ms);
}

static void printFloat(float val, bool valid, int len, int prec)
{
  bool valiide=true;
  if (!valid)
  {
    while (len-- > 1)
      Serial.print('*');
    Serial.print(' ');
  }
  else
  {
    Serial.print(val, prec);
    int vi = abs((int)val);
    int flen = prec + (val < 0.0 ? 2 : 1); // . and -
    flen += vi >= 1000 ? 4 : vi >= 100 ? 3 : vi >= 10 ? 2 : 1;
    for (int i=flen; i<len; ++i)
      Serial.print(' ');
  }
  smartDelay(0);
}

static void printInt(unsigned long val, bool valid, int len)
{
  char sz[32] = "*****************";
  if (valid)
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i=strlen(sz); i<len; ++i)
    sz[i] = ' ';
  if (len > 0) 
    sz[len-1] = ' ';
  Serial.print(sz);
  smartDelay(0);
}

static void printDateTime(TinyGPSDate &d, TinyGPSTime &t)
{
  if (!d.isValid())
  {
    Serial.print(F("********** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d ", d.month(), d.day(), d.year());
    Serial.print(sz);
  }
  
  if (!t.isValid())
  {
    Serial.print(F("******** "));
  }
  else
  {
    char sz[32];
    sprintf(sz, "%02d:%02d:%02d ", t.hour(), t.minute(), t.second());
    Serial.print(sz);
  }

  printInt(d.age(), d.isValid(), 5);
  smartDelay(0);
}

static void printStr(const char *str, int len)
{
  int slen = strlen(str);
  for (int i=0; i<len; ++i)
    Serial.print(i<slen ? str[i] : ' ');
  smartDelay(0);
}

void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
  if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      //handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void setup_routing() {
  //server.on("/Powerbank", getPowerbank);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"powerbank\":\"Welcome\"}");
  });
  CUBEserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"powerbank\":\"Welcome\"}");
  });

  server.on("/get-message", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<100> data;
    if (request->hasParam("powerbank")) {
      data["powerbank"] = request->getParam("powerbank")->value();
    } else {
      data["powerbank"] = "No powerbank parameter";
    }
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  });

  CUBEserver.on("/get-message", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<100> data;
    if (request->hasParam("powerbank")) {
      data["powerbank"] = request->getParam("powerbank")->value();
    } else {
      data["powerbank"] = "No powerbank parameter";
    }
    String response;
    serializeJson(data, response);
    request->send(200, "application/json", response);
  });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/powerbank", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();
    StaticJsonDocument<200> data;
    if (json.is<JsonArray>())
    {
      Serial.println("is<JsonArray");
      data = json.as<JsonArray>();
    }
    else if (json.is<JsonObject>())
    {
      Serial.println("is<JsonObject");
      data = json.as<JsonObject>();
      //for (JsonPair keyValue : jsonObj){
      //  Serial.printf("%s=%s\n",keyValue.key().c_str(),keyValue.value().as<String>());
      //}
      if(jsonObj.containsKey("powerbank")){
        int powerbankNewValue = jsonObj["powerbank"];
        setPowerbank(powerbankNewValue);
      }
//      CUBE
      if(jsonObj.containsKey("WLAN")){
        String WLANSSID = jsonObj["WLAN"];
        Serial.printf("%s\n",WLANSSID);
        if(WLANSSID.equalsIgnoreCase(String("scan"))) {
          WiFiScan();
        } else {
          if(WLANSSID.equalsIgnoreCase(String("cube"))) {
            Serial.printf("cube Netz!!!");
            WiFi.softAP("Cube","Ehl95W23",0,0,1,false);
            WiFi.softAPConfig(IPAddress(192,158,17,1), IPAddress(192,158,17,1), IPAddress(255,255,255,0));
            //WiFi.softAP("cube");
          }
        }
        
      }
    }
    String response;
    serializeJson(data, response);
  
    request->send(200, "application/json", response);

  });
  server.addHandler(handler);
  CUBEserver.addHandler(handler);
  //ws.onEvent(onEvent);
  //server.addHandler(&ws);
  //CUBEserver.addHandler(&ws);
  server.onNotFound(notFound);
  CUBEserver.onNotFound(notFound);
  getPowerbank();
}


/*WIFI*/
const char* ssid = "dama";
const char* password = "8136699728311780";

const char* host = "CUBE";

/*test
const int LED_BUILTIN = 2;
*/

/*RELAY*/
const int RELAIS = 27;
// BUILDIN LED 2

void setRelais(bool state) {
      if (state) {
          digitalWrite(RELAIS,HIGH);
        } else {
          digitalWrite(RELAIS,LOW);
        }
}

void setPowerbank(bool state) {
  PowerBankState=state;
  getPowerbank();
  if(PowerBankState) {
    Serial.printf("Powerbank: An\n");
    digitalWrite(RELAIS,HIGH);
  } else {
    Serial.printf("Powerbank: Aus\n");
    digitalWrite(RELAIS,LOW);
  }
  Serial.printf("PIN %d = %d\n",RELAIS,digitalRead(RELAIS));
  delay(1000);
    //Serial.printf("%d\n",PowerBankState);
}

void addSecond(){
  if(tmLast.tm_sec<59){
    tmLast.tm_sec++;
  } else {
    tmLast.tm_sec=0;
    if(tmLast.tm_min<59){
      tmLast.tm_min++;
    } else {
      tmLast.tm_min=0;
      if(tmLast.tm_hour<23){
        tmLast.tm_hour++;
      } else {
        tmLast.tm_hour=0;
      }
    }
  }
}

void printLocalTime() {
  struct tm timeinfo;
  time_t myTime;
  // cMillis aktuelle Laufzeit ;-)
  if(cMillis-myRuntime>=1000) {
      addSecond();
      myRuntime=cMillis;
      Serial.printf("%02d:%02d:%02d\n",tmLast.tm_hour,tmLast.tm_min,tmLast.tm_sec);
    }
  return;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    Serial.printf("--:--:--\n");
    return;
  }
  if (memcmp(&timeinfo,&tmLast,sizeof(timeinfo))!=0) {
    //Serial.printf("%02d:%02d:%02d\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    tmLast=timeinfo;
    Serial.printf("%02d:%02d:%02d\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
    //Serial.printf("%s\n",lines[1]);
  }
  /*
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.print("Day of week: ");
  Serial.println(&timeinfo, "%A");
  Serial.print("Month: ");
  Serial.println(&timeinfo, "%B");
  Serial.print("Day of Month: ");
  Serial.println(&timeinfo, "%d");
  Serial.print("Year: ");
  Serial.println(&timeinfo, "%Y");
  Serial.print("Hour: ");
  Serial.println(&timeinfo, "%H");
  Serial.print("Hour (12 hour format): ");
  Serial.println(&timeinfo, "%I");
  Serial.print("Minute: ");
  Serial.println(&timeinfo, "%M");
  Serial.print("Second: ");
  Serial.println(&timeinfo, "%S");
  Serial.printf("Sommerzeit=%d\n",timeinfo.tm_isdst);
  Serial.println("Time variables");
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  Serial.println(timeHour);
  char timeWeekDay[10];
  strftime(timeWeekDay,10, "%A", &timeinfo);
  Serial.println(timeWeekDay);
  Serial.println();
  myTime=mktime(&timeinfo);
  struct tm ts2; 
   localtime_r(&myTime, &ts2);
  Serial.print("Time obtained in struct from myTime is: ");
  Serial.println(&ts2, "%A, %B %d %Y %H:%M:%S");
  */
}

void setupNTP() {
  configTime(0,0, ntpServer);
  setenv("TZ",TIMEZONE,1);
  tzset();
  tmLast.tm_hour=0;
  tmLast.tm_min=0;
  tmLast.tm_sec=0;
  printLocalTime();
}


void setupRELAIS() {
  pinMode(RELAIS,OUTPUT);
  PowerBankState=0;
  setPowerbank(PowerBankState);
}

void setupWifi() {
  Serial.print(F("\nStarting ESPAsync_WiFi using "));
  Serial.print(FS_Name);
  Serial.print(F(" on "));
  Serial.println(ARDUINO_BOARD);
  Serial.println(ESP_ASYNC_WIFI_MANAGER_LITE_VERSION);

#if USING_MRD
  Serial.println(ESP_MULTI_RESET_DETECTOR_VERSION);
#else
  Serial.println(ESP_DOUBLE_RESET_DETECTOR_VERSION);
#endif

  ESPAsync_WiFiManager = new ESPAsync_WiFiManager_Lite();
  String AP_SSID = host;
  String AP_PWD  = "configuration";
  // Set customized AP SSID and PWD
  ESPAsync_WiFiManager->setConfigPortal(AP_SSID, AP_PWD);
  ESPAsync_WiFiManager->setConfigPortalChannel(0);
  ESPAsync_WiFiManager->setConfigPortalIP(IPAddress(192,158,17,1));
  

#if USING_CUSTOMS_STYLE
  ESPAsync_WiFiManager->setCustomsStyle(NewCustomsStyle);
#endif

#if USING_CUSTOMS_HEAD_ELEMENT
  ESPAsync_WiFiManager->setCustomsHeadElement(PSTR("<style>html{filter: invert(10%);}</style>"));
#endif

#if USING_CORS_FEATURE
  ESPAsync_WiFiManager->setCORSHeader(PSTR("Your Access-Control-Allow-Origin"));
#endif

  // Set customized DHCP HostName
  //Or use default Hostname "ESP_XXXXXX"
  ESPAsync_WiFiManager->begin();
  WiFi.softAP("Cube","Ehl95W23",0,0,1,false);
  WiFi.softAPConfig(IPAddress(192,158,17,1), IPAddress(192,158,17,1), IPAddress(255,255,255,0));
}

void WiFiScan() 
{
int numSsid = WiFi.scanNetworks();
if(numSsid!=0) {
  Serial.printf("WLAN Netzwerke: %d\n",numSsid);
  for (int thisNet = 0; thisNet < numSsid; thisNet++) {
    Serial.print(thisNet);
    Serial.print(") ");
    Serial.print(WiFi.SSID(thisNet));
    Serial.print("\tSignal: ");
    Serial.print(WiFi.RSSI(thisNet));
    Serial.println();
  }
}
}

void heartBeatPrint()
{
  static int num = 1;
  
  

  if (WiFi.status() == WL_CONNECTED) {
    //Serial.print("H");        // H means connected to WiFi
    Serial.printf("%s -> %s...%s",ESPAsync_WiFiManager->localIP(),WiFi.SSID(),"AP-IP=");
    Serial.print(WiFi.softAPIP());
    Serial.println();
  }
  else
  {
    if (ESPAsync_WiFiManager->isConfigMode()) {
      //Serial.print("C");        // C means in Config Mode
      Serial.printf("%s\n",ESPAsync_WiFiManager->localIP());
    }
    else {
      //Serial.print("F");        // F means not connected to WiFi
      Serial.printf("IP: ---.---.---.---\n");
    }
  }

  if (num == 80)
  {
    Serial.println();
    num = 1;
  }
  else if (num++ % 10 == 0)
  {
    Serial.print(F(" "));
  }
}

void check_status()
{
  static unsigned long checkstatus_timeout = 0;

  //KH
#define HEARTBEAT_INTERVAL    20000L
  // Print hearbeat every HEARTBEAT_INTERVAL (20) seconds.
  if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
  {
    heartBeatPrint();
    checkstatus_timeout = millis() + HEARTBEAT_INTERVAL;
  }
}

void setupOTA() {
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  AsyncElegantOTA.begin(&server,AsyncUser,AsyncPWD);
  setup_routing();
  server.begin();
}

void setup() {
/*  // put your setup code here, to run once:
*/
  Serial.begin(115200);
  //SerialBT.begin("CUBE");
  setupWifi();
  setupOTA();
  setupNTP();
  setupRELAIS();
  //setupgps();
}

void loop() {
  cMillis = millis();
  //parsegps();  
  ESPAsync_WiFiManager->run();
  check_status();
  /*if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }*/
  //printLocalTime();
}
