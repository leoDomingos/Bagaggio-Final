#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "time.h"
#include <FS.h>

#include "LittleFS.h"

#define FORMAT_LITTLEFS_IF_FAILED true
#define JSON_SAVE_FILE "/saves.json"

/*
TODOS:
- Fazer o sistema reiniciar depois de 1 min sem wifi (done)
- codigo que alerta iminencia de overflow (nao vou fazer)
- wifimanager parando execuçao de codigo quando nao tem wifi (DONE)
- ajustar sensibilidade pelo portal (nao vou fazer)
- tirar mudanças do portal
- ajustar algoritmo de passagem

- Testar tudo
*/

#include "./Detector/detector.cpp"
#include "./bancoDeDados/bancoDeDados.cpp"
#include "./WiFiStarter/WiFiStarter.cpp"

unsigned long ultimo_portal_millis = 0;
unsigned long ultima_conexao_millis = 0;
int ultimo_save_contagem_min;
int ultima_mandada_min = 0;  // há quanto tempo mandamos ao db
int hora_colhida;     // Hora e minuto, de acordo com a última vez que atualizamos data&hora
int minuto_colhido;   //
int hora_atual;
int minuto_atual = 70;
int ajuste_hora = 0;
bool ajustou_hora = false;
unsigned long ultima_atualizacao_datahora_millis;
char* registro_loja = "apples";
struct tm timeinfo;
bool pegou_data = false;      // se em algum momento desde ser ligado ele conseguiu pegar a data&hora
int registrou_loja = 0;  // se ele já registrou a loja desde ser ligado
bool usou_backup = false;     // se o backup já foi usado
char data[21];                // data formatada
bool nao_salvou = true;
bool mudancou = false;

#define INTERVALO_GET_TIME_MIN 3        // A cada X min atualizaremos a data&hora
#define INTERVALO_REGISTROS_HORA 1
#define INTERVALO_SALVAR_CONTAGEM_MIN 2
#define TEMPO_PARA_USAR_BACKUP_MIN 60
#define DOC_SIZE 5000

// const char* ntpServer = "pool.ntp.org"; // Servidor que vai nos fornecer a data
// const long gmtOffset_sec = -3600 * 3; // Quantas horas estamos atrasados em relação a GMT
// const int daylightOffset_sec = 3600; // Offset do horário de verão

Detector detector;
Banco_de_Dados banco_de_dados;
WiFibro WiFiStarter;

void atualizar_e_formatar_data_antiga(int minuto, int hora);
void luzes_de_espera();

void setup() {
  delay(5000);
  Serial.begin(115200);          //Inicialização do monitor serial
  Serial.println("Começou!");
  configTime(-3600 * 4, 3600, "pool.ntp.org");
  pinMode(detector.led_alinhamento, OUTPUT);
  pinMode(banco_de_dados.led_banco_de_dados, OUTPUT);
  pinMode(detector.dir_receptor, OUTPUT);  //Definindo os pinos dos fotorreceptores
  pinMode(detector.esq_receptor, OUTPUT); //
  pinMode(detector.led_alinhamento, OUTPUT);
  pinMode(detector.receptor, INPUT);  //Definindo os pinos dos fotorreceptores

  pinMode(detector.led_on, OUTPUT);
  digitalWrite(detector.led_on, HIGH);
  WiFi.mode(WIFI_STA);

  WiFiStarter.init_wifi();
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  
  if(WiFi.status() == WL_CONNECTED)
  {

    if(getLocalTime(&timeinfo))
    {
      Serial.println("Data obtida (de primeira)");
      pegou_data = true;
      minuto_colhido = timeinfo.tm_min;
      hora_colhida = timeinfo.tm_hour;
      if (millis()/(60*1000) <= TEMPO_PARA_USAR_BACKUP_MIN) // Se tiver conexão em até X minutos após ligar, tentamos incluir o backup também
        {
          // readSaveCount() também precisa da hora atual para ver se o backup é válido (e não há mais de 1h)
          // por isso precisamos garantir que temos conexão, e que já pegamos a data&hora antes.
          int antes = detector.entraram;
          detector.entraram += banco_de_dados.readSaveCount(hora_colhida);
          Serial.print("Temos WiFi. Contagem de pessoas recuperada pelo backu: ");
          Serial.println(detector.entraram - antes);
          banco_de_dados.saveCount(0, hora_colhida); // Zerando o backup que tínhamos
        }
    }

    if(banco_de_dados.registrar_loja(registro_loja))
    {
      Serial.println("Registro de loja feito com êxito (de primeira)");
      registrou_loja = 10;
    }

  }

  if(!LittleFS.begin()){
      Serial.println("LittleFS Mount Failed");
      delay(1000);
  }
  //if /ssid.json doesn't exist, create it littlefs
  if(!LittleFS.exists(JSON_SAVE_FILE))
  {
    Serial.println("File doesn't exist, creating it");
    File credFile = LittleFS.open(JSON_SAVE_FILE, "w");
    if(!credFile){
        Serial.println("Failed to create file");
        delay(1000);
    }
    credFile.close();
  }
  // wifi_config_t conf;
  // registro_loja = banco_de_dados.getRegistroLoja((char*)conf.sta.ssid, (char*)conf.sta.password);
  // detector.readSens();
  // detector.printar_deteccao_max();
  // WiFiStarter.init_portal();
  // detector.saveSens();
  if (digitalRead(16) == LOW)
  {
    String sen_dir = "200";
    String sen_esq = "200";
    detector.atualizar_sens(sen_esq, sen_dir);
    detector.printar_deteccao_max();
  }
  else if (digitalRead(3) == LOW)
  {
    String sen_dir = "150";
    String sen_esq = "150";
    detector.atualizar_sens(sen_esq, sen_dir);
    detector.printar_deteccao_max();
  }
  else
  {
    String sen_dir = "100";
    String sen_esq = "100";
    detector.atualizar_sens(sen_esq, sen_dir);
    detector.printar_deteccao_max();
  }
}

void loop() 
{
  if (!pegou_data && millis()> 1000*60* 2) {ESP.restart();}
  if(!LittleFS.begin()) // Iniciando a memória
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }


  // while (WiFiStarter.inputDoUsuario() == "s") // Pausando o programa, quando o usuário quiser
  // {
  //   luzes_de_espera();
  // }

  detector.observar();
  digitalWrite(detector.led_alinhamento, HIGH);

  if(WiFi.status() == WL_CONNECTED)
  {
    ultima_conexao_millis = millis();
    if (minuto_atual != 70) {pegou_data = true;}
    if (!pegou_data) // garantindo que ele pegue a data&hora pela primeira vez (dada conexão com wifi)
    {
      Serial.println("Tentando pegar a data.");
      if(getLocalTime(&timeinfo))
      {
        Serial.println("Data obtida com êxito.");
        pegou_data = true;
        minuto_colhido = timeinfo.tm_min;
        hora_colhida = timeinfo.tm_hour;
        if (millis()/(60*1000) <= TEMPO_PARA_USAR_BACKUP_MIN) // Se tiver conexão em até X minutos após ligar, tentamos incluir o backup também
        {
          // readSaveCount() também precisa da hora atual para ver se o backup é válido (e não há mais de 1h)
          // por isso precisamos garantir que temos conexão, e que já pegamos a data&hora antes.
          int antes = detector.entraram;
          detector.entraram += banco_de_dados.readSaveCount(hora_atual);
          Serial.print("Temos WiFi. Contagem de pessoas recuperada pelo backup: ");
          Serial.println(detector.entraram - antes);
          banco_de_dados.saveCount(0, hora_atual); // Zerando o backup que tínhamos
        }
      }
      else
      {
        Serial.println("Data não obtida.");
      }
    }
    // if (registrou_loja == 0)
    // {
    //   if(banco_de_dados.registrar_loja(registro_loja))
    //     {
    //       Serial.println("Registro de loja feito com êxito.");
    //       registrou_loja = 10;
    //     }
    // }

    digitalWrite(banco_de_dados.led_banco_de_dados, HIGH);

    if (pegou_data)
    {
      int minutos_a_adicionar = int(((millis() - ultima_atualizacao_datahora_millis) / (1000*60)) % 60);
      int horas_a_adicionar   = int(((millis() - ultima_atualizacao_datahora_millis) / (1000*60*60)) % 24);
      atualizar_e_formatar_data_antiga(minuto_colhido+minutos_a_adicionar,hora_colhida+horas_a_adicionar);

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
  if (millis() - ultima_conexao_millis > 1000*60*2) {ESP.restart();}
  }

  if (pegou_data) // Se a data&hora está disponível
  {
    // Checamos se está na hora de salvar a contagem na memória
    if (minuto_atual % 2 != 0) {nao_salvou = true;}
    if (minuto_atual % 2 == 0 && nao_salvou && millis() >= 2*1000*60) // Intervalo de 2 em 2 min!
    {
      nao_salvou = false;
      // Só falta evitar overflow
      Serial.println("Tentando salvar tomada de dados na memória, para mandar depois.");
      delay(500);
      DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
      if (arquivo_dos_saves.memoryUsage() + DOC_SIZE*0.2 < DOC_SIZE)
      {
        if (arquivo_dos_saves.memoryUsage() + DOC_SIZE*0.1 > DOC_SIZE)
        {
          Serial.print("Overflow iminente! Espaço ocupado: ");
          Serial.println(arquivo_dos_saves.memoryUsage());
          // Acender algum led depois
        }
        char contagem_string[(((sizeof detector.entraram) * CHAR_BIT) + 2)/3 + 2];
        sprintf(contagem_string, "%d", detector.entraram);
        banco_de_dados.addCredentials(registro_loja, data, contagem_string);
        detector.entraram = 0;
        Serial.println("Salvamento concluído.");
      }
      else
      {
        Serial.println("Alerta de overflow!! Não vamos mais adicionar dados.");
      }
    }
  }

  if(WiFi.status()== WL_CONNECTED && minuto_atual % 2 == 0) // Se temos conexão, madamos para o banco de dados as coisas da memória
  {
    DynamicJsonDocument arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
    bool abortar_mandar_ao_db = false;
    ultima_mandada_min = minuto_atual;
    abortar_mandar_ao_db = false;
    if (WiFi.status() == WL_CONNECTED && !arquivo_dos_saves.isNull())
    {
      arquivo_dos_saves = banco_de_dados.readFile(LittleFS, JSON_SAVE_FILE);
      String pessoas_a_mandar;
      String registro_loja_a_mandar;
      String data_a_mandar;
      String teste;
      banco_de_dados.grabFirst(&registro_loja_a_mandar, &data_a_mandar, &pessoas_a_mandar);
      if (pessoas_a_mandar != teste)
      {
        Serial.println("Mandando leituras para o banco de dados.");
        Serial.println("Mandando a tomada de dados:");
        Serial.print("codigo_loja=");
        Serial.println(registro_loja_a_mandar);
        Serial.print("data=");
        Serial.println(data_a_mandar);
        Serial.print("contagem=");
        Serial.println(pessoas_a_mandar);
        if (banco_de_dados.registrar_leituras(pessoas_a_mandar, data_a_mandar, registro_loja_a_mandar)) 
        {
          Serial.println("Tomada de dados enviada com sucesso. Removendo-a da memória...");
          arquivo_dos_saves.remove(0);
          arquivo_dos_saves.garbageCollect();
          banco_de_dados.writeFile(LittleFS, JSON_SAVE_FILE, arquivo_dos_saves);
          Serial.print("Novo espaço ocupado na memória: ");
          Serial.println(arquivo_dos_saves.memoryUsage());
        }
      }
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