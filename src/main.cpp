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



int esq_receptor = 35;
int dir_receptor = 34;

int entrou_led = 21;
int saiu_led = 16;
int led_alinhamento = 22;
int led_banco_de_dados = 23;

int entraram = 0;                   // Quantidade de pessoas que...
int sairam = 0;

int deteccao_maxima = 500;         // Valor máximo de leitura dos fotorreceptores antes de ser considerado como obstruído.
int tempo_maximo = 2000;            // Tempo máximo que uma obstrução na esquerda seguida de uma na direita vai contar como alguém passando.
int tempo_limite_alinhamento = 20 * 1000; // Limite de tempo antes de os laser serem considerados fora de alinhamento.
bool is_obstruido_esq = false;      // Essas variáveis vão dizer se o sensor está obstruído ou não.
bool is_obstruido_dir = false;     //

long tempo_obstrucao_esq = tempo_maximo + 1;     // Há quanto tempo o sensor foi obstruído
long tempo_obstrucao_dir = tempo_maximo + 1;
long tempo_laser_esq = tempo_limite_alinhamento + 1;     // Há quanto tempo o sensor viu o laser
long tempo_laser_dir = tempo_limite_alinhamento + 1;
ulong ultima_contagem = tempo_maximo + 1;               // Quando foi a última contagem

bool is_dir_obs_valid = false;                            // Se a última contagem é válida
bool is_esq_obs_valid = false;                           //
bool contou = false;

int display[8] = {33, 32, 18, 5, 17, 25, 26, 19};

const char* ssid     = "Fluxo";    // Nome do Wifi
const char* password = "foconocliente";   // Senha

const char* serverName = "https://database-bagaggio-flask.herokuapp.com/registro";

int ultima_conexao = 0;


void printar_leituras();
bool is_obstruido(int leitura, int limite);
void printar_obs();
bool is_obs_valid(ulong obs);
void printar_display(int numero);
void escrever_segmento(int display[8], int valor[8]);
void esperar_alinhamento();
String to_json(int pessoas, String data);




void setup() {
  Serial.begin(115200);          //Inicialização do monitor serial
  pinMode(esq_receptor, INPUT);  //Definindo os pinos dos fotorreceptores
  pinMode(dir_receptor, INPUT); //
  pinMode(entrou_led, OUTPUT);   // Definindo os pinos dos leds adicionais
  pinMode(saiu_led, OUTPUT);    //
  pinMode(led_alinhamento, OUTPUT);
  pinMode(led_banco_de_dados, OUTPUT);
  for (int i = 0; i < 8; i++)
  {
    pinMode(display[i], OUTPUT);
  }
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
  digitalWrite(entrou_led, LOW);
  digitalWrite(saiu_led, LOW);
  printar_leituras();
  int leitura_esquerda = analogRead(esq_receptor);
  int leitura_direita = analogRead(dir_receptor);
  is_obstruido_esq = is_obstruido(leitura_esquerda, deteccao_maxima);
  is_obstruido_dir = is_obstruido(leitura_direita, deteccao_maxima);
  if (is_obstruido_esq) // Se está obstruído
  {
    //Serial.println("esq obs");
    tempo_obstrucao_esq = millis(); // Registramos quando isso ocorreu
  }
  else
  {
    tempo_laser_esq = millis(); // Se não, registramos também
  }
  if (is_obstruido_dir)
  {
    //Serial.println("dir obs");
    tempo_obstrucao_dir = millis();
  }
  else
  {
    tempo_laser_esq = millis();
  }
  if (millis() - tempo_laser_esq > tempo_limite_alinhamento || millis() - tempo_laser_dir > tempo_limite_alinhamento) // Se a última vez que o laser foi visto foi há tanto tempo, provavelmente está desalinhado
  {
    esperar_alinhamento(); // Vamos esperar alinhamento
    int leitura_esquerda = analogRead(esq_receptor); // Depois do alinhamento realizado, vamos pegar leituras novas
    int leitura_direita = analogRead(dir_receptor);
    is_obstruido_esq = is_obstruido(leitura_esquerda, deteccao_maxima);
    is_obstruido_dir = is_obstruido(leitura_direita, deteccao_maxima);
  }
  is_dir_obs_valid = is_obs_valid(tempo_obstrucao_dir); // Vendo se a obstrução detectada foi há mais de x segundos
  is_esq_obs_valid = is_obs_valid(tempo_obstrucao_esq);
  
  
  if (is_dir_obs_valid && is_esq_obs_valid && millis() > tempo_maximo*2 && !contou && !(is_obstruido_esq && is_obstruido_dir))
  {
    if (tempo_obstrucao_dir > tempo_obstrucao_esq)
    {
      digitalWrite(entrou_led, HIGH);
      Serial.println("entrou");
      entraram += 1;
      contou = true;
      delay(300);
    }
    if (tempo_obstrucao_esq > tempo_obstrucao_dir)
    {
      digitalWrite(saiu_led, HIGH);
      Serial.println("saiu");
      sairam += 1;
      contou = true;
      delay(300);
    }
  }
  if ((contou)) {
    contou = false;
    tempo_obstrucao_dir = millis() - (tempo_maximo + 500);
    tempo_obstrucao_esq = millis() - (tempo_maximo + 500);
  }
  printar_display(entraram);
  
  if(WiFi.status()== WL_CONNECTED){
    digitalWrite(led_banco_de_dados, HIGH);
    WiFiClient client;
    HTTPClient http;

    // Your Domain name with URL path or IP address with path
    http.begin(serverName);
    
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
    int httpResponseCode = http.POST(String(to_json(entraram, data)));
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

void printar_leituras()
{
  // Printa as leituras atuais de cada sensor.

  Serial.print("Sensor esquerda:");
  Serial.println(analogRead(esq_receptor));
  Serial.println(" ");
  Serial.print("Sensor direita:");
  Serial.println(analogRead(dir_receptor));
}

bool is_obstruido(int leitura, int limite)
{
  // Detecta se o sensor está obstruído, comparando a média das suas leituras com
  // o que ele está recebendo agora.

  if (limite < leitura)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void printar_obs()
{
  Serial.print("Última esq obs há: ");
  Serial.println(millis()-tempo_obstrucao_esq);
  Serial.print("Última dir obs há: ");
  Serial.println(millis()-tempo_obstrucao_dir);
}

bool is_obs_valid(ulong obs)
{
  if (millis() - obs < tempo_maximo)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void escrever_segmento(int display[8], int valor[8])
{
  for (int i = 0; i < 8; i++)
  {
    digitalWrite(display[i], valor[i]);
  }
}

void printar_display(int numero)
{
  if (numero == 0)
  {
    int valor[8] = {1,1,1,1,1,1,0,0};
    escrever_segmento(display, valor);
  }
  if (numero == 1)
  {
    int valor[8] = {0,1,1,0,0,0,0,0};
    escrever_segmento(display, valor);
  }
  if (numero == 2)
  {
    int valor[8] = {1,1,0,1,1,0,1,0};
    escrever_segmento(display, valor);
  }
  if (numero == 3)
  {
    int valor[8] = {1,1,1,1,0,0,1,0};
    escrever_segmento(display, valor);
  }
  if (numero == 4)
  {
    int valor[8] = {0,1,1,0,0,1,1,0};
    escrever_segmento(display, valor);
  }
  if (numero == 5)
  {
    int valor[8] = {1,0,1,1,0,1,1,0};
    escrever_segmento(display, valor);
  }
  if (numero == 6)
  {
    int valor[8] = {1,0,1,1,1,1,1,0};
    escrever_segmento(display, valor);
  }
  if (numero == 7)
  {
    int valor[8] = {1,1,1,0,0,0,0,0};
    escrever_segmento(display, valor);
  }
  if (numero == 8)
  {
    int valor[8] = {1,1,1,1,1,1,1,0};
    escrever_segmento(display, valor);
  }
  if (numero == 9)
  {
    int valor[8] = {1,1,1,1,0,1,1,0};
    escrever_segmento(display, valor);
  }
  if (numero > 9)
  {
    int valor[8] = {1,1,1,1,0,1,1,1};
    escrever_segmento(display, valor);
  }
}

void esperar_alinhamento()
{
  bool alinhado = false;
  int leitura_esquerda = analogRead(esq_receptor);
  int leitura_direita = analogRead(dir_receptor);
  is_obstruido_esq = is_obstruido(leitura_esquerda, deteccao_maxima);
  is_obstruido_dir = is_obstruido(leitura_direita, deteccao_maxima);
  while (!alinhado)
  {
    int leitura_esquerda = analogRead(esq_receptor);
    int leitura_direita = analogRead(dir_receptor);
    is_obstruido_esq = is_obstruido(leitura_esquerda, deteccao_maxima);
    is_obstruido_dir = is_obstruido(leitura_direita, deteccao_maxima);
    digitalWrite(led_alinhamento, HIGH);
    if (is_obstruido_esq) // Se está obstruído
    {
      tempo_obstrucao_esq = millis(); // Registramos quando isso ocorreu
    }
    if (is_obstruido_dir)
    {
      tempo_obstrucao_dir = millis();
    }
    is_dir_obs_valid = is_obs_valid(tempo_obstrucao_dir); // Vendo se a obstrução detectada foi há mais de x segundos
    is_esq_obs_valid = is_obs_valid(tempo_obstrucao_esq);
    if (!is_dir_obs_valid && !is_esq_obs_valid) {alinhado = true;}
  }
  digitalWrite(led_alinhamento, LOW);
  tempo_laser_esq = millis();
  tempo_laser_dir = millis();
}

String to_json(int pessoas, String data)
{
  // "{\"numero_pessoas\":\"pessoas\",\"datetime\":\"06/11/2022\"}"
  String json = String('{') + String('"') + String("numero_pessoas") + String('"') + String(':') + String('"') + String(pessoas) + String('"') + String(",") + String('"') + String("datetime") + String('"') + String(':') + String('"') + String(data) + String('"') + String("}");
  return json;
}