#ifndef SONICO_H  // Isso é um "Include Guard"
#define SONICO_H
#include <ESP32Servo.h>

void setupSonico();
float lerDistancia();
void calcularDistanciaMedia();
int loopSonico();

#endif