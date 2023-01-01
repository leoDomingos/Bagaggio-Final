#ifndef BANCODEDADOS
#define BANCODEDADOS
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "EEPROM.h"

const char* serverNameLoja = "https://database-bagaggio.onrender.com/registroLoja";
const char* serverNameSensor = "https://database-bagaggio.onrender.com/registroSensor";

#define EEPROM_SIZE 8
#define EEPROM_COUNT_ADDRESS 0
#define EEPROM_HOUR_ADDRESS 1
#define SAVE_INTERVALO_MAX 2

class Banco_de_Dados
{
    public:
        static inline int led_banco_de_dados = 23;
        int registrar_leituras(int pessoas, String data, String id_loja);
        int registrar_loja(String id_loja);
        String httpGETRequest(const char* serverName);
        // String getRegistroLoja(char* ssid, char* password);
        struct tm getDate();
        String getFormatedDate(struct tm timeinfo);
        int saveCount(int pessoas, int hora_do_registro);
        int readSaveCount(int hora_atual);
};

int Banco_de_Dados::registrar_leituras(int pessoas, String data, String id_loja)
{
  WiFiClient client;
  HTTPClient http;
  // "{\"numero_pessoas\":\"pessoas\",\"datetime\":\"06/11/2022\"}"
  http.begin(serverNameSensor);
  http.addHeader("Content-Type", "application/json");
  String sensor_json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String(",") + String('"') + String("numero_pessoas") + String('"') + String(':') + String('"') + String(pessoas) + String('"') + String(",") + String('"') + String("datetime") + String('"') + String(':') + String('"') + String(data) + String('"') + String("}");
  int sensor_resposta = http.POST(String(sensor_json));
  if (sensor_resposta>0) 
  {
    return 1;
  }
  else
  {
    Serial.print("Error code (sensor): ");
    Serial.println(sensor_resposta);
    return 0;
  }
  http.end();
}

int Banco_de_Dados::registrar_loja(String id_loja)
{
  WiFiClient client;
  HTTPClient http;
  String loja_json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String("}");
  http.begin(serverNameLoja);
  http.addHeader("Content-Type", "application/json");
  int loja_resposta = http.POST(String(loja_json));
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

struct tm Banco_de_Dados::getDate()
{
  const char* ntpServer = "pool.ntp.org"; // Servidor que vai nos fornecer a data

  const long gmtOffset_sec = -3600 * 3; // Quantas horas estamos atrasados em relação a GMT

  const int daylightOffset_sec = 3600; // Offset do horário de verão

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;

  // if(!getLocalTime(&timeinfo))
  // {
  //   Serial.println("Failed to obtain time");
  //   return 0;
  // }
  return timeinfo;
}

String Banco_de_Dados::getFormatedDate(struct tm timeinfo)
{
  char data[21];
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);

  char timeMonth[10];
  strftime(timeMonth,10, "%B", &timeinfo);

  char timeDay[3];
  strftime(timeDay,3, "%d", &timeinfo);

  char timeYear[5];
  strftime(timeYear,5, "%Y", &timeinfo);
  strcat(data, timeDay);
  strcat(data,"/");
  strcat(data,timeMonth);
  strcat(data,"/");
  strcat(data,timeYear);
  strcat(data," ");
  strcat(data,timeHour);
  strcat(data,":00");
  return data;
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
  if (!EEPROM.begin(EEPROM_SIZE)) 
  {
    Serial.println("failed to init EEPROM");
    return -1;
  }
  EEPROM.write(pessoas, EEPROM_COUNT_ADDRESS);
  EEPROM.write(hora_do_registro, EEPROM_HOUR_ADDRESS);
  EEPROM.commit();
  return 1;
}

int Banco_de_Dados::readSaveCount(int hora_atual)
{
  if (!EEPROM.begin(EEPROM_SIZE)) 
  {
    Serial.println("failed to init EEPROM");
    return -1;
  }
  int pessoas = EEPROM.read(EEPROM_COUNT_ADDRESS);
  int ultimo_registro = EEPROM.read(EEPROM_HOUR_ADDRESS);
  if (abs(hora_atual - ultimo_registro) >= SAVE_INTERVALO_MAX)
  {
    return 0;
  }
  else
  {
    return pessoas;
  }
}

#endif