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

int ultima_conexao_hora = 0;
int ultima_conexao_min = 0;
int ultimo_save_contagem_min = 0;
String registro_loja;
struct tm timeinfo;
bool ja_mandou = false;
bool ja_salvou = false;


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
  // int registro_loja_sucesso = banco_de_dados.registrar_loja(registro_loja); // Lidar com erro
  detector.entraram = banco_de_dados.readSaveCount(timeinfo.tm_hour); // Lidar com -1
  ultimo_save_contagem_min = timeinfo.tm_min;
  ultima_conexao_hora = timeinfo.tm_hour;
}

void loop() {
  digitalWrite(detector.led_on, HIGH);
  WiFiStarter.processar_pagina_html();
  while (WiFiStarter.inputDoUsuario() == "s")
  {
    luzes_de_espera();
  }

  detector.observar();
  getLocalTime(&timeinfo);
  if(WiFi.status()== WL_CONNECTED){
    digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);

    if (abs(timeinfo.tm_min - ultimo_save_contagem_min) >= INTERVALO_SALVAR_CONTAGEM_MIN)
    {
      Serial.println("Salvando contagem na memória.");
      Serial.print("Minuto: ");
      Serial.println(timeinfo.tm_min);
      Serial.print("ultimo_save: ");
      Serial.println(ultimo_save_contagem_min);
      banco_de_dados.saveCount(detector.entraram, timeinfo.tm_hour);
      ultimo_save_contagem_min = timeinfo.tm_min;
    }
    
    if (abs(timeinfo.tm_hour - ultima_conexao_hora) >= INTERVALO_REGISTROS_HORA)
    {
      Serial.println("Mandando leituras para o banco de dados.");
      Serial.print("Hora: ");
      Serial.println(timeinfo.tm_hour);
      Serial.print("ultima_conexao: ");
      Serial.println(ultima_conexao_hora);
      // int sensor_leituras_sucesso = banco_de_dados.registrar_leituras(detector.entraram, banco_de_dados.getFormatedDate(timeinfo), registro_loja);
      detector.entraram = 0;
      banco_de_dados.saveCount(detector.entraram, timeinfo.tm_hour);
      ultima_conexao_hora = timeinfo.tm_hour;
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