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

int ultima_conexao_min = 0;
int ultima_contagem_min = 0;
String registro_loja;
struct tm timeinfo;
bool ja_mandou = false;
bool ja_salvou = false;

#define INTERVALO_REGISTROS 1
#define INTERVALO_SALVAR_CONTAGEM 5


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
  // WiFiStarter.init_wifi();
  // wifi_config_t conf;
  // registro_loja = banco_de_dados.getRegistroLoja((char*)conf.sta.ssid, (char*)conf.sta.password);
  // int registro_loja_sucesso = banco_de_dados.registrar_loja(registro_loja); // Lidar com erro
  // detector.entraram = banco_de_dados.readSaveCount(timeinfo.tm_hour); // Lidar com -1
}

void loop()
{
  detector.observar();
}
// void loop() {
//   int contagem_antiga = detector.entraram;
//   detector.observar();
//   if (contagem_antiga != detector.entraram)
//   {
//     ultima_contagem_min = timeinfo.tm_min;
//   }
//   if(WiFi.status()== WL_CONNECTED){
//     digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);

//     timeinfo = banco_de_dados.getDate();
//     if (abs(timeinfo.tm_min - ultima_contagem_min) >= INTERVALO_SALVAR_CONTAGEM)
//     {
//       if (!ja_salvou)
//       {
//       banco_de_dados.saveCount(detector.entraram, timeinfo.tm_hour);
//       ja_salvou = true;
//       }
//     }
//     else
//     {
//       ja_salvou = false;
//     }
//     if (abs(timeinfo.tm_hour - ultima_conexao_min) >= INTERVALO_REGISTROS)
//     {
//       if (!ja_mandou)
//       {
//         int sensor_leituras_sucesso = banco_de_dados.registrar_leituras(detector.entraram, banco_de_dados.getFormatedDate(timeinfo), registro_loja);
//         detector.entraram = 0;
//         ultima_conexao_min = timeinfo.tm_hour;
//         ja_mandou = true;
//       }
//     }
//     else
//     {
//       ja_mandou = false;
//     }
//   }
//   else
//   {
//     digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
//     WiFiStarter.init_wifi();
//   }
// }
