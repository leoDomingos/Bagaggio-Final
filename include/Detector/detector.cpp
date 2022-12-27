#ifndef DETECTOR
#define DETECTOR
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "time.h"
class Detector
{
    public:
        static void printar_leituras();
        static bool is_obstruido(int leitura, int limite);
        static void printar_obs();
        static bool is_obs_valid(ulong obs);
        static void esperar_alinhamento();
        void observar();
        static inline int esq_receptor = 35;
        static inline int dir_receptor = 34;

        static inline int led_on = 21;
        static inline int led_alinhamento = 22;

        static inline int entraram = 0;                   // Quantidade de pessoas que...
        static inline int sairam = 0;

        static inline int deteccao_maxima = 500;         // Valor máximo de leitura dos fotorreceptores antes de ser considerado como obstruído.
        static inline int tempo_maximo = 2000;            // Tempo máximo que uma obstrução na esquerda seguida de uma na direita vai contar como alguém passando.
        static inline int tempo_limite_alinhamento = 20 * 1000; // Limite de tempo antes de os laser serem considerados fora de alinhamento.
        static inline bool is_obstruido_esq = false;      // Essas variáveis vão dizer se o sensor está obstruído ou não.
        static inline bool is_obstruido_dir = false;     //

        static inline long tempo_obstrucao_esq = tempo_maximo + 1;     // Há quanto tempo o sensor foi obstruído
        static inline long tempo_obstrucao_dir = tempo_maximo + 1;
        static inline long tempo_laser_esq = tempo_limite_alinhamento + 1;     // Há quanto tempo o sensor viu o laser
        static inline long tempo_laser_dir = tempo_limite_alinhamento + 1;
        static inline unsigned long ultima_contagem = tempo_maximo + 1;               // Quando foi a última contagem

        static inline bool is_dir_obs_valid = false;                            // Se a última contagem é válida
        static inline bool is_esq_obs_valid = false;                           //
        static inline bool contou = false;
    private:
};

void Detector::observar()
{
    Detector::printar_leituras();
    int leitura_esquerda = analogRead(Detector::esq_receptor);
    int leitura_direita = analogRead(Detector::dir_receptor);
    Detector::is_obstruido_esq = Detector::is_obstruido(leitura_esquerda, Detector::deteccao_maxima);
    Detector::is_obstruido_dir = Detector::is_obstruido(leitura_direita, Detector::deteccao_maxima);
    if (Detector::is_obstruido_esq) // Se está obstruído
    {
        //Serial.println("esq obs");
        Detector::tempo_obstrucao_esq = millis(); // Registramos quando isso ocorreu
    }
    else
    {
        Detector::tempo_laser_esq = millis(); // Se não, registramos também
    }
    if (Detector::is_obstruido_dir)
    {
        //Serial.println("dir obs");
        Detector::tempo_obstrucao_dir = millis();
    }
    else
    {
        Detector::tempo_laser_esq = millis();
    }
    if (millis() - Detector::tempo_laser_esq > Detector::tempo_limite_alinhamento || millis() - Detector::tempo_laser_dir > Detector::tempo_limite_alinhamento) // Se a última vez que o laser foi visto foi há tanto tempo, provavelmente está desalinhado
    {
        Detector::esperar_alinhamento(); // Vamos esperar alinhamento
        int leitura_esquerda = analogRead(Detector::esq_receptor); // Depois do alinhamento realizado, vamos pegar leituras novas
        int leitura_direita = analogRead(Detector::dir_receptor);
        Detector::is_obstruido_esq = Detector::is_obstruido(leitura_esquerda, Detector::deteccao_maxima);
        Detector::is_obstruido_dir = Detector::is_obstruido(leitura_direita, Detector::deteccao_maxima);
    }
    Detector::is_dir_obs_valid = is_obs_valid(Detector::tempo_obstrucao_dir); // Vendo se a obstrução detectada foi há mais de x segundos
    Detector::is_esq_obs_valid = is_obs_valid(Detector::tempo_obstrucao_esq);


    if (Detector::is_dir_obs_valid && Detector::is_esq_obs_valid && millis() > Detector::tempo_maximo*2 && !Detector::contou && !(Detector::is_obstruido_esq && Detector::is_obstruido_dir))
    {
        if (Detector::tempo_obstrucao_dir > Detector::tempo_obstrucao_esq)
        {
        Serial.println("entrou");
        Detector::entraram += 1;
        Detector::contou = true;
        delay(300);
        }
        if (Detector::tempo_obstrucao_esq > Detector::tempo_obstrucao_dir)
        {
        Serial.println("saiu");
        Detector::sairam += 1;
        Detector::contou = true;
        delay(300);
        }
    }
    if ((Detector::contou)) {
        Detector::contou = false;
        Detector::tempo_obstrucao_dir = millis() - (Detector::tempo_maximo + 500);
        Detector::tempo_obstrucao_esq = millis() - (Detector::tempo_maximo + 500);
    }
}

void Detector::printar_leituras()
{
  // Printa as leituras atuais de cada sensor.

  Serial.print("Sensor esquerda:");
  Serial.println(analogRead(Detector::esq_receptor));
  Serial.println(" ");
  Serial.print("Sensor direita:");
  Serial.println(analogRead(Detector::dir_receptor));
}

bool Detector::is_obstruido(int leitura, int limite)
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

void Detector::printar_obs()
{
  Serial.print("Última esq obs há: ");
  Serial.println(millis()-tempo_obstrucao_esq);
  Serial.print("Última dir obs há: ");
  Serial.println(millis()-tempo_obstrucao_dir);
}

bool Detector::is_obs_valid(ulong obs)
{
  if (millis() - obs < Detector::tempo_maximo)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void Detector::esperar_alinhamento()
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

#endif