#ifndef DADOS_LITTLEFS_H 
#define DADOS_LITTLEFS_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "structures_projeto.h"

// Declaração (protótipo) da função
void setupLittleFS(std::vector<Rota> *protasArmazenadas);
void salvarRotasLittleFS();
void carregarRotasLittleFS();
void enviarRotasParaCliente(AsyncWebSocketClient *client);
void apagarRotas();
#endif