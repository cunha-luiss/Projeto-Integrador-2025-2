#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "structures_projeto.h"

#define ROTAS_FILE "/rotas.json"

static std::vector<Rota> *rotasArmazenadas = nullptr;

void setupLittleFS(std::vector<Rota> *protasArmazenadas){
    rotasArmazenadas = protasArmazenadas;
}

void salvarRotasLittleFS() {
  DynamicJsonDocument doc(8192);
  JsonArray rotasArray = doc.createNestedArray("rotas");

  for (const auto &rota : *rotasArmazenadas) {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;
    rotaObj["deviceId"] = rota.deviceId;

    JsonArray comandosArray = rotaObj.createNestedArray("comandos");
    for (const auto &cmd : rota.comandos) {
      JsonObject cmdObj = comandosArray.createNestedObject();
      cmdObj["tipo"] = cmd.tipo;
      cmdObj["valor"] = cmd.valor;
      if (cmd.tipo == "ROTATE") {
        cmdObj["direcao"] = cmd.direcao;
      }
    }
  }

  File file = LittleFS.open(ROTAS_FILE, "w");
  if (!file) {
    Serial.println("❌ Erro ao abrir arquivo para escrita");
    return;
  }

  serializeJson(doc, file);
  file.close();
  Serial.printf("💾 %d rotas salvas no LittleFS\n", rotasArmazenadas->size());
}

// Carregar rotas do LittleFS
void carregarRotasLittleFS() {
  if (!LittleFS.exists(ROTAS_FILE)) {
    Serial.println("📂 Nenhum arquivo de rotas encontrado");
    return;
  }

  File file = LittleFS.open(ROTAS_FILE, "r");
  if (!file) {
    Serial.println("❌ Erro ao abrir arquivo para leitura");
    return;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("❌ Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  rotasArmazenadas->clear();
  JsonArray rotasArray = doc["rotas"];

  for (JsonObject rotaObj : rotasArray) {
    Rota rota;
    rota.id = rotaObj["id"];
    rota.dataHora = rotaObj["dataHora"].as<String>();
    rota.deviceId = rotaObj["deviceId"].as<String>();

    JsonArray comandosArray = rotaObj["comandos"];
    for (JsonObject cmdObj : comandosArray) {
      ComandoRota cmd;
      cmd.tipo = cmdObj["tipo"].as<String>();
      cmd.valor = cmdObj["valor"];
      if (cmd.tipo == "ROTATE") {
        cmd.direcao = cmdObj["direcao"].as<String>();
      }
      rota.comandos.push_back(cmd);
    }

    rotasArmazenadas->push_back(rota);
  }

  Serial.printf("✅ %d rotas carregadas do LittleFS\n", rotasArmazenadas->size());
}

// Enviar todas as rotas para um cliente específico
void enviarRotasParaCliente(AsyncWebSocketClient *client) {
  DynamicJsonDocument doc(8192);
  doc["channel"] = "SYNC_ROTAS";
  JsonArray rotasArray = doc.createNestedArray("rotas");

  for (const auto &rota : *rotasArmazenadas) {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;

    JsonArray elementosArray = rotaObj.createNestedArray("elementos");
    for (const auto &cmd : rota.comandos) {
      JsonObject elemObj = elementosArray.createNestedObject();
      elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
      elemObj["valor"] = cmd.valor;
      elemObj["id"] = millis();
      if (cmd.tipo == "ROTATE") {
        elemObj["direcao"] = cmd.direcao;
      }
    }
  }

  String jsonString;
  serializeJson(doc, jsonString);
  client->text(jsonString);
  Serial.printf("📤 Rotas sincronizadas para cliente #%u\n", client->id());
}

void apagarRotas(){
    LittleFS.remove(ROTAS_FILE);
}
