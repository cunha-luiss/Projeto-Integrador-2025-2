#ifndef BATERIA_H
#define BATERIA_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "structures_projeto.h"
#include "motores_andar.h"
#include "dados_littlefs.h"

// Declaração (protótipo) da função
void setupBateria(AsyncWebSocket *pws);
float lerTensaoBateria();
float calcularSOC(float voltage);
void atualizarBateria();
void enviarDadosBateria();
void verificarBateriaCritica();
#endif