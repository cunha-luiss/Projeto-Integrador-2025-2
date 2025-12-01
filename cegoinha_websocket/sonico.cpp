#include <Arduino.h>
#define TRIG_PIN 5
#define ECHO_PIN 4

const int NUM_LEITURAS = 10;   // leituras para média
const float DIST_ATIVAR = 3.0; // distância limite em cm

static float distancia = -1;

void setupSonico()
{
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(TRIG_PIN, LOW);
}

float lerDistancia()
{
    long duration;
    float distance;

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout de 30ms

    if (duration == 0)
        return -1; // falha na leitura

    distance = (duration * 0.0343) / 2.0;
    return distance;
}

void calcularDistanciaMedia()
{
    static unsigned long ultimaLeitura = 0;
    static int indice = 0;
    static float soma = 0;
    static int validas = 0;

    unsigned long agora = millis();

    // Executa leitura a cada 40ms
    if (agora - ultimaLeitura >= 40)
    {
        ultimaLeitura = agora;

        float d = lerDistancia();
        if (d > 0)
        {
            soma += d;
            validas++;
        }

        indice++;

        // Quando completar todas as leituras, processa
        if (indice >= NUM_LEITURAS)
        {
            distancia = (validas > 0) ? soma / validas : -1;

            // Reset para próxima rodada
            indice = 0;
            soma = 0;
            validas = 0;
        }
    }
}

int loopSonico()
{
    calcularDistanciaMedia();
    if (distancia > 0 && distancia <= DIST_ATIVAR)
    {
        Serial.println("Objeto detectado!");
        return 1;
    }
    return 0;
}