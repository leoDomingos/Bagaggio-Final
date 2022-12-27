#ifndef WIFICLIENT
#define WIFICLIENT

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>

#define JSON_CONFIG_FILE "/test_config.json"
bool shouldSaveConfig = false;

char testString[50] = "test value";
int testNumber = 1234;

const char* serverNameLoja = "https://database-bagaggio.onrender.com/registroLoja";
const char* serverNameSensor = "https://database-bagaggio.onrender.com/registroSensor";

class WiFibro
{
  public:
    WiFiManager wm;

    String to_json(int pessoas, String data, String id_loja);
    String registrar_loja(String id_loja);
    String httpGETRequest(const char* serverName);
    void init_wifi();
  private:
    bool loadConfigFile();
    void saveConfigFile();
    static void saveConfigCallback();
    static void configModeCallback(WiFiManager *myWiFiManager);
};

void WiFibro::init_wifi()
{
  // Change to true when testing to force configuration every time we run
  bool forceConfig = false;
 
  bool spiffsSetup = WiFibro::loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }
 
  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);
 
  // Setup Serial monitor
  Serial.begin(115200);
  delay(10);
 
  // Reset settings (only for development)
  // wm.resetSettings();
 
  // Set config save notify callback
  WiFibro::wm.setSaveConfigCallback(WiFibro::saveConfigCallback);
 
  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  WiFibro::wm.setAPCallback(WiFibro::configModeCallback);
 
  // Custom elements
 
  // Text box (String) - 50 characters maximum
  WiFiManagerParameter custom_text_box("key_text", "Enter your string here", testString, 50);
  
  // Need to convert numerical input to string to display the default value.
  char convertedValue[6];
  sprintf(convertedValue, "%d", testNumber); 
  
  // Text box (Number) - 7 characters maximum
  WiFiManagerParameter custom_text_box_num("key_num", "Enter your number here", convertedValue, 7); 
 
  // Add all defined parameters
  WiFibro::wm.addParameter(&custom_text_box);
  WiFibro::wm.addParameter(&custom_text_box_num);
 
  if (forceConfig)
    // Run if we need a configuration
  {
    if (!WiFibro::wm.startConfigPortal("NEWTEST_AP", "password"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      ESP.restart();
      delay(5000);
    }
  }
  else
  {
    if (!WiFibro::wm.autoConnect("NEWTEST_AP", "password"))
    {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      // if we still have not connected restart and try all over again
      ESP.restart();
      delay(5000);
    }
  }
 
  // If we get here, we are connected to the WiFi
 
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  // Lets deal with the user config values
 
  // Copy the string value
  strncpy(testString, custom_text_box.getValue(), sizeof(testString));
  Serial.print("testString: ");
  Serial.println(testString);
 
  //Convert the number value
  testNumber = atoi(custom_text_box_num.getValue());
  Serial.print("testNumber: ");
  Serial.println(testNumber);
 
 
  // Save the custom parameters to FS
  if (shouldSaveConfig)
  {
    saveConfigFile();
  }
}

String WiFibro::to_json(int pessoas, String data, String id_loja)
{
  // "{\"numero_pessoas\":\"pessoas\",\"datetime\":\"06/11/2022\"}"
  String json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String(",") + String('"') + String("numero_pessoas") + String('"') + String(':') + String('"') + String(pessoas) + String('"') + String(",") + String('"') + String("datetime") + String('"') + String(':') + String('"') + String(data) + String('"') + String("}");
  return json;
}

String WiFibro::registrar_loja(String id_loja)
{
  String json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String("}");
  return json;
}

String WiFibro::httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(serverName);
  
  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void WiFibro::saveConfigFile()
// Save Config in JSON format
{
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  json["testString"] = testString;
  json["testNumber"] = testNumber;
 
  // Open config file
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    // Error, file did not open
    Serial.println("failed to open config file for writing");
  }
 
  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  configFile.close();
}
 
bool WiFibro::loadConfigFile()
// Load existing configuration file
{
  // Uncomment if we need to format filesystem
  // SPIFFS.format();
 
  // Read configuration from FS json
  Serial.println("Mounting File System...");
 
  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("Opened configuration file");
        StaticJsonDocument<512> json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          Serial.println("Parsing JSON");
 
          strcpy(testString, json["testString"]);
          testNumber = json["testNumber"].as<int>();
 
          return true;
        }
        else
        {
          // Error loading JSON data
          Serial.println("Failed to load json config");
        }
      }
    }
  }
  else
  {
    // Error mounting file system
    Serial.println("Failed to mount FS");
  }
 
  return false;
}
 
void WiFibro::saveConfigCallback()
// Callback notifying us of the need to save configuration
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
 
void WiFibro::configModeCallback(WiFiManager *myWiFiManager)
// Called when config mode launched
{
  Serial.println("Entered Configuration Mode");
 
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
 
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}
#endif