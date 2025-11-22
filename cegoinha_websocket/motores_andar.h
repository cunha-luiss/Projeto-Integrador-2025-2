#ifndef MOTORES_ANDAR_H  // Isso é um "Include Guard"
#define MOTORES_ANDAR_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PID_v1_bc.h>
#include "structures_projeto.h"

// Declaração (protótipo) da função
void proximoComando(Rota &novaRota);
void executarRota(Rota &novaRota);
void pararMotores();
void moverMotorDir(int direcao);
void moverMotorEsq(int direcao);
void motoresSetup(AsyncWebSocket *pws, int *pPASSO_ROTA, String *pmovimento, int64_t *pMETA_PULSOS, Rota *pROTA_ATUAL, volatile int64_t *ptotal_pulsos_esq, volatile int64_t *ptotal_pulsos_dir, const int *psampleTimePID);
void configuraEncoderDireitoPCNT();
void configuraEncoderEsquerdoPCNT();
void calcularPID();
void IRAM_ATTR readEncoderA();
void IRAM_ATTR readEncoderB();
void moverMotorB(int pwmVal);
void moverMotorA(int pwmVal);
float calcularVelocidadeInstantanea();
#endif