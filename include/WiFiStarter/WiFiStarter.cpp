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

WiFiManager wm;
WiFiManagerParameter custom_text_box("key_text", "Pausar (s/n)?", "n", 1);

class WiFibro
{
  public:
    void init_wifi();
    String inputDoUsuario();
    void processar_pagina_html();
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
 
  // bool spiffsSetup = WiFibro::loadConfigFile();
  // if (!spiffsSetup)
  // {
  //   Serial.println(F("Forcing config mode as there is no saved config"));
  //   forceConfig = true;
  // }
 
  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);
 
  // Setup Serial monitor
  Serial.begin(115200);
  delay(10);

  wm.setConfigPortalBlocking(false);
  // wm.setSTAStaticIPConfig(IPAddress(192,168,0,99), IPAddress(192,168,0,1), IPAddress(255,255,255,0)); // optional DNS 4th argument
  // Reset settings (only for development)
  // wm.resetSettings();
 
  // Set config save notify callback
  wm.setSaveConfigCallback(WiFibro::saveConfigCallback);
 
  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(WiFibro::configModeCallback);
 
  // Custom elements
 
  // Text box (String) - 50 characters maximum
  // WiFiManagerParameter custom_text_box("key_text", "Pausar (s/n)?", "n", 1);
  
  // Need to convert numerical input to string to display the default value.
  // char convertedValue[6];
  // sprintf(convertedValue, "%d", testNumber); 
  
  // Text box (Number) - 7 characters maximum
  // WiFiManagerParameter custom_text_box_num("key_num", "Enter your number here", convertedValue, 7); 
 
  // Add all defined parameters
  wm.addParameter(&custom_text_box);
  // WiFibro::wm.addParameter(&custom_text_box_num);
 
  if(wm.autoConnect("Sensor de Porta", "123456789"))
  {
      Serial.println("connected...yeey :)");
  }
  else {
      Serial.println("Configportal running");
  }
 
  // If we get here, we are connected to the WiFi
 
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  wm.startConfigPortal("Sensor de Porta", "123456789");
  // Lets deal with the user config values
 
  // Copy the string value
  // strncpy(testString, custom_text_box.getValue(), sizeof(testString));
  // Serial.print("testString: ");
  // Serial.println(testString);
 
  //Convert the number value
  // testNumber = atoi(custom_text_box_num.getValue());
  // Serial.print("testNumber: ");
  // Serial.println(testNumber);
 
 
  // Save the custom parameters to FS
  // if (shouldSaveConfig)
  // {
  //   saveConfigFile();
  // }
}

String WiFibro::inputDoUsuario()
{
  return custom_text_box.getValue();
}

void WiFibro::processar_pagina_html()
{
  wm.process();
}

void WiFibro::saveConfigFile()
// Save Config in JSON format
{
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  StaticJsonDocument<512> json;
  // json["testString"] = testString;
  // json["testNumber"] = testNumber;
 
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
 
          // strcpy(testString, json["testString"]);
          // testNumber = json["testNumber"].as<int>();
 
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