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

#include "./Detector/detector.cpp"
#include "./bancoDeDados/bancoDeDados.cpp"
#include "./WiFiStarter/WiFiStarter.cpp"

int ultima_conexao = 0;

Detector detector;
Banco_de_Dados banco_de_dados;
WiFibro WiFiStarter;

void setup() {
  Serial.begin(115200);          //Inicialização do monitor serial
  pinMode(detector.led_alinhamento, OUTPUT);
  pinMode(banco_de_dados.led_banco_de_dados, OUTPUT);
  pinMode(detector.esq_receptor, INPUT);  //Definindo os pinos dos fotorreceptores
  pinMode(detector.dir_receptor, INPUT); //
  pinMode(detector.led_alinhamento, OUTPUT);
  pinMode(detector.led_on, OUTPUT);
  digitalWrite(detector.led_on, HIGH);
  WiFiStarter.init_wifi();
}


void loop() {
  detector.observar();
  
  if(WiFi.status()== WL_CONNECTED){
    digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(serverNameLoja);
    http.addHeader("Content-Type", "application/json");
    http.POST(String(banco_de_dados.registrar_loja("tstpcb")));
    http.end();

    http.begin(serverNameSensor);
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

    if (timeinfo.tm_hour - ultima_conexao >= 1)
    {
    // Prepare your HTTP POST request data
    int httpResponseCode = http.POST(String(banco_de_dados.readings_to_json(detector.entraram, data, "tstpcb")));
    if (httpResponseCode>0) 
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    }
    else 
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    ultima_conexao = timeinfo.tm_hour;
    }
    http.end();
  }
}
