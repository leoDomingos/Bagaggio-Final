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
int hora_colhida;     // Hora e minuto, de acordo com a última vez que atualizamos data&hora
int minuto_colhido;   //
int hora_atual;
int minuto_atual;
int ajuste_hora = 0;
bool ajustou_hora = false;
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
#define TEMPO_PARA_USAR_BACKUP_MIN 60
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

    if(getLocalTime(&timeinfo))
    {
      Serial.println("Data obtida (de primeira)");
      pegou_data = true;
      minuto_colhido = timeinfo.tm_min;
      hora_colhida = timeinfo.tm_hour;
    }

    if(banco_de_dados.registrar_loja(registro_loja))
    {
      Serial.println("Registro de loja feito com êxito (de primeira)");
      registrou_loja = true;
    }

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
      if(getLocalTime(&timeinfo))
      {
        Serial.println("Data obtida com êxito.");
        pegou_data = true;
        minuto_colhido = timeinfo.tm_min;
        hora_colhida = timeinfo.tm_hour;
      }
    }
    if (!registrou_loja)
    {
      if(banco_de_dados.registrar_loja(registro_loja))
        {
          Serial.println("Registro de loja feito com êxito.");
          registrou_loja = true;
        }
    }

    digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);
    
    if (pegou_data)
    {
      int minutos_a_adicionar = int(((millis() - ultima_atualizacao_datahora_millis) / (1000*60)) % 60);
      int horas_a_adicionar   = int(((millis() - ultima_atualizacao_datahora_millis) / (1000*60*60)) % 24);
      atualizar_e_formatar_data_antiga(minuto_colhido+minutos_a_adicionar,hora_colhida+horas_a_adicionar);
      if (millis()/(60*1000) <= TEMPO_PARA_USAR_BACKUP_MIN && !usou_backup) // Se tiver conexão em até X minutos após ligar, tentamos incluir o backup também
      {
        // readSaveCount() também precisa da hora atual para ver se o backup é válido (e não há mais de 1h)
        // por isso precisamos garantir que temos conexão, e que já pegamos a data&hora antes.
        int antes = detector.entraram;
        detector.entraram += banco_de_dados.readSaveCount(hora_atual);
        Serial.print("Temos WiFi. Contagem de pessoas recuperada pelo backup: ");
        Serial.println(detector.entraram - antes);
        banco_de_dados.saveCount(0, hora_atual); // Zerando o backup que tínhamos
        usou_backup = true; // Não precisaremos mais do backup, já o usamos
      }
      
      if (abs(minuto_atual - ultimo_save_contagem_min) >= INTERVALO_SALVAR_CONTAGEM_MIN) // Salvando contagem na memória após X min
      {
        Serial.println("Salvando contagem na memória.");
        banco_de_dados.saveCount(detector.entraram, hora_atual);
        ultimo_save_contagem_min = minuto_atual;
      }

    }
  }
  else // Avisando que está sem conexão
  {
  digitalWrite(banco_de_dados.led_banco_de_dados, LOW);
  }
  if (pegou_data) // Se a data&hora está disponível
  {
    // Checamos se está na hora de salvar a contagem na memória
    if (abs(minuto_colhido-minuto_atual) == 5) // Intervalo de 5 em 5 min!
    {
      // Só falta evitar overflow
      Serial.println("Tentando salvar tomada de dados na memória, para mandar depois.");
      DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
      if (arquivo_dos_saves.memoryUsage() + DOC_SIZE*0.2 < DOC_SIZE)
      {
        if (arquivo_dos_saves.memoryUsage() + DOC_SIZE*0.1 < DOC_SIZE)
        {
          Serial.print("Overflow iminente! Espaço ocupado: ");
          Serial.println(arquivo_dos_saves.memoryUsage());
          // Acender algum led depois
        }
        String contagem_string = String(detector.entraram);
        banco_de_dados.addCredentials(registro_loja.c_str(), data, contagem_string.c_str());
        detector.entraram = 0;
        Serial.println("Salvamento concluído.");
      }
      else
      {
        Serial.println("Alerta de overflow!! Não vamos mais adicionar dados.");
      }
    }
  }

  if(WiFi.status()== WL_CONNECTED) // Se temos conexão, madamos para o banco de dados as coisas da memória
  {
    Serial.println("Mandando leituras para o banco de dados.");
    DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
    bool abortar_mandar_ao_db = false;
    while (!arquivo_dos_saves.isNull() && WiFi.status() == WL_CONNECTED && !abortar_mandar_ao_db)
    {
      arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
      String pessoas_a_mandar;
      String registro_loja_a_mandar;
      String data_a_mandar;
      banco_de_dados.grabFirst(&registro_loja_a_mandar, &data_a_mandar, &pessoas_a_mandar);
      Serial.println("Mandando a tomada de dados:");
      Serial.print("codigo_loja=");
      Serial.println(registro_loja_a_mandar);
      Serial.print("data=");
      Serial.println(data_a_mandar);
      Serial.print("contagem=");
      Serial.println(pessoas_a_mandar);
      ulong inicio_da_tentativa_de_mandar = millis();
      while (!banco_de_dados.registrar_leituras(pessoas_a_mandar, data_a_mandar, registro_loja_a_mandar) && WiFi.status() == WL_CONNECTED) 
      {
        delay(100);
        if ((millis()-inicio_da_tentativa_de_mandar) > 3000)
        {
          abortar_mandar_ao_db = true;
          Serial.println("Demorou demais para mandar ao db. Abortando.");
          break;
        }
      }
      if (!abortar_mandar_ao_db) // Se realmente mandamos, vamos atualizar a memória
      {
        Serial.println("Tomada de dados enviada com sucesso. Removendo-a da memória...");
        arquivo_dos_saves.remove(0);
        arquivo_dos_saves.garbageCollect();
        banco_de_dados.writeFile(LittleFS, JSON_SAVE_FILE, arquivo_dos_saves);
        Serial.print("Novo espaço ocupado na memória: ");
        Serial.println(arquivo_dos_saves.memoryUsage());
      }
      abortar_mandar_ao_db = false;
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

void atualizar_e_formatar_data_antiga(int minuto, int hora)
{
  if ((minuto)%60!=0) {ajustou_hora = false;}
  if ((minuto)%60==0 && !ajustou_hora)
  {
    ajuste_hora += 1;
    ajustou_hora = true;
  }
  minuto = minuto - 60*ajuste_hora;
  hora = hora + ajuste_hora;
  minuto_atual = minuto;
  hora_atual = hora;
  memset(data, 0, sizeof(data));
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
  String hora_str;
  if (hora < 10)
  {
    hora_str = String("0") + String(hora);
  }
  else
  {
    hora_str = String(hora);
  }
  strcat(data, hora_str.c_str());
  
  String minuto_str;
  if (minuto < 10)
  {
    minuto_str = String("0") + String(minuto);
  }
  else
  {
    minuto_str = String(minuto);
  }
  minuto_str = ":" + minuto_str;
  strcat(data,minuto_str.c_str());
}