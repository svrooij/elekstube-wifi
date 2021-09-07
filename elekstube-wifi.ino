#include <FS.h> // Reading files from flash
#include <WiFiManager.h> // Installed from library manager, v2.0.3-alpha
#include <SoftwareSerial.h> // Installed from library manager (EspSoftwareSerial), v6.13.2
#include <ArduinoJson.h> // Installed from Library manager, v6.18.3
#include <ArduinoOTA.h> // Used to enable Over-the-air updates
#include <WiFiUdp.h> // Used for custom NTP messages
#include <NTPClient.h> // Installed from library manager, v3.2.0

// Pin Configuration (for Elekstube serial port)
// Tested connection
//
// Elekstube  Wemos D1 mini
// RX         D4
// 5V         5V
// -          GND
//
// Currently the TX port is not used
#define TUBE_RX D4
#define TUBE_TX D2

// Config values (loaded from the WifiManager).
char ntp_server[30] = "nl.pool.ntp.org";
char ntp_offset[7] = "7200";
bool shouldSaveConfig = false;

// Elekstube stuff
#define ORANGE "402000"
const char TUBE_REALTIME[2] = "/";
const char TUBE_SETTIME[2] = "*";
SoftwareSerial tubePort;

// NTP Stuff
WiFiUDP udp_ntp;
NTPClient timeClient(udp_ntp, ntp_server, 0, 900000); // Interval 15 minutes, 15 * 60000

// Arduino OTA
void setupOTA() {
  Serial.println("Setting up OTA");
  ArduinoOTA.setHostname("elekstube");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"clock");

  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

// Save callback
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void saveConfigIfNessecary() {
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument json(1024);
    json["ntp_server"] = ntp_server;
    json["ntp_offset"] = ntp_offset;
    
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    serializeJsonPretty(json, Serial);
    serializeJson(json, configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }
}

void setupSpiffs(){
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, configFile);
        if (error) {
          Serial.println("Error deserialize json");
          return;
        }
        
        Serial.println("\nparsed json");

        strcpy(ntp_server, doc["ntp_server"]);
        strcpy(ntp_offset, doc["ntp_offset"]);
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setupWifiWithManager() {
  // Initialize wifi manager
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  // Add two custom parameters
  WiFiManagerParameter custom_ntp_server("ntp_server", "NTP Server", ntp_server, 30);
  // This should be a number instead of a string (is that possible?)
  WiFiManagerParameter custom_ntp_offset("ntp_offset", "NTP Offset", ntp_offset, 8);
  wm.addParameter(&custom_ntp_server);
  wm.addParameter(&custom_ntp_offset);

  // Auto connect or start portal
  if (!wm.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

  // If we get here there must be a wifi connection
  Serial.println("Connected to wifi with IP:");
  Serial.println(WiFi.localIP());

  // Read parameters
  strcpy(ntp_server, custom_ntp_server.getValue());
  strcpy(ntp_offset, custom_ntp_offset.getValue());
}

void setup() {
  setupSpiffs();
  
  //Set pin mode for led
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Debug serial, using the regular serial
  Serial.begin(38400);
  delay(1000);
  Serial.println("Elekstube wifi sync started");

  setupWifiWithManager();

  saveConfigIfNessecary();

  setupOTA();
  
  // Secondary serial for clock (this is send only, so the Rx port is set to -1)
  // baud     115200          needed speed
  // config   SWSERIAL_8N1    good guess
  // rxPin    -1              no receive pin for software serial
  // txPin    TUBE_RX         as defined above
  // invert   false           no inverted signal
  tubePort.begin(115200, SWSERIAL_8N1, -1, TUBE_RX, false);

  if(!tubePort) {
    Serial.println("Tube port not created");
  }

  // Start UDP port and set offset dynamicly from settings.
  timeClient.begin();
  timeClient.setTimeOffset(atoi(ntp_offset));
  
}

void loop() {
  ArduinoOTA.handle();
  
  if(!tubePort) {
    blink();
    return;
  }
  
  //Serial.println("Sending message to clock");
  timeClient.update();
  writeModeToTube(0);
  writeNtpTimeToTube();
  delay(1000);
}

void blink() {
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
    delay(500);                      // Wait for half a second
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
    delay(500); 
}

void writeNtpTimeToTube() {
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();

  int h1 = hours / 10;
  int h2 = hours % 10;
  int m1 = minutes / 10;
  int m2 = minutes % 10;
  int s1 = seconds / 10;
  int s2 = seconds % 10;
  writeTimeToTube(h1, h2, m1, m2, s1, s2);
  Serial.print("Current NTP time: ");
  Serial.print(timeClient.getFormattedTime());
  //blink();
}

void writeTimeToTube(int h1, int h2, int m1, int m2, int s1, int s2) {
  String clock_out = (String(TUBE_SETTIME) + String(h1) + String(ORANGE) + String(h2) + String(ORANGE) + String(m1) + String(ORANGE)+ String(m2) + String(ORANGE)+ String(s1) + String(ORANGE)+ String(s2) + String(ORANGE));
  tubePort.print(clock_out);
  Serial.println(clock_out);
}

// Write new mode to Tube (0 = normal, 1 = rainbow, 2 = water fall, 3 = test mode, 4 = random);
void writeModeToTube(int newmode) {
  String clock_out = (String("$0") + String(newmode));
  Serial.println(clock_out);
  tubePort.print(clock_out);
}
