#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "structures_projeto.h"
#include "driver/pcnt.h"
#include "motores_andar.h"


const char *ssid = "cegoinha";
const char *password = "cegoinha123";

#define ROTAS_FILE "/rotas.json"

// ===== CONFIGURAÇÃO MONITORAMENTO DE BATERIA =====
#define BATTERY_ADC_PIN 36        // GPIO36 (VP) - Pino ADC para leitura da bateria
#define VOLTAGE_DIVIDER_RATIO 3.0 // Divisor de tensão (R1=20k, R2=10k) -> (R1+R2)/R2 = 3
#define ADC_RESOLUTION 4095.0     // Resolução do ADC (12 bits)
#define ADC_VOLTAGE 3.3           // Tensão de referência do ADC

// Tensões da bateria Li-Ion 2S (7.4V nominal)
#define BATTERY_VOLTAGE_MAX 8.4   // 100% carregada
#define BATTERY_VOLTAGE_MIN 6.0   // 0% (proteção de descarga)
#define BATTERY_VOLTAGE_NOMINAL 7.4

// Variáveis de bateria
float batteryVoltage = 7.4;
float batterySOC = 100.0;
float batteryVoltageFiltered = 7.4;
const float VOLTAGE_FILTER_ALPHA = 0.1; // Filtro passa-baixa (0.0 a 1.0)

unsigned long lastBatteryUpdate = 0;
const unsigned long BATTERY_UPDATE_INTERVAL = 30000; // Atualiza a cada 1 min

// para contar pulsos
volatile int64_t total_pulsos_esq = 0;
volatile int64_t total_pulsos_dir = 0;

// --- "Bandeiras" (Flags) de Meta ---
volatile bool meta_esq_atingida = false;
volatile bool meta_dir_atingida = false;

int64_t META_PULSOS = 0;

// Tempo de amostragem PID 
unsigned long lastTime = 0;
const int sampleTimePID = 100; // Calcular a cada 100ms

std::vector<Rota> rotasArmazenadas;
int PASSO_ROTA = -1;
unsigned long tempoTerminoComandoAnterior = 0;
const unsigned int intervaloEsperaEntreComandos = 5000;  //VV ajustar


Rota ROTA_ATUAL;
String movimento = "none";
volatile bool aguardandoProximoComando = false;

// --- Variáveis para o Timer de Impressão ---
unsigned long tempoPrintAnterior = 0;
const unsigned long intervaloPrint = 1500;  // Imprime a cada 1s

// --- Variáveis de Contagem do Encoder ---
volatile long contadorPulsos = 0;  // Contador de pulsos do encoder

// --- Variáveis para Cálculo de ETA ---
float distanciaDestino = 0.0;     // Distância total até o destino em cm
float distanciaPercorrida = 0.0;  // Distância acumulada percorrida desde o início da rota em cm
unsigned long ultimoTempoCalculoETA = 0;
const unsigned long intervaloCalculoETA = 500;  // Calcular ETA a cada 500ms
float etaSegundos = 0.0;                        // ETA em segundos

// Mapa de dispositivos conectados
std::vector<DispositivoConectado> dispositivosConectados;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// Salvar rotas no LittleFS
void salvarRotasLittleFS() {
  DynamicJsonDocument doc(8192);
  JsonArray rotasArray = doc.createNestedArray("rotas");

  for (const auto &rota : rotasArmazenadas) {
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
  Serial.printf("💾 %d rotas salvas no LittleFS\n", rotasArmazenadas.size());
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

  rotasArmazenadas.clear();
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

    rotasArmazenadas.push_back(rota);
  }

  Serial.printf("✅ %d rotas carregadas do LittleFS\n", rotasArmazenadas.size());
}

// Enviar todas as rotas para um cliente específico
void enviarRotasParaCliente(AsyncWebSocketClient *client) {
  DynamicJsonDocument doc(8192);
  doc["channel"] = "SYNC_ROTAS";
  JsonArray rotasArray = doc.createNestedArray("rotas");

  for (const auto &rota : rotasArmazenadas) {
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


/**
 * Lê a tensão da bateria através do ADC com divisor de tensão
 */
float lerTensaoBateria() {
  int adcValue = analogRead(BATTERY_ADC_PIN);
  float voltage = (adcValue / ADC_RESOLUTION) * ADC_VOLTAGE * VOLTAGE_DIVIDER_RATIO;
  return voltage;
}

/**
 * Calcula o SOC (State of Charge) baseado na tensão da bateria
 * Usa curva de descarga aproximada de bateria Li-Ion
 */
float calcularSOC(float voltage) {
  // Proteção contra valores fora do range
  if (voltage >= BATTERY_VOLTAGE_MAX) return 100.0;
  if (voltage <= BATTERY_VOLTAGE_MIN) return 0.0;
  
  // Curva de descarga Li-Ion (aproximação linear por segmentos)
  // Li-Ion tem descarga relativamente linear entre 20-80%
  float soc;
  
  if (voltage >= 8.0) {
    // 8.4V - 8.0V = 100% - 90%
    soc = 90.0 + ((voltage - 8.0) / 0.4) * 10.0;
  } else if (voltage >= 7.6) {
    // 8.0V - 7.6V = 90% - 70%
    soc = 70.0 + ((voltage - 7.6) / 0.4) * 20.0;
  } else if (voltage >= 7.2) {
    // 7.6V - 7.2V = 70% - 40%
    soc = 40.0 + ((voltage - 7.2) / 0.4) * 30.0;
  } else if (voltage >= 6.8) {
    // 7.2V - 6.8V = 40% - 20%
    soc = 20.0 + ((voltage - 6.8) / 0.4) * 20.0;
  } else if (voltage >= 6.4) {
    // 6.8V - 6.4V = 20% - 10%
    soc = 10.0 + ((voltage - 6.4) / 0.4) * 10.0;
  } else {
    // 6.4V - 6.0V = 10% - 0%
    soc = ((voltage - 6.0) / 0.4) * 10.0;
  }
  
  return constrain(soc, 0.0, 100.0);
}

/**
 * Atualiza os dados da bateria com filtro passa-baixa
 */
void atualizarBateria() {
  // Lê tensão bruta
  float voltageRaw = lerTensaoBateria();
  
  // Aplica filtro passa-baixa para suavizar leituras
  batteryVoltageFiltered = (VOLTAGE_FILTER_ALPHA * voltageRaw) + 
                           ((1.0 - VOLTAGE_FILTER_ALPHA) * batteryVoltageFiltered);
  
  batteryVoltage = batteryVoltageFiltered;
  batterySOC = calcularSOC(batteryVoltage);
}

/**
 * Envia dados da bateria via WebSocket para todos os clientes
 */
void enviarDadosBateria() {
  DynamicJsonDocument doc(256);
  doc["channel"] = "BATTERY_STATUS";
  doc["voltage"] = round(batteryVoltage * 100.0) / 100.0; // 2 casas decimais
  doc["soc"] = round(batterySOC * 10.0) / 10.0; // 1 casa decimal
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  ws.textAll(jsonString);
}

/**
 * Verifica nível crítico da bateria
 */
void verificarBateriaCritica() {
  if (batterySOC <= 10.0) {
    Serial.println("⚠️ BATERIA CRÍTICA! SOC: " + String(batterySOC) + "%");
    
    // Para o carrinho se estiver em movimento
    if (PASSO_ROTA >= 0) {
      //pararMotores();
      //PASSO_ROTA = -1;
      //META_PULSOS = 0;
      //ROTA_ATUAL = Rota();
      //vv tirar aviso da bateria
      // Notifica clientes
      //ws.textAll("{\"channel\":\"BATTERY_CRITICAL\",\"status\":\"stopped\",\"message\":\"Bateria crítica! Carrinho parado.\"}");
    }
  } else if (batterySOC <= 20.0) {
    // Apenas aviso
    Serial.println("⚠️ Bateria baixa: " + String(batterySOC) + "%");
  }
}


// Função para definir a distância até o destino
void definirDistanciaDestino(float distancia) {
  distanciaDestino = distancia;
  distanciaPercorrida = 0.0;  // Reseta a distância percorrida ao definir novo destino
  Serial.printf("Distância até destino definida: %.2f cm\n", distanciaDestino);
}


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:  // executado quando cliente novo entra
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      // Enviar rotas existentes para o novo cliente após um pequeno delay
      // (aguardar identificação do dispositivo)
      break;
    case WS_EVT_DISCONNECT:  // executado quando cliente desconecta
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      // Remover dispositivo da lista ao desconectar
      for (size_t i = 0; i < dispositivosConectados.size(); i++) {
        if (dispositivosConectados[i].clientId == client->id()) {
          Serial.printf("📤 Dispositivo %s desconectado\n", dispositivosConectados[i].deviceId.c_str());
          dispositivosConectados.erase(dispositivosConectados.begin() + i);
          break;
        }
      }
      break;
    case WS_EVT_DATA:  // executado quando chega mensagem
      mensagemRecebida(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void mensagemRecebida(AsyncWebSocketClient *client, void *metadados, uint8_t *mensagem, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)metadados;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {  // verifica se recebe só texto

    // Aumentar tamanho do buffer JSON para acomodar rotas maiores
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, mensagem, len);

    if (error) {
      Serial.print("Falha ao ler JSON: ");
      Serial.println(error.c_str());
      return;
    }

    if (!doc.containsKey("channel")) {
      Serial.println("JSON recebido não contém a chave 'channel'.");
      return;
    }

    const char *channel = doc["channel"];
    Serial.printf("Canal recebido: %s\n", channel);

    // Processar identificação de dispositivo
    if (strcmp(channel, "DEVICE_ID") == 0) {
      String deviceId = doc["deviceId"].as<String>();
      String sessionId = doc["sessionId"].as<String>();

      // Verificar se dispositivo já existe
      bool dispositivoExistente = false;
      for (auto &disp : dispositivosConectados) {
        if (disp.deviceId == deviceId) {
          // Atualizar informações do dispositivo existente
          disp.clientId = client->id();
          disp.sessionId = sessionId;
          disp.lastSeen = millis();
          dispositivoExistente = true;
          Serial.printf("🔄 Dispositivo reconectado: %s (Client #%u)\n", deviceId.c_str(), client->id());
          break;
        }
      }

      if (!dispositivoExistente) {
        // Adicionar novo dispositivo
        DispositivoConectado novoDispositivo;
        novoDispositivo.clientId = client->id();
        novoDispositivo.deviceId = deviceId;
        novoDispositivo.sessionId = sessionId;
        novoDispositivo.lastSeen = millis();
        dispositivosConectados.push_back(novoDispositivo);
        Serial.printf("🆕 Novo dispositivo registrado: %s (Client #%u)\n", deviceId.c_str(), client->id());
      }

      Serial.printf("📊 Total de dispositivos: %d\n", dispositivosConectados.size());

      // Enviar confirmação
      String resposta = "{\"status\":\"ok\",\"message\":\"Dispositivo identificado\",\"deviceId\":\"" + deviceId + "\"}";
      client->text(resposta);

      atualizarBateria();
      enviarDadosBateria();
      verificarBateriaCritica();
      // Enviar rotas existentes para sincronização
      enviarRotasParaCliente(client);
      return;
    }

    // Processar envio de rotas
    if (strcmp(channel, "ENVIAR_ROTAS") == 0) {
      if (ROTA_ATUAL.comandos.size() > 0) {
        ws.textAll("Rota em andamento!! aguarde");  // VV implementar no front dps
        return;
      }
      if (!doc.containsKey("value")) {
        Serial.println("Erro: 'value' não encontrado para ENVIAR_ROTAS");
        return;
      }

      String deviceId = doc.containsKey("deviceId") ? doc["deviceId"].as<String>() : "unknown";
      JsonArray comandosArray = doc["value"].as<JsonArray>();

      // Criar nova rota
      Rota novaRota;
      novaRota.id = doc.containsKey("rotaId") ? doc["rotaId"].as<unsigned long>() : millis();
      novaRota.dataHora = String(novaRota.id);
      novaRota.deviceId = deviceId;

      Serial.println("=== Nova Rota Recebida ===");
      Serial.printf("Device ID: %s\n", deviceId.c_str());
      Serial.printf("ID da Rota: %lu\n", novaRota.id);
      Serial.printf("Total de comandos: %d\n", comandosArray.size());

      // Processar cada comando e calcular distância total
      float distanciaTotal = 0.0;
      for (JsonObject comandoObj : comandosArray) {
        ComandoRota comando;
        comando.tipo = comandoObj["tipo"].as<String>();

        if (comando.tipo == "MOVE") {
          comando.valor = comandoObj["valor"];
          distanciaTotal += comando.valor;  // Acumula a distância
          Serial.printf("  - MOVE: %d\n", comando.valor);
        } else if (comando.tipo == "ROTATE") {
          comando.valor = comandoObj["angulo"];
          comando.direcao = comandoObj["direcao"].as<String>();
          Serial.printf("  - ROTATE: %d° para %s\n", comando.valor, comando.direcao.c_str());
        }

        novaRota.comandos.push_back(comando);
      }

      // Definir a distância total calculada como destino
      definirDistanciaDestino(distanciaTotal);
      Serial.printf("📏 Distância total da rota: %.2f cm\n", distanciaTotal);

      // Verificar se já atingiu o limite de rotas
      if (rotasArmazenadas.size() >= 5) {
        Serial.printf("⚠️ Limite de %d rotas atingido. Removendo rota mais antiga (ID: %lu)\n", 
                      5, rotasArmazenadas[0].id);
        rotasArmazenadas.erase(rotasArmazenadas.begin()); // Remove a primeira (mais antiga)
      }

      // Adicionar rota ao armazenamento
      rotasArmazenadas.push_back(novaRota);
      Serial.printf("Rota armazenada! Total de rotas: %d\n", rotasArmazenadas.size());

      // Salvar no LittleFS
      salvarRotasLittleFS();

      Serial.println("==========================\n");

      // Notificar TODOS os clientes sobre a nova rota
      DynamicJsonDocument notifDoc(2048);
      notifDoc["channel"] = "NOVA_ROTA";
      notifDoc["rotaId"] = novaRota.id;
      notifDoc["totalRotas"] = rotasArmazenadas.size();

      JsonObject rotaObj = notifDoc.createNestedObject("rota");
      rotaObj["id"] = novaRota.id;
      rotaObj["dataHora"] = novaRota.dataHora;

      JsonArray elementosArray = rotaObj.createNestedArray("elementos");
      for (const auto &cmd : novaRota.comandos) {
        JsonObject elemObj = elementosArray.createNestedObject();
        elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
        elemObj["valor"] = cmd.valor;
        elemObj["id"] = millis() + random(1000);
        if (cmd.tipo == "ROTATE") {
          elemObj["direcao"] = cmd.direcao;
        }
      }

      ROTA_ATUAL = novaRota;
      proximoComando(ROTA_ATUAL);
      String notifString;
      serializeJson(notifDoc, notifString);
      ws.textAll(notifString);
    }

    // Processar comando para limpar rotas
    else if (strcmp(channel, "LIMPAR_ROTAS") == 0) {
      String deviceId = doc.containsKey("deviceId") ? doc["deviceId"].as<String>() : "unknown";
      int totalRotasAntes = rotasArmazenadas.size();
      rotasArmazenadas.clear();

      // Limpar arquivo LittleFS
      LittleFS.remove(ROTAS_FILE);

      Serial.println("=== Rotas Limpas ===");
      Serial.printf("Device ID: %s\n", deviceId.c_str());
      Serial.printf("Rotas removidas: %d\n", totalRotasAntes);
      Serial.println("====================\n");

      // Notificar TODOS os clientes
      String resposta = "{\"channel\":\"ROTAS_LIMPAS\",\"status\":\"ok\",\"rotasRemovidas\":" + String(totalRotasAntes) + "}";
      ws.textAll(resposta);
    }

    // Processar comando para listar rotas armazenadas
    else if (strcmp(channel, "LISTAR_ROTAS") == 0) {
      Serial.println("=== Rotas Armazenadas ===");
      Serial.printf("Total: %d rotas\n", rotasArmazenadas.size());

      for (size_t i = 0; i < rotasArmazenadas.size(); i++) {
        Serial.printf("\nRota %d (ID: %lu):\n", i + 1, rotasArmazenadas[i].id);
        Serial.printf("  Comandos: %d\n", rotasArmazenadas[i].comandos.size());

        for (size_t j = 0; j < rotasArmazenadas[i].comandos.size(); j++) {
          ComandoRota cmd = rotasArmazenadas[i].comandos[j];
          if (cmd.tipo == "MOVE") {
            Serial.printf("    %d. MOVE %d\n", j + 1, cmd.valor);
          } else {
            Serial.printf("    %d. ROTATE %d° %s\n", j + 1, cmd.valor, cmd.direcao.c_str());
          }
        }
      }
      Serial.println("=========================\n");
    }

    // Processar comando para PARAR O CARRINHO
    else if (strcmp(channel, "PARAR_CARRINHO") == 0)
    {
      Serial.println("⛔ Comando: PARAR_CARRINHO recebido!");
      
      // Para todos os motores imediatamente
      pararMotores();
      
      // Reseta as variáveis de controle da rota
      PASSO_ROTA = -1;
      META_PULSOS = 0;
      total_pulsos_esq = 0;
      total_pulsos_dir = 0;
      meta_esq_atingida = false;
      meta_dir_atingida = false;
      aguardandoProximoComando = false;
      movimento = "none";
      
      // Limpa a rota atual
      ROTA_ATUAL = Rota();
      
      Serial.println("✅ Carrinho parado. Rota cancelada.");
      
      // Notifica todos os clientes
      String resposta = "{\"channel\":\"CARRINHO_PARADO\",\"status\":\"ok\",\"message\":\"Carrinho parado e rota cancelada\"}";
      ws.textAll(resposta);
      
      client->text("{\"status\":\"ok\",\"message\":\"Comando 'PARAR_CARRINHO' executado\"}");
    }

    // Processar comando para DEFINIR DISTÂNCIA DESTINO
    else if (strcmp(channel, "DEFINIR_DISTANCIA") == 0) {
      if (doc.containsKey("value")) {
        float distancia = doc["value"];
        definirDistanciaDestino(distancia);
        client->text("{\"status\":\"ok\",\"message\":\"Distância até destino definida\"}");
      } else {
        Serial.println("Erro: 'value' não encontrado para DEFINIR_DISTANCIA");
        client->text("{\"status\":\"error\",\"message\":\"Valor não encontrado\"}");
      }
    }

    // Comando genérico
    else {
      float value = doc["value"];
      Serial.printf("Valor recebido: %f\n", value);
    }
  }
}

String setupVariables(const String &var) {
  if (var == "VARIAVEL1") {  // Ai coloa %VARIAVEL1% no HTML, que ai vai ser substituida
    return "Valor da variável";
  }
  return String();  // para não crashar se nao existir a variável
}
void notifyClients(String value) {
  ws.textAll(String(value));
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Inicializar LittleFS
  Serial.println("\n--- Inicializando LittleFS ---");
  if (!LittleFS.begin(true)) {
    Serial.println("❌ Erro ao montar LittleFS");
    Serial.println("⚠ Sistema continuará sem persistência");
  } else {
    Serial.println("✅ LittleFS montado com sucesso");

    // Mostrar informações do sistema de arquivos
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("📊 Espaço total: %d bytes\n", totalBytes);
    Serial.printf("📊 Espaço usado: %d bytes (%.1f%%)\n", usedBytes, (usedBytes * 100.0) / totalBytes);
  }
  Serial.println("------------------------------\n");

  // Inicializar vetores
  rotasArmazenadas.clear();
  dispositivosConectados.clear();
  Serial.println("✅ Sistema de armazenamento de rotas inicializado");
  Serial.println("✅ Sistema de identificação de dispositivos inicializado");

  // Carregar rotas salvas
  carregarRotasLittleFS();

  // Connect to Wi-Fi
  WiFi.softAP(ssid, password);

  // Print IP address and start web server
  Serial.println("\n--- Configuração de Rede ---");
  Serial.println("Modo: Access Point");
  Serial.printf("SSID: %s\n", ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("----------------------------\n");

  initWebSocket();

  // Servir arquivos estáticos do LittleFS
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Rota alternativa caso o arquivo não seja encontrado
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Arquivo não encontrado. Faça upload dos arquivos da pasta data/ para o LittleFS.");
  });

  // Start server
  server.begin();

  Serial.println("Servidor Web iniciado!");
  Serial.println("Aguardando conexões...\n");
  Serial.println("=================================\n");


  //andar
  motoresSetup(&ws, &PASSO_ROTA, &movimento, &META_PULSOS, &ROTA_ATUAL, &total_pulsos_esq, &total_pulsos_dir, &sampleTimePID);

  Serial.println("\n--- Configurando Monitoramento de Bateria ---");
  
  // Configura o pino ADC para leitura da bateria
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogSetAttenuation(ADC_11db); // Permite leitura até 3.3V
  
  // Leitura inicial
  atualizarBateria();
  Serial.printf("📊 Tensão da bateria: %.2fV\n", batteryVoltage);
  Serial.printf("🔋 SOC inicial: %.1f%%\n", batterySOC);
  
  if (batterySOC < 20.0) {
    Serial.println("⚠️ ATENÇÃO: Bateria baixa! Considere recarregar.");
  }
  
  Serial.println("✅ Monitoramento de bateria configurado");
  Serial.println("---------------------------------------------\n");

  // ENCODER

  configuraEncoderEsquerdoPCNT();
  configuraEncoderDireitoPCNT();
}

void loop() {
  ws.cleanupClients();

  unsigned long currentTime = millis();
  if (currentTime - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
    lastBatteryUpdate = currentTime;
    
    atualizarBateria();
    enviarDadosBateria();
    verificarBateriaCritica();
  }
  
  // 1. Variáveis para guardar as leituras parciais
  int16_t parcial_esq = 0;
  int16_t parcial_dir = 0;

  // --- Leitura Atômica do Encoder ESQUERDO ---
  if (!meta_esq_atingida) {
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_get_counter_value(PCNT_UNIT_0, &parcial_esq);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);

    if (parcial_esq != 0) {
      total_pulsos_esq += (int64_t)parcial_esq;
    }
  }

  // --- Leitura Atômica do Encoder DIREITO ---
  if (!meta_dir_atingida) {
    pcnt_counter_pause(PCNT_UNIT_1);
    pcnt_get_counter_value(PCNT_UNIT_1, &parcial_dir);
    pcnt_counter_clear(PCNT_UNIT_1);
    pcnt_counter_resume(PCNT_UNIT_1);

    if (parcial_dir != 0) {
      total_pulsos_dir += (int64_t)parcial_dir;
    }
  }

  // --- Lógica de Parada (individual) ---
  if (META_PULSOS != 0) {
    if (movimento == "MOVE") {
      if (millis() - lastTime >= sampleTimePID) {
    
      calcularPID();
      lastTime = millis();

    }
      if ((total_pulsos_esq >= META_PULSOS) && (!meta_esq_atingida)) {
        meta_esq_atingida = true;
        moverMotorEsq(0);
        Serial.println(">>> META ESQUERDA ATINGIDA! <<<");
        ws.textAll(">>> META ESQUERDA ATINGIDA! <<<");
      }

      if ((total_pulsos_dir >= META_PULSOS) && (!meta_dir_atingida)) {
        meta_dir_atingida = true;
        moverMotorDir(0);
        Serial.println(">>> META DIREITA ATINGIDA! <<<");
        ws.textAll(">>> META DIREITA ATINGIDA! <<<");
      }

      // --- Verificação Final ---
      if (meta_esq_atingida && meta_dir_atingida) {
        // prepara para próxima rota
        pararMotores();
        META_PULSOS = 0;
        meta_dir_atingida = false;
        meta_esq_atingida = false;
        total_pulsos_dir = 0;
        total_pulsos_esq = 0;
        parcial_dir = 0;
        parcial_esq = 0;

        aguardandoProximoComando = true;
        ws.textAll("Esperaremos " + String(intervaloEsperaEntreComandos) + " segundos até o próximo comando");
        tempoTerminoComandoAnterior = millis();
      }
    }

    else if (movimento == "ROTATE_D") {
      if ((total_pulsos_esq >= META_PULSOS) && (!meta_esq_atingida)) {
        meta_esq_atingida = true;
        moverMotorEsq(2);
        Serial.println(">>> META ESQUERDA ATINGIDA! <<<");
        ws.textAll(">>> META ESQUERDA ATINGIDA! <<<");
      }
      if (meta_esq_atingida) {
        // prepara para próxima rota
        pararMotores();
        META_PULSOS = 0;
        meta_dir_atingida = false;
        meta_esq_atingida = false;
        total_pulsos_dir = 0;
        total_pulsos_esq = 0;
        parcial_dir = 0;
        parcial_esq = 0;

        aguardandoProximoComando = true;
        ws.textAll("Esperaremos " + String(intervaloEsperaEntreComandos) + " segundos até o próximo comando");
        tempoTerminoComandoAnterior = millis();
      }
    }

    else if (movimento == "ROTATE_E") {
      if ((total_pulsos_dir >= META_PULSOS) && (!meta_dir_atingida)) {
        meta_dir_atingida = true;
        moverMotorDir(2);
        Serial.println(">>> META DIREITA ATINGIDA! <<<");
        ws.textAll(">>> META DIREITA ATINGIDA! <<<");
      }
      if (meta_dir_atingida) {
        // prepara para próxima rota
        pararMotores();
        META_PULSOS = 0;
        meta_dir_atingida = false;
        meta_esq_atingida = false;
        total_pulsos_dir = 0;
        total_pulsos_esq = 0;
        parcial_dir = 0;
        parcial_esq = 0;

        aguardandoProximoComando = true;
        ws.textAll("Esperaremos " + String(intervaloEsperaEntreComandos) + " segundos até o próximo comando");
        tempoTerminoComandoAnterior = millis();
      }
    }

  }
  // --- Bloco de Impressão (Debug) ---
  unsigned long tempoAtual = millis();
  if (tempoAtual - tempoPrintAnterior >= intervaloPrint) {
    tempoPrintAnterior = tempoAtual;  // Reinicia o timer de print

    // O '\n' no final significa "pular linha"
    Serial.printf("ESQ: %lld | DIR: %lld \n",
                  total_pulsos_esq,
                  total_pulsos_dir);
    ws.textAll("ESQ: " + String(total_pulsos_esq) + " | DIR: " + String(total_pulsos_dir) + " \n");
  }
  tempoAtual = millis();
  if (aguardandoProximoComando && (tempoAtual - tempoTerminoComandoAnterior >= intervaloEsperaEntreComandos)) {
    aguardandoProximoComando = false;
    ws.textAll(String(intervaloEsperaEntreComandos) + " segundos esperados");
    proximoComando(ROTA_ATUAL);
  }
}