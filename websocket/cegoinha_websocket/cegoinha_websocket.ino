#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>

const char* ssid = "cegoinha";
const char* password = "cegoinha123";

#define ROTAS_FILE "/rotas.json"

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

// --- Pinos do Motor da Porta (L298N) ---
// Mude estes pinos conforme a sua ligação real
#define PIN_IN1 25
#define PIN_IN2 26
#define PIN_ENA 27 // Pino para controle de velocidade (PWM)

// --- Pinos dos Sensores da Porta (Fim de Curso) ---
// Mude estes pinos conforme a sua ligação real
#define SENSOR_PORTA_ABERTA 34
#define SENSOR_PORTA_FECHADA 35

// --- Configuração do PWM da Porta ---
int freqPWM_Porta = 5000;
int canalPWM_Porta = 0; // Canal PWM 0 (verificar se não há conflito com PWM das rodas)
int resolucaoPWM_Porta = 8; // 8 bits (0-255)
int velocidadeMotorPorta = 200; // Velocidade de 0-255

// --- Controle de Estado da Porta (Lógica Não-Bloqueante) ---
#define ESTADO_PORTA_PARADO 0
#define ESTADO_PORTA_ABRINDO 1
#define ESTADO_PORTA_FECHANDO 2

int estadoPorta = ESTADO_PORTA_PARADO; // Estado atual da porta

// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

// Estrutura para armazenar informações do dispositivo conectado
struct DispositivoConectado {
  uint32_t clientId;
  String deviceId;
  String sessionId;
  unsigned long lastSeen;
};

// Mapa de dispositivos conectados
std::vector<DispositivoConectado> dispositivosConectados;

// Estrutura para armazenar comandos de rota
struct ComandoRota {
  String tipo;      // "MOVE" ou "ROTATE"
  int valor;        // distância ou ângulo em graus
  String direcao;   // "direita" ou "esquerda" (apenas para ROTATE)
};

// Estrutura para armazenar uma rota completa
struct Rota {
  unsigned long id;
  std::vector<ComandoRota> comandos;
  String dataHora;
  String deviceId;    // ID do dispositivo que enviou a rota
};

// Vetor para armazenar todas as rotas recebidas
std::vector<Rota> rotasArmazenadas;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML, CSS e JS agora são servidos do LittleFS (pasta data/)
// Para fazer upload dos arquivos para a placa:
// 1. Arduino IDE: Instale o plugin "ESP32 Sketch Data Upload"
// Os arquivos em cegoinha_websocket/data/ serão enviados para o LittleFS


// ===== Funções LittleFS para Persistência de Rotas =====

// Salvar rotas no LittleFS
void salvarRotasLittleFS() {
  DynamicJsonDocument doc(8192);
  JsonArray rotasArray = doc.createNestedArray("rotas");
  
  for (const auto& rota : rotasArmazenadas) {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;
    rotaObj["deviceId"] = rota.deviceId;
    
    JsonArray comandosArray = rotaObj.createNestedArray("comandos");
    for (const auto& cmd : rota.comandos) {
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
  
  for (const auto& rota : rotasArmazenadas) {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;
    
    JsonArray elementosArray = rotaObj.createNestedArray("elementos");
    for (const auto& cmd : rota.comandos) {
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

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
// --- Funções de Baixo Nível do Motor da Porta ---

// Para o motor (freio)
void pararPorta() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  // Use ESP32 LEDC API when available, otherwise fall back to analogWrite
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // ESP32 Arduino Core 3.x: ledcWrite takes pin directly
    ledcWrite(PIN_ENA, 0);
  #else
    // ESP32 Arduino Core 2.x: ledcWrite takes channel
    ledcWrite(canalPWM_Porta, 0);
  #endif
#else
  analogWrite(PIN_ENA, 0);
#endif
}

// Gira em um sentido (Ex: Abrir)
void abrirPortaLogica(int velocidade) { 
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  // Use ESP32 LEDC API when available, otherwise fall back to analogWrite
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // ESP32 Arduino Core 3.x: ledcWrite takes pin directly
    ledcWrite(PIN_ENA, velocidade);
  #else
    // ESP32 Arduino Core 2.x: ledcWrite takes channel
    ledcWrite(canalPWM_Porta, velocidade);
  #endif
#else
  analogWrite(PIN_ENA, velocidade);
#endif
}

// Gira no outro sentido (Ex: Fechar)
void fecharPortaLogica(int velocidade) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  // Use ESP32 LEDC API when available, otherwise fall back to analogWrite
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // ESP32 Arduino Core 3.x: ledcWrite takes pin directly
    ledcWrite(PIN_ENA, velocidade);
  #else
    // ESP32 Arduino Core 2.x: ledcWrite takes channel
    ledcWrite(canalPWM_Porta, velocidade);
  #endif
#else
  analogWrite(PIN_ENA, velocidade);
#endif
}
// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
  void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: //executado quando cliente novo entra
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      // Enviar rotas existentes para o novo cliente após um pequeno delay
      // (aguardar identificação do dispositivo)
      break;
    case WS_EVT_DISCONNECT: //executado quando cliente desconecta
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
    case WS_EVT_DATA: //executado quando chega mensagem
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
  AwsFrameInfo *info = (AwsFrameInfo*)metadados;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) { //verifica se recebe só texto
    
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

    const char* channel = doc["channel"];
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
      
      // Enviar rotas existentes para sincronização
      enviarRotasParaCliente(client);
      return;
    }

    // Processar envio de rotas
    if (strcmp(channel, "ENVIAR_ROTAS") == 0) {
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
      
      // Processar cada comando
      for (JsonObject comandoObj : comandosArray) {
        ComandoRota comando;
        comando.tipo = comandoObj["tipo"].as<String>();
        
        if (comando.tipo == "MOVE") {
          comando.valor = comandoObj["valor"];
          Serial.printf("  - MOVE: %d\n", comando.valor);
        } else if (comando.tipo == "ROTATE") {
          comando.valor = comandoObj["angulo"];
          comando.direcao = comandoObj["direcao"].as<String>();
          Serial.printf("  - ROTATE: %d° para %s\n", comando.valor, comando.direcao.c_str());
        }
        
        novaRota.comandos.push_back(comando);
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
      for (const auto& cmd : novaRota.comandos) {
        JsonObject elemObj = elementosArray.createNestedObject();
        elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
        elemObj["valor"] = cmd.valor;
        elemObj["id"] = millis() + random(1000);
        if (cmd.tipo == "ROTATE") {
          elemObj["direcao"] = cmd.direcao;
        }
      }
      
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

    // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

    // Processar comando para ABRIR A PORTA
    else if (strcmp(channel, "ABRIR") == 0) {
      // Verifica se a porta já não está aberta (sensor HIGH = não pressionado)
      if (digitalRead(SENSOR_PORTA_ABERTA) == HIGH) { 
        Serial.println("Comando: ABRIR. Iniciando abertura...");
        estadoPorta = ESTADO_PORTA_ABRINDO; // Muda o estado
        client->text("{\"status\":\"ok\",\"message\":\"Comando 'ABRIR' recebido. Abrindo...\"}");
      } else {
        Serial.println("Comando: ABRIR. Porta já está aberta.");
        client->text("{\"status\":\"info\",\"message\":\"Porta ja esta aberta.\"}");
      }
    }

    // Processar comando para FECHAR A PORTA
    else if (strcmp(channel, "FECHAR") == 0) {
      // Verifica se a porta já não está fechada (sensor HIGH = não pressionado)
      if (digitalRead(SENSOR_PORTA_FECHADA) == HIGH) { 
        Serial.println("Comando: FECHAR. Iniciando fechamento...");
        estadoPorta = ESTADO_PORTA_FECHANDO; // Muda o estado
        client->text("{\"status\":\"ok\",\"message\":\"Comando 'FECHAR' recebido. Fechando...\"}");
      } else {
        Serial.println("Comando: FECHAR. Porta já está fechada.");
        client->text("{\"status\":\"info\",\"message\":\"Porta ja esta fechada.\"}");
      }
    }

    // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
    
    // Comando genérico
    else {
      float value = doc["value"];
      Serial.printf("Valor recebido: %f\n", value);
    }
  }
}


String setupVariables(const String& var){
  if (var == "VARIAVEL1"){ //Ai coloa %VARIAVEL1% no HTML, que ai vai ser substituida
    return "Valor da variável";
  }
  return String(); //para não crashar se nao existir a variável
}
void notifyClients(String value){
ws.textAll(String(value));
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  
  Serial.println("\n\n=================================");
  Serial.println("    CEGOINHA ESP32 - Iniciando");
  Serial.println("=================================");
  
  // Inicializar LittleFS
  Serial.println("\n--- Inicializando LittleFS ---");
  if (!LittleFS.begin(true)) {
    Serial.println("❌ Erro ao montar LittleFS");
    Serial.println("⚠️ Sistema continuará sem persistência");
  } else {
    Serial.println("✅ LittleFS montado com sucesso");
    
    // Mostrar informações do sistema de arquivos
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("📊 Espaço total: %d bytes\n", totalBytes);
    Serial.printf("📊 Espaço usado: %d bytes (%.1f%%)\n", usedBytes, (usedBytes * 100.0) / totalBytes);
  }
  Serial.println("------------------------------\n");

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
  Serial.println("--- Setup do Motor da Porta ---");
  // --- Setup do Motor ---
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  // Configura o PWM para o pino ENA
  // Use ESP32 LEDC API when available, otherwise use analogWrite on other boards
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    // ESP32 Arduino Core 3.x and later: use new API
    ledcAttach(PIN_ENA, freqPWM_Porta, resolucaoPWM_Porta);
  #else
    // ESP32 Arduino Core 2.x and earlier: use old API
    ledcSetup(canalPWM_Porta, freqPWM_Porta, resolucaoPWM_Porta);
    ledcAttachPin(PIN_ENA, canalPWM_Porta);
  #endif
#else
  // On non-ESP32 platforms (e.g., AVR, ESP8266) use analogWrite. Frequency/resolution
  // may differ depending on the core. Ensure the pin supports PWM on your board.
  pinMode(PIN_ENA, OUTPUT);
#endif
  // Garante que o motor comece parado
  pararPorta(); 
  Serial.println("✅ Driver L298N (Porta) configurado.");

  // --- Setup dos Sensores ---
  // INPUT_PULLUP: O pino fica em HIGH (1) por padrão.
  // Quando o sensor é pressionado, ele aterra o pino, que lê LOW (0).
  pinMode(SENSOR_PORTA_ABERTA, INPUT_PULLUP);
  pinMode(SENSOR_PORTA_FECHADA, INPUT_PULLUP);
  Serial.println("✅ Sensores Fim de Curso (Porta) configurados.");
  Serial.println("------------------------------\n");
  // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
  
  // Inicializar vetores
  rotasArmazenadas.clear();
  dispositivosConectados.clear();
  Serial.println("✅ Sistema de armazenamento de rotas inicializado");
  Serial.println("✅ Sistema de identificação de dispositivos inicializado");
  
  // Carregar rotas salvas
  carregarRotasLittleFS();
  
  // Connect to Wi-Fi
  WiFi.softAP(ssid,password);
  
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
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Arquivo não encontrado. Faça upload dos arquivos da pasta data/ para o LittleFS.");
  });

  // Start server
  server.begin();
  
  Serial.println("Servidor Web iniciado!");
  Serial.println("Aguardando conexões...\n");
  Serial.println("=================================\n");
}

void loop() {
  ws.cleanupClients();

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
  
  // --- MÁQUINA DE ESTADOS DO MOTOR DA PORTA ---
  // Esta parte roda continuamente, verificando o estado da porta
  // sem usar 'delay()' ou 'while()', permitindo que o WebSocket
  // e o servidor web continuem funcionando.

  switch (estadoPorta) {
    
    case ESTADO_PORTA_ABRINDO:
      // Se estamos abrindo, verificamos o sensor de porta aberta
      if (digitalRead(SENSOR_PORTA_ABERTA) == LOW) { // LOW = Pressionado
        // Chegamos ao fim!
        Serial.println("Fim de curso: Porta totalmente aberta.");
        pararPorta();
        estadoPorta = ESTADO_PORTA_PARADO;
        // Avisa todos os clientes que a porta terminou de abrir
        ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"ABERTA\"}"); 
      } else {
        // Ainda não chegamos, continuar abrindo
        abrirPortaLogica(velocidadeMotorPorta);
      }
      break;

    case ESTADO_PORTA_FECHANDO:
      // Se estamos fechando, verificamos o sensor de porta fechada
      if (digitalRead(SENSOR_PORTA_FECHADA) == LOW) { // LOW = Pressionado
        // Chegamos ao fim!
        Serial.println("Fim de curso: Porta totalmente fechada.");
        pararPorta();
        estadoPorta = ESTADO_PORTA_PARADO;
        // Avisa todos os clientes que a porta terminou de fechar
        ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"FECHADA\"}");
      } else {
        // Ainda não chegamos, continuar fechando
        fecharPortaLogica(velocidadeMotorPorta);
      }
      break;

    case ESTADO_PORTA_PARADO:
      // Não faz nada. O motor já está parado.
      break;
  }
  // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
}

// Função auxiliar para obter informações de uma rota específica
String getRotaInfo(size_t indice) {
  if (indice >= rotasArmazenadas.size()) {
    return "Rota não encontrada";
  }
  
  Rota rota = rotasArmazenadas[indice];
  String info = "Rota ID: " + String(rota.id) + "\n";
  info += "Comandos: " + String(rota.comandos.size()) + "\n";
  
  for (size_t i = 0; i < rota.comandos.size(); i++) {
    ComandoRota cmd = rota.comandos[i];
    info += "  " + String(i + 1) + ". ";
    if (cmd.tipo == "MOVE") {
      info += "MOVE " + String(cmd.valor) + "\n";
    } else {
      info += "ROTATE " + String(cmd.valor) + "° " + cmd.direcao + "\n";
    }
  }
  
  return info;
 
float distancia = 0.0;

void loop() {
  // 🔄 Simulação do carrinho se movendo
  distancia += 0.05; // 5 cm
  String json = "{\"distancia\": " + String(distancia, 2) + "}";
  webSocket.broadcastTXT(json);

  delay(500); // Atualiza a cada meio segundo
}

}
