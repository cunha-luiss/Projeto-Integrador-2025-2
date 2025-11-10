#ifndef MOTORES_ANDAR_H  // Isso é um "Include Guard"
#define MOTORES_ANDAR_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "structures_projeto.h"

// Declaração (protótipo) da função
void proximoComando(Rota &novaRota);
void executarRota(Rota &novaRota);
void pararMotores();
void moverMotorDir(int direcao);
void moverMotorEsq(int direcao);
void motoresSetup(AsyncWebSocket *pws, int *pPASSO_ROTA, String *pmovimento, int64_t *pMETA_PULSOS, Rota *pROTA_ATUAL, volatile int64_t *ptotal_pulsos_esq, volatile int64_t *ptotal_pulsos_dir);
void configuraEncoderDireitoPCNT();
void configuraEncoderEsquerdoPCNT();
int passoRota();
#endif