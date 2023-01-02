#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
/*
TODOS:
- Consertar problema de falta de alinhamento se passar várias pessoas (FEITOOO)
- Verificar mecânica de mandar ao banco de dados (reseta a contagem, a cada 1h, necessidade de wifi, etc) (FEITO)
- Fazer ele escrever na memória a contagem a cada 5 min (FEITO)
- Inverter funcionamento do LED de alinhamento (FEITO)
- Terminar esqueleto da implementação com o db deles
- Implementar NTP (FEITO)
- Botar return nas funções POST (FEITO)

- Testar tudo
*/

#include "./Detector/detector.cpp"
#include "./bancoDeDados/bancoDeDados.cpp"
#include "./WiFiStarter/WiFiStarter.cpp"

int ultima_conexao_hora;
int ultimo_save_contagem_min;
String registro_loja = "defin5";
struct tm timeinfo;

#define INTERVALO_REGISTROS_HORA 1
#define INTERVALO_SALVAR_CONTAGEM_MIN 2

const char* ntpServer = "pool.ntp.org"; // Servidor que vai nos fornecer a data
const long gmtOffset_sec = -3600 * 3; // Quantas horas estamos atrasados em relação a GMT
const int daylightOffset_sec = 3600; // Offset do horário de verão

Detector detector;
Banco_de_Dados banco_de_dados;
WiFibro WiFiStarter;

void luzes_de_espera();

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
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  // wifi_config_t conf;
  // registro_loja = banco_de_dados.getRegistroLoja((char*)conf.sta.ssid, (char*)conf.sta.password);
  banco_de_dados.registrar_loja(registro_loja); // Lidar com erro
  getLocalTime(&timeinfo);
  detector.entraram = banco_de_dados.readSaveCount(int(timeinfo.tm_hour)); // Lidar com -1
  if (detector.entraram != 0) 
  {
    Serial.print("Contagem de pessoas recuperada: ");
    Serial.println(detector.entraram);
    delay(1000);
  }
  ultimo_save_contagem_min = int(timeinfo.tm_min);
  ultima_conexao_hora = int(timeinfo.tm_hour);
}

void loop() {
  digitalWrite(detector.led_on, HIGH);
  WiFiStarter.processar_pagina_html();
  
  while (WiFiStarter.inputDoUsuario() == "s")
  {
    luzes_de_espera();
  }

  detector.observar();
  digitalWrite(detector.led_alinhamento, HIGH);
  getLocalTime(&timeinfo);
  if(WiFi.status()== WL_CONNECTED){
    digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);

    if (abs(int(timeinfo.tm_min) - ultimo_save_contagem_min) >= INTERVALO_SALVAR_CONTAGEM_MIN)
    {
      Serial.println("Salvando contagem na memória.");
      banco_de_dados.saveCount(detector.entraram, int(timeinfo.tm_hour));
      ultimo_save_contagem_min = int(timeinfo.tm_min);
      delay(500);
    }
    
    if (abs(int(timeinfo.tm_hour) - ultima_conexao_hora) >= INTERVALO_REGISTROS_HORA)
    {
      Serial.println("Mandando leituras para o banco de dados.");
      Serial.println(abs(int(timeinfo.tm_hour) - ultima_conexao_hora));
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
      int sensor_leituras_sucesso = banco_de_dados.registrar_leituras(detector.entraram, data, registro_loja);
      detector.entraram = 0;
      banco_de_dados.saveCount(detector.entraram, int(timeinfo.tm_hour));
      ultima_conexao_hora = int(timeinfo.tm_hour);
      delay(500);
    }
  }
  else
  {
    digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
  }
}


void luzes_de_espera()
{
  digitalWrite(detector.led_on, HIGH);
  digitalWrite(detector.led_alinhamento, LOW);
  digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
  WiFiStarter.processar_pagina_html();
  delay(800);
  digitalWrite(detector.led_on, LOW);
  digitalWrite(detector.led_alinhamento, HIGH);
  digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
  WiFiStarter.processar_pagina_html();
  delay(800);
  digitalWrite(detector.led_on, LOW);
  digitalWrite(detector.led_alinhamento, LOW);
  digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);
  WiFiStarter.processar_pagina_html();
  delay(800);
  digitalWrite(detector.led_on, LOW);
  digitalWrite(detector.led_alinhamento, HIGH);
  digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
  WiFiStarter.processar_pagina_html();
  delay(800);
}