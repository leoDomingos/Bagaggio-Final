#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
#include <SPIFFS.h>
#include <FS.h>
#include "LittleFS.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#define JSON_SAVE_FILE "/saves.json"

/*
TODOS:
- botar sistema novo de wifi (DONE)
- ver que backup depende do wifi (DONE)
- ver que registro depende do wifi ()
- ver que mandar pro db/memoria depende do wifi (DONE)
- codigo que evita overflow (DONE)
- codigo que alerta iminencia de overflow
- mudar armazenamento de dados para cada 15 min (5 para testes)
- fazer com que configtime() só precise ser chamado uma vez!!

- Testar tudo
*/

#include "./Detector/detector.cpp"
#include "./bancoDeDados/bancoDeDados.cpp"
#include "./WiFiStarter/WiFiStarter.cpp"

int ultima_conexao_hora;
unsigned long ultima_conexao_millis;
int ultimo_save_contagem_min;
int ultimo_get_time_min = 0;  // há quanto tempo atualizamos a data&hora
int hora;     // Hora e minuto, de acordo com a última vez que atualizamos data&hora
int minuto;   //
unsigned long ultima_atualizacao_datahora_millis;
String registro_loja = "defin5";
struct tm timeinfo;
bool pegou_data = false;      // se em algum momento desde ser ligado ele conseguiu pegar a data&hora
bool registrou_loja = false;  // se ele já registrou a loja desde ser ligado
bool usou_backup = false;     // se o backup já foi usado
char data[21];                // data formatada

#define INTERVALO_GET_TIME_MIN 3        // A cada X min atualizaremos a data&hora
#define INTERVALO_REGISTROS_HORA 1
#define INTERVALO_SALVAR_CONTAGEM_MIN 2
#define TEMPO_PARA_USAR_BACKUP_MIN 10
#define DOC_SIZE 5000

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
  Serial.begin(115200);
  WiFiStarter.init_wifi();
  
  if(WiFi.status() == WL_CONNECTED)
  {
  getLocalTime(&timeinfo);
  pegou_data = true;
  banco_de_dados.registrar_loja(registro_loja);
  registrou_loja = true;
  ultimo_save_contagem_min = int(timeinfo.tm_min);
  ultima_conexao_hora = int(timeinfo.tm_hour);
  }

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
      Serial.println("LittleFS Mount Failed");
      return;
  }
  //if /ssid.json doesn't exist, create it littlefs
  if(!LittleFS.exists(JSON_SAVE_FILE))
  {
    Serial.println("File doesn't exist, creating it");
    File credFile = LittleFS.open(JSON_SAVE_FILE, FILE_WRITE);
    if(!credFile){
        Serial.println("Failed to create file");
        return;
    }
    credFile.close();
  }
  // wifi_config_t conf;
  // registro_loja = banco_de_dados.getRegistroLoja((char*)conf.sta.ssid, (char*)conf.sta.password);
  
}

void loop() 
{
  digitalWrite(detector.led_on, HIGH);
  
  WiFiStarter.init_portal();
  WiFiStarter.processar_pagina_html();
  
  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) // Iniciando a memória
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  while (WiFiStarter.inputDoUsuario() == "s") // Pausando o programa, quando o usuário quiser
  {
    luzes_de_espera();
  }

  detector.observar();
  digitalWrite(detector.led_alinhamento, HIGH);
  if(WiFi.status() == WL_CONNECTED)
  {
    if (!pegou_data) // garantindo que ele pegue a data&hora pela primeira vez (dada conexão com wifi)
    {
      getLocalTime(&timeinfo);
      pegou_data = true;
    }
    digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);
    if (pegou_data)
    {
      if (abs(timeinfo.tm_min - ultimo_get_time_min) >= INTERVALO_GET_TIME_MIN) // Pegando nova data após X seg
      {
        Serial.println("Pegando nova data");
        getLocalTime(&timeinfo);
        ultimo_get_time_min = timeinfo.tm_min;
        ultima_atualizacao_datahora_millis = millis();
        hora = timeinfo.tm_hour;
        minuto = timeinfo.tm_min;
        formatar_data_recente();
      }
      
      if (millis() <= TEMPO_PARA_USAR_BACKUP_MIN * 1000 * 60 && !usou_backup) // Se tiver conexão em até X minutos após ligar, tentamos incluir o backup também
      {
        // readSaveCount() também precisa da hora atual para ver se o backup é válido (e não da hora passada)
        // por isso precisamos garantir que temos conexão, e que já pegamos a data&hora antes.
        int antes = detector.entraram;
        detector.entraram += banco_de_dados.readSaveCount(int(timeinfo.tm_hour));
        Serial.print("Contagem de pessoas recuperada: ");
        Serial.println(detector.entraram - antes);
        banco_de_dados.saveCount(0, timeinfo.tm_hour); // Zerando o backup que tínhamos
        usou_backup = true; // Não precisaremos mais do backup, já o usamos
        delay(1000);
      }
      
      if (abs(timeinfo.tm_min - ultimo_save_contagem_min) >= INTERVALO_SALVAR_CONTAGEM_MIN) // Salvando contagem na memória após X min
      {
        Serial.println("Salvando contagem na memória.");
        banco_de_dados.saveCount(detector.entraram, timeinfo.tm_hour);
        ultimo_save_contagem_min = timeinfo.tm_min;
        delay(500);
      }

    }
  }
  else // Avisando que está sem conexão
  {
  digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
  }
  if (abs(ultimo_get_time_min - INTERVALO_GET_TIME_MIN) <= INTERVALO_GET_TIME_MIN * 2 && pegou_data) // Se a data&hora está relativamente atualizada
  {
    // Checamos se está na hora de salvar a contagem na memória
    if (abs(timeinfo.tm_hour - ultima_conexao_hora) >= INTERVALO_REGISTROS_HORA || (millis() > INTERVALO_REGISTROS_HORA * 60 * 60 * 1000 && millis() - ultima_conexao_millis >= INTERVALO_REGISTROS_HORA * 60 * 60 * 1000)) // Mandando leituras para a memória após X horas
    {
      // Se sim, a guardaremos.
      if(WiFi.status() == WL_CONNECTED) {getLocalTime(&timeinfo);formatar_data_recente();} // Mas antes, vamos tentar ter a data&hora mais atualizada possível
      // Só falta evitar overflow
      DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
      if (arquivo_dos_saves.memoryUsage() + DOC_SIZE*0.2 < DOC_SIZE)
      {
        if (arquivo_dos_saves.memoryUsage() + DOC_SIZE*0.1 < DOC_SIZE); // Acender algum led depois
        String contagem_string = String(detector.entraram);
        banco_de_dados.addCredentials(registro_loja.c_str(), data, contagem_string.c_str());
        detector.entraram = 0;
      }
      else
      {
        Serial.println("Alerta de overflow!! Não vamos mais adicionar dados.");
      }
    }
  }
  
  else if (pegou_data)// Se faz tempo que temos conexão com wifi, tentaremos guardar os dados mesmo assim. Contudo, precisamos ter pego, em algum momento, a data.
  {
    int minutos_a_adicionar = int(((millis() - ultima_atualizacao_datahora_millis) / (1000*60)) % 60);
    int horas_a_adicionar   = int(((millis() - ultima_atualizacao_datahora_millis) / (1000*60*60)) % 24);
    hora += horas_a_adicionar;
    minuto += minutos_a_adicionar;
    atualizar_e_formatar_data_antiga(minuto, hora);
    DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
    if (arquivo_dos_saves.memoryUsage() + 200 < DOC_SIZE)
    {
      String contagem_string = String(detector.entraram);
      banco_de_dados.addCredentials(registro_loja.c_str(), data, contagem_string.c_str());
      detector.entraram = 0;
    }
    else
    {
      Serial.println("Alerta de overflow!! Não vamos mais adicionar dados.");
    }
  }

  if(WiFi.status()== WL_CONNECTED) // Se temos conexão, madamos para o banco de dados as coisas da memória
  {
    Serial.println("Mandando leituras para o banco de dados.");
    DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
    while (!arquivo_dos_saves.isNull() && WiFi.status()== WL_CONNECTED)
    {
      arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
      int pessoas_a_mandar;
      String registro_loja_a_mandar;
      String data_a_mandar;
      banco_de_dados.grabFirst(&registro_loja, &data_a_mandar, &registro_loja_a_mandar);
      int sensor_leituras_sucesso = banco_de_dados.registrar_leituras(pessoas_a_mandar, data_a_mandar, registro_loja_a_mandar);
      arquivo_dos_saves.remove(0);
      arquivo_dos_saves.garbageCollect();
      banco_de_dados.writeFile(LittleFS, JSON_SAVE_FILE, arquivo_dos_saves);
    }
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

void formatar_data_recente()
{
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  char timeMonth[10];
  strftime(timeMonth,10, "%B", &timeinfo);
  char timeDay[3];
  strftime(timeDay,3, "%d", &timeinfo);
  char timeYear[5];
  strftime(timeYear,5, "%Y", &timeinfo);
  // Atualizando a variável data
  strcat(data, timeDay);
  strcat(data,"/");
  strcat(data,timeMonth);
  strcat(data,"/");
  strcat(data,timeYear);
  strcat(data," ");
  strcat(data,timeHour);
  String minuto_str = ":" + String(timeinfo.tm_min);
  strcat(data,minuto_str.c_str());
}

void atualizar_e_formatar_data_antiga(int minuto, int hora)
{
  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  char timeMonth[10];
  strftime(timeMonth,10, "%B", &timeinfo);
  char timeDay[3];
  strftime(timeDay,3, "%d", &timeinfo);
  char timeYear[5];
  strftime(timeYear,5, "%Y", &timeinfo);
  // Atualizando a variável data
  strcat(data, timeDay);
  strcat(data,"/");
  strcat(data,timeMonth);
  strcat(data,"/");
  strcat(data,timeYear);
  strcat(data," ");
  String hora_str = String(hora);
  strcat(data, hora_str.c_str());
  String minuto_str = ":" + String(minuto);
  strcat(data,minuto_str.c_str());
}