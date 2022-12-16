#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"

/*
TODOS:
- LED de alinhamento DONE
- Parte do banco de dados
- Indicação do banco de dados

*/

int entraram = 0;                   // Quantidade de pessoas que...

const char* ssid     = "JUIZADODAINF";    // Nome do Wifi
const char* password = "100juizo2018";   // Senha

const char* serverNameLoja = "https://database-bagaggio.onrender.com/registroLoja";
const char* serverNameSensor = "https://database-bagaggio.onrender.com/registroSensor";

int ultima_conexao = 0;


void printar_leituras();
bool is_obstruido(int leitura, int limite);
void printar_obs();
bool is_obs_valid(ulong obs);
void printar_display(int numero);
void escrever_segmento(int display[8], int valor[8]);
void esperar_alinhamento();
String to_json(int pessoas, String data, String id_loja);
String registrar_loja(String id_loja);



void setup() {
  Serial.begin(115200);          //Inicialização do monitor serial
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) { 
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
}


void loop() {
  entraram = 47;
  
  if(WiFi.status()== WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(serverNameLoja);
    
    http.addHeader("Content-Type", "application/json");

    const char* ntpServer = "pool.ntp.org"; // Servidor que vai nos fornecer a data

    const long gmtOffset_sec = -3600 * 3; // Quantas horas estamos atrasados em relação a GMT

    const int daylightOffset_sec = 3600; // Offset do horário de verão

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    struct tm timeinfo;

    if(!getLocalTime(&timeinfo))
    {
      Serial.println("Failed to obtain time");
      return;
    }
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

    // Prepare your HTTP POST request data
    delay(5000);
    int httpResponseCodeLoja = http.POST(String(registrar_loja(String("alalal"))));
    Serial.print("Loja: ");
    Serial.println(httpResponseCodeLoja);
    http.end();

    http.begin(serverNameSensor);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCodeSensor = http.POST(String(to_json(entraram, data, String("alalal"))));
    Serial.print("Sensor: ");
    Serial.println(httpResponseCodeSensor);
    http.end();
  }
}

String to_json(int pessoas, String data, String id_loja)
{
  // "{\"numero_pessoas\":\"pessoas\",\"datetime\":\"06/11/2022\"}"
  String json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String(",") + String('"') + String("numero_pessoas") + String('"') + String(':') + String('"') + String(pessoas) + String('"') + String(",") + String('"') + String("datetime") + String('"') + String(':') + String('"') + String(data) + String('"') + String("}");
  return json;
}

String registrar_loja(String id_loja)
{
  String json = String('{') + String('"') + String("codigo_loja") + String('"') + String(':') + String('"') + String(id_loja) + String('"') + String("}");
  return json;
}
