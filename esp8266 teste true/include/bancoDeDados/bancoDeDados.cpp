#ifndef BANCODEDADOS
#define BANCODEDADOS
#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <WiFiClientSecure.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <WiFiClient.h>

#include <ArduinoJson.h>
#include "time.h"
#include "EEPROM.h"
#include <FS.h>
#include "LittleFS.h"

const char* serverNameLoja = "https://database-bagaggio-teste.onrender.com/registroLoja";
const char* serverNameSensor = "https://database-bagaggio-teste.onrender.com/registroSensor";

#define EEPROM_SIZE 8
#define EEPROM_COUNT_ADDRESS 0
#define EEPROM_HOUR_ADDRESS 1
#define SAVE_INTERVALO_MAX 1
#define DOC_SIZE 5000
#define FORMAT_LITTLEFS_IF_FAILED true
#define JSON_SAVE_FILE "/saves.json"

class Banco_de_Dados
{
    public:
        static inline int led_banco_de_dados = 14;
        int registrar_leituras(String pessoas, String data, String id_loja);
        int registrar_loja(String id_loja);
        String httpGETRequest(const char* serverName);
        // String getRegistroLoja(char* ssid, char* password);
        struct tm getDate();
        String getFormatedDate(struct tm timeinfo);
        int saveCount(int pessoas, int hora_do_registro);
        int readSaveCount(int hora_atual);
        void addCredentials(const char * registro, const char * data, const char * contagem);
        void writeFile(fs::FS &fs, const char * path, DynamicJsonDocument message);
        void grabFirst(String* registro, String* data, String* contagem);
        DynamicJsonDocument readFile(fs::FS &fs, const char * path);
};

void Banco_de_Dados::addCredentials(const char * registro, const char * data,const char * contagem){
  DynamicJsonDocument doc(DOC_SIZE);// create json doc
  String Serialized; // create string to store serialized json

  File credFile = LittleFS.open(JSON_SAVE_FILE, "r");// open file for reading

if( !credFile ){ Serial.println("Failed to open file"); return; }// if file doesn't exist, return
    
DeserializationError error = deserializeJson(doc, credFile);// deserialize json
if( error ){ Serial.printf("Error on deserialization: %s\n", error.c_str() );} //error when spiff is empty or not formatted correctly

JsonArray inforArr = doc["information"].as<JsonArray>();// get array from json


JsonObject newCred = doc.createNestedObject();// create new object in json
newCred["codigo_loja"] = registro;
newCred["data"] = data;
newCred["contagem"] = contagem;

 
serializeJsonPretty(doc, Serialized);

File credFile2 = LittleFS.open(JSON_SAVE_FILE, "w");// open file for writing

credFile2.print(Serialized);

credFile2.close();
credFile.close();
Serialized = "";      

}

void Banco_de_Dados::writeFile(fs::FS &fs, const char * path, DynamicJsonDocument message)
{
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if(!file){
    Serial.println("- failed to open file for writing");
  }
  serializeJson(message, file);
  file.close();
}

void Banco_de_Dados::grabFirst(String* registro, String* data, String* contagem){
  DynamicJsonDocument doc(DOC_SIZE);
  int index_alvo = 0;
  File credFile = LittleFS.open(JSON_SAVE_FILE, "r");// open file for reading
  DeserializationError error = deserializeJson(doc, credFile);// deserialize json
  if( !credFile ){ Serial.println("Failed to open file"); return; }// if file doesn't exist, return 
   
  const char* registro_salvo = doc[index_alvo]["codigo_loja"]; //
  const char* data_salvo = doc[index_alvo]["data"]; // 
  const char* contagem_salvo = doc[index_alvo]["contagem"]; //
  credFile.close();
  
  *registro = registro_salvo;
  *data = data_salvo;
  *contagem = contagem_salvo;
}

DynamicJsonDocument Banco_de_Dados::readFile(fs::FS &fs, const char * path){
    // Serial.printf("Reading file: %s\r\n", path);

    File file = LittleFS.open(path, "r");
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
    }
    DynamicJsonDocument json(5000);
    DeserializationError error = deserializeJson(json, file);
    serializeJsonPretty(json, Serial);
    file.close();
    return json;
}

int Banco_de_Dados::registrar_leituras(String pessoas, String data, String id_loja)
{
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, serverNameSensor);
  http.addHeader("Content-Type", "application/json");
  String sensor_json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String(",") + String('"') + String("numero_pessoas") + String('"') + String(':') + String('"') + String(pessoas) + String('"') + String(",") + String('"') + String("datetime") + String('"') + String(':') + String('"') + String(data) + String('"') + String("}");
  int sensor_resposta = http.POST(String(sensor_json));
  Serial.println(sensor_resposta);
  if (sensor_resposta>0) 
  {
    return 1;
  }
  else
  {
    Serial.print("Erro ao mandar para o banco de dados: ");
    Serial.println(sensor_resposta);
    return 0;
  }
  http.end();
}

int Banco_de_Dados::registrar_loja(String id_loja)
{
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String loja_json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String("}");
  http.begin(client, "https://database-bagaggio-teste.onrender.com/registroLoja");
  http.addHeader("Content-Type", "application/json");
  int loja_resposta = http.POST(String(loja_json));
  Serial.println(loja_resposta);
  if (loja_resposta>0) 
  {
    return 1;
  }
  else
  {
    Serial.print("Error code (loja): ");
    Serial.println(loja_resposta);
    return 0;
  }
  http.end();
}

String Banco_de_Dados::httpGETRequest(const char* serverName) 
{
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
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

// String Banco_de_Dados::getRegistroLoja(char* ssid, char* password)
// {
//   String resposta;
//   StaticJsonDocument<25000> doc;
//   const char* servidor_com_registros = "";
//   String resposta = Banco_de_Dados::httpGETRequest(servidor_com_registros);
//   DeserializationError error = deserializeJson(doc, resposta);
//   const char* lojas = doc[0]["codigo_loja"];
//   // Test if parsing succeeds.
//   if (error) {
//     Serial.print(F("deserializeJson() failed: "));
//     Serial.println(error.f_str());
//     return;
//   }
//   else
//   {
//     Serial.println(lojas);
//   }
// }

int Banco_de_Dados::saveCount(int pessoas, int hora_do_registro)
{
  EEPROM.begin(EEPROM_SIZE);

  EEPROM.write(EEPROM_COUNT_ADDRESS, pessoas);
  EEPROM.write(EEPROM_HOUR_ADDRESS, hora_do_registro);
  EEPROM.commit();
  return 1;
}

int Banco_de_Dados::readSaveCount(int hora_atual)
{
  EEPROM.begin(EEPROM_SIZE);

  int pessoas = EEPROM.read(EEPROM_COUNT_ADDRESS);
  int ultimo_registro = EEPROM.read(EEPROM_HOUR_ADDRESS);
  if (abs(hora_atual - ultimo_registro) > SAVE_INTERVALO_MAX)
  {
    return 0;
  }
  else
  {
    Serial.println("Recuperando contagem antiga");
    return pessoas;
  }
}

#endif