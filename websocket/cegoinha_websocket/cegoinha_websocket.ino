#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "esp_task_wdt.h"

const char *ssid = "cegoinha";
const char *password = "cegoinha123";

#define ROTAS_FILE "/rotas.json"
#define ERRORS_LOG_FILE "/errors.log"

// Limites de memória para prevenir overflow
#define MAX_ROTAS 50
#define MAX_DISPOSITIVOS 10
#define MAX_DISTANCIA 10000  // cm
#define MAX_ANGULO 360       // graus
#define WATCHDOG_TIMEOUT 10  // segundos

// Mutex para proteção de race condition
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// ===== INÍCIO: CÓDIGO DO ENCODER E VELOCIDADE =====

// --- Pinos do Encoder ---
#define ENCODER_PIN_A 35 // IO35 conectado ao OUT A do encoder
#define ENCODER_PIN_B 34 // IO34 conectado ao OUT B do encoder

// --- Parâmetros do Motor e Encoder ---
#define PULSOS_POR_REVOLUCAO 20.0 // Número de pulsos por revolução do encoder (ajuste conforme seu motor)
#define DIAMETRO_RODA 6.5         // Diâmetro da roda em cm (ajuste conforme seu carrinho)
#define PI 3.14159265359

// --- Variáveis de Contagem do Encoder ---
volatile long contadorPulsos = 0; // Contador de pulsos do encoder

// --- Variáveis para Cálculo de Velocidade ---
unsigned long ultimoTempoCalculo = 0;
const unsigned long intervaloCalculo = 100; // Calcular velocidade a cada 100ms
float velocidadeAtual = 0.0;                // Velocidade em cm/s

// --- Variáveis para Cálculo de ETA ---
float distanciaDestino = 0.0;    // Distância total até o destino em cm
float distanciaPercorrida = 0.0; // Distância acumulada percorrida desde o início da rota em cm
unsigned long ultimoTempoCalculoETA = 0;
const unsigned long intervaloCalculoETA = 500; // Calcular ETA a cada 500ms
float etaSegundos = 0.0;                       // ETA em segundos

// ===== FIM: CÓDIGO DO ENCODER E VELOCIDADE =====

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

// --- Pinos do Motor da Porta (L298N) ---
// Mude estes pinos conforme a sua ligação real
#define PIN_IN1 36
#define PIN_IN2 37
// Pino ENA deve estar conectado direto ao 5V ou 12V para velocidade máxima

// --- Configuração de Tempo da Porta ---
#define TEMPO_ABERTURA_MS 3000   // Tempo para abrir completamente (3 segundos)
#define TEMPO_FECHAMENTO_MS 3000 // Tempo para fechar completamente (3 segundos)
#define TEMPO_SEGURAR_MS 5000    // Tempo máximo para segurar posição (5 segundos)

// --- Controle de Estado da Porta (Lógica Não-Bloqueante) ---
#define ESTADO_PORTA_PARADO 0
#define ESTADO_PORTA_ABRINDO 1
#define ESTADO_PORTA_FECHANDO 2
#define ESTADO_PORTA_SEGURANDO 3 // Novo estado: segurar posição

int estadoPorta = ESTADO_PORTA_PARADO;  // Estado atual da porta
unsigned long tempoInicioMovimento = 0; // Marca quando o movimento começou
unsigned long tempoInicioSegurar = 0;   // Marca quando começou a segurar

// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

// Estrutura para armazenar informações do dispositivo conectado
struct DispositivoConectado
{
  uint32_t clientId;
  String deviceId;
  String sessionId;
  unsigned long lastSeen;
};

// Mapa de dispositivos conectados
std::vector<DispositivoConectado> dispositivosConectados;

// Estrutura para armazenar comandos de rota
struct ComandoRota
{
  String tipo;    // "MOVE" ou "ROTATE"
  int valor;      // distância ou ângulo em graus
  String direcao; // "direita" ou "esquerda" (apenas para ROTATE)
};

// Estrutura para armazenar uma rota completa
struct Rota
{
  unsigned long id;
  std::vector<ComandoRota> comandos;
  String dataHora;
  String deviceId; // ID do dispositivo que enviou a rota
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

// Função para registrar erros no LittleFS
void logErro(String erro)
{
  File logFile = LittleFS.open(ERRORS_LOG_FILE, "a");
  if (!logFile)
  {
    Serial.println("❌ Erro ao abrir arquivo de log");
    return;
  }

  String timestamp = String(millis() / 1000); // segundos desde boot
  String logEntry = "[" + timestamp + "s] " + erro + "\n";
  logFile.print(logEntry);
  logFile.close();

  Serial.print("📝 Log: ");
  Serial.print(logEntry);
}

// Salvar rotas no LittleFS
void salvarRotasLittleFS()
{
  // Calcular tamanho necessário do buffer JSON dinamicamente
  size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(rotasArmazenadas.size());
  for (const auto &rota : rotasArmazenadas)
  {
    capacity += JSON_OBJECT_SIZE(3) + JSON_ARRAY_SIZE(rota.comandos.size());
    capacity += rota.comandos.size() * JSON_OBJECT_SIZE(3);
    capacity += rota.dataHora.length() + rota.deviceId.length();
  }
  capacity += 512; // margem de segurança

  DynamicJsonDocument doc(capacity);
  JsonArray rotasArray = doc.createNestedArray("rotas");

  for (const auto &rota : rotasArmazenadas)
  {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;
    rotaObj["deviceId"] = rota.deviceId;

    JsonArray comandosArray = rotaObj.createNestedArray("comandos");
    for (const auto &cmd : rota.comandos)
    {
      JsonObject cmdObj = comandosArray.createNestedObject();
      cmdObj["tipo"] = cmd.tipo;
      cmdObj["valor"] = cmd.valor;
      if (cmd.tipo == "ROTATE")
      {
        cmdObj["direcao"] = cmd.direcao;
      }
    }
  }

  File file = LittleFS.open(ROTAS_FILE, "w");
  if (!file)
  {
    logErro("Erro ao abrir arquivo para escrita: " + String(ROTAS_FILE));
    Serial.println("❌ Erro ao abrir arquivo para escrita");
    return;
  }

  serializeJson(doc, file);
  file.close();
  Serial.printf("💾 %d rotas salvas no LittleFS (buffer: %d bytes)\n", rotasArmazenadas.size(), capacity);
}

// Carregar rotas do LittleFS
void carregarRotasLittleFS()
{
  if (!LittleFS.exists(ROTAS_FILE))
  {
    Serial.println("📂 Nenhum arquivo de rotas encontrado");
    return;
  }

  File file = LittleFS.open(ROTAS_FILE, "r");
  if (!file)
  {
    logErro("Erro ao abrir arquivo para leitura: " + String(ROTAS_FILE));
    Serial.println("❌ Erro ao abrir arquivo para leitura");
    return;
  }

  // Calcular tamanho necessário baseado no tamanho do arquivo
  size_t fileSize = file.size();
  size_t capacity = fileSize + 512; // margem de segurança

  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    logErro("Erro ao parsear JSON: " + String(error.c_str()));
    Serial.print("❌ Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }

  rotasArmazenadas.clear();
  JsonArray rotasArray = doc["rotas"];

  for (JsonObject rotaObj : rotasArray)
  {
    Rota rota;
    rota.id = rotaObj["id"];
    rota.dataHora = rotaObj["dataHora"].as<String>();
    rota.deviceId = rotaObj["deviceId"].as<String>();

    JsonArray comandosArray = rotaObj["comandos"];
    for (JsonObject cmdObj : comandosArray)
    {
      ComandoRota cmd;
      cmd.tipo = cmdObj["tipo"].as<String>();
      cmd.valor = cmdObj["valor"];
      if (cmd.tipo == "ROTATE")
      {
        cmd.direcao = cmdObj["direcao"].as<String>();
      }
      rota.comandos.push_back(cmd);
    }

    rotasArmazenadas.push_back(rota);
  }

  Serial.printf("✅ %d rotas carregadas do LittleFS\n", rotasArmazenadas.size());
}

// Enviar todas as rotas para um cliente específico
void enviarRotasParaCliente(AsyncWebSocketClient *client)
{
  // Calcular tamanho necessário do buffer JSON dinamicamente
  size_t capacity = JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(rotasArmazenadas.size());
  for (const auto &rota : rotasArmazenadas)
  {
    capacity += JSON_OBJECT_SIZE(2) + JSON_ARRAY_SIZE(rota.comandos.size());
    capacity += rota.comandos.size() * JSON_OBJECT_SIZE(4);
  }
  capacity += 1024; // margem de segurança

  DynamicJsonDocument doc(capacity);
  doc["channel"] = "SYNC_ROTAS";
  JsonArray rotasArray = doc.createNestedArray("rotas");

  for (const auto &rota : rotasArmazenadas)
  {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;

    JsonArray elementosArray = rotaObj.createNestedArray("elementos");
    for (const auto &cmd : rota.comandos)
    {
      JsonObject elemObj = elementosArray.createNestedObject();
      elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
      elemObj["valor"] = cmd.valor;
      elemObj["id"] = millis();
      if (cmd.tipo == "ROTATE")
      {
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

// Para o motor completamente (desliga)
void pararPorta()
{
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
}

// Gira em um sentido (Ex: Abrir)
void abrirPortaLogica()
{
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
}

// Gira no outro sentido (Ex: Fechar)
void fecharPortaLogica()
{
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
}

// Segura a posição (freio do motor - ambos HIGH)
void segurarPosicaoPorta()
{
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, HIGH);
}
// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

// ===== INÍCIO: FUNÇÕES DO ENCODER =====

// Função de interrupção para o encoder (canal A)
void IRAM_ATTR encoderISR()
{
  // Lê o estado do canal B para determinar a direção
  int estadoB = digitalRead(ENCODER_PIN_B);

  // Proteção contra race condition usando critical section
  portENTER_CRITICAL_ISR(&timerMux);

  // Se B está HIGH quando A muda, está girando para frente
  // Se B está LOW quando A muda, está girando para trás
  if (estadoB == HIGH)
  {
    contadorPulsos++;
  }
  else
  {
    contadorPulsos--;
  }

  portEXIT_CRITICAL_ISR(&timerMux);
}

// Função para calcular e enviar a velocidade
void calcularEEnviarVelocidade()
{
  unsigned long tempoAtual = millis();

  // Verifica se já passou o intervalo de cálculo
  if (tempoAtual - ultimoTempoCalculo >= intervaloCalculo)
  {
    // Calcula o tempo decorrido em segundos
    float tempoDecorrido = (tempoAtual - ultimoTempoCalculo) / 1000.0;

    // Desabilita interrupções temporariamente para ler o contador (proteção contra race condition)
    portENTER_CRITICAL(&timerMux);
    long pulsos = contadorPulsos;
    contadorPulsos = 0; // Reseta o contador
    portEXIT_CRITICAL(&timerMux);

    // Calcula o número de revoluções
    float revolucoes = pulsos / PULSOS_POR_REVOLUCAO;

    // Calcula a distância percorrida (em cm)
    float distancia = revolucoes * PI * DIAMETRO_RODA;

    // Acumula a distância percorrida para cálculo do ETA
    distanciaPercorrida += abs(distancia);

    // Calcula a velocidade (cm/s)
    velocidadeAtual = distancia / tempoDecorrido;

    // Envia a velocidade via WebSocket
    DynamicJsonDocument doc(256);
    doc["channel"] = "VELOCIDADE";
    doc["value"] = abs(velocidadeAtual); // Envia o valor absoluto

    String jsonString;
    serializeJson(doc, jsonString);
    ws.textAll(jsonString);

    // Debug no Serial
    Serial.printf("Velocidade: %.2f cm/s\n", abs(velocidadeAtual));

    // Atualiza o tempo da última leitura
    ultimoTempoCalculo = tempoAtual;
  }
}

// ===== FIM: FUNÇÕES DO ENCODER =====

// ===== INÍCIO: FUNÇÕES DO ETA =====

// Função para definir a distância até o destino
void definirDistanciaDestino(float distancia)
{
  distanciaDestino = distancia;
  distanciaPercorrida = 0.0; // Reseta a distância percorrida ao definir novo destino
  Serial.printf("Distância até destino definida: %.2f cm\n", distanciaDestino);
}

// Função para calcular e enviar o ETA
void calcularEEnviarETA()
{
  unsigned long tempoAtual = millis();

  // Verifica se já passou o intervalo de cálculo do ETA
  if (tempoAtual - ultimoTempoCalculoETA >= intervaloCalculoETA)
  {
    // Calcula a distância restante até o destino
    float distanciaRestante = distanciaDestino - distanciaPercorrida;
    
    // Garante que a distância restante não seja negativa
    if (distanciaRestante < 0.0)
    {
      distanciaRestante = 0.0;
    }

    // Se não há distância restante ou velocidade é muito baixa, não calcula
    if (distanciaRestante <= 0.0 || abs(velocidadeAtual) < 0.1)
    {
      etaSegundos = 0.0;
    }
    else
    {
      // Calcula o ETA em segundos: tempo = distância restante / velocidade
      etaSegundos = distanciaRestante / abs(velocidadeAtual);
    }

    // Envia o ETA via WebSocket
    DynamicJsonDocument doc(256);
    doc["channel"] = "ETA";
    doc["value"] = etaSegundos;
    doc["distancia"] = distanciaRestante;
    doc["velocidade"] = abs(velocidadeAtual);

    String jsonString;
    serializeJson(doc, jsonString);
    ws.textAll(jsonString);

    // Debug no Serial
    if (etaSegundos > 0)
    {
      Serial.printf("ETA: %.2f segundos (Distância restante: %.2f cm, Velocidade: %.2f cm/s)\n",
                    etaSegundos, distanciaRestante, abs(velocidadeAtual));
    }

    // Atualiza o tempo da última leitura
    ultimoTempoCalculoETA = tempoAtual;
  }
}

// ===== FIM: FUNÇÕES DO ETA =====

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT: // executado quando cliente novo entra
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    // Enviar rotas existentes para o novo cliente após um pequeno delay
    // (aguardar identificação do dispositivo)
    break;
  case WS_EVT_DISCONNECT: // executado quando cliente desconecta
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    // Remover dispositivo da lista ao desconectar
    for (size_t i = 0; i < dispositivosConectados.size(); i++)
    {
      if (dispositivosConectados[i].clientId == client->id())
      {
        Serial.printf("📤 Dispositivo %s desconectado\n", dispositivosConectados[i].deviceId.c_str());
        dispositivosConectados.erase(dispositivosConectados.begin() + i);
        break;
      }
    }
    break;
  case WS_EVT_DATA: // executado quando chega mensagem
    mensagemRecebida(client, arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}
void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void mensagemRecebida(AsyncWebSocketClient *client, void *metadados, uint8_t *mensagem, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)metadados;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  { // verifica se recebe só texto

    // Calcular tamanho necessário do buffer baseado no tamanho da mensagem
    size_t capacity = len + 512; // margem de segurança
    DynamicJsonDocument doc(capacity);
    DeserializationError error = deserializeJson(doc, mensagem, len);

    if (error)
    {
      String erro = "Falha ao ler JSON: " + String(error.c_str());
      logErro(erro);
      Serial.print("❌ ");
      Serial.println(erro);
      client->text("{\"status\":\"error\",\"message\":\"JSON inválido\"}");
      return;
    }

    if (!doc.containsKey("channel"))
    {
      logErro("JSON recebido não contém a chave 'channel'");
      Serial.println("❌ JSON recebido não contém a chave 'channel'.");
      client->text("{\"status\":\"error\",\"message\":\"Campo 'channel' obrigatório\"}");
      return;
    }

    const char *channel = doc["channel"];
    Serial.printf("Canal recebido: %s\n", channel);

    // Processar identificação de dispositivo
    if (strcmp(channel, "DEVICE_ID") == 0)
    {
      String deviceId = doc["deviceId"].as<String>();
      String sessionId = doc["sessionId"].as<String>();

      // Verificar se dispositivo já existe
      bool dispositivoExistente = false;
      for (auto &disp : dispositivosConectados)
      {
        if (disp.deviceId == deviceId)
        {
          // Atualizar informações do dispositivo existente
          disp.clientId = client->id();
          disp.sessionId = sessionId;
          disp.lastSeen = millis();
          dispositivoExistente = true;
          Serial.printf("🔄 Dispositivo reconectado: %s (Client #%u)\n", deviceId.c_str(), client->id());
          break;
        }
      }

      if (!dispositivoExistente)
      {
        // Verificar limite de dispositivos (proteção contra overflow)
        if (dispositivosConectados.size() >= MAX_DISPOSITIVOS)
        {
          logErro("Limite de dispositivos atingido (" + String(MAX_DISPOSITIVOS) + ")");
          Serial.printf("⚠️ Limite de dispositivos atingido: %d\n", MAX_DISPOSITIVOS);
          client->text("{\"status\":\"error\",\"message\":\"Limite de dispositivos atingido\"}");
          return;
        }

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
    if (strcmp(channel, "ENVIAR_ROTAS") == 0)
    {
      if (!doc.containsKey("value"))
      {
        logErro("Campo 'value' não encontrado para ENVIAR_ROTAS");
        Serial.println("❌ Erro: 'value' não encontrado para ENVIAR_ROTAS");
        client->text("{\"status\":\"error\",\"message\":\"Campo 'value' obrigatório\"}");
        return;
      }

      if (!doc["value"].is<JsonArray>())
      {
        logErro("Campo 'value' deve ser um array");
        Serial.println("❌ Erro: 'value' deve ser um array");
        client->text("{\"status\":\"error\",\"message\":\"Campo 'value' deve ser array\"}");
        return;
      }

      String deviceId = doc.containsKey("deviceId") ? doc["deviceId"].as<String>() : "unknown";
      JsonArray comandosArray = doc["value"].as<JsonArray>();

      // Validar número de comandos
      if (comandosArray.size() == 0)
      {
        logErro("Rota vazia recebida");
        Serial.println("❌ Erro: Rota não pode estar vazia");
        client->text("{\"status\":\"error\",\"message\":\"Rota não pode estar vazia\"}");
        return;
      }

      if (comandosArray.size() > 100)
      {
        logErro("Rota com muitos comandos: " + String(comandosArray.size()));
        Serial.println("❌ Erro: Rota com muitos comandos");
        client->text("{\"status\":\"error\",\"message\":\"Máximo de 100 comandos por rota\"}");
        return;
      }

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
      for (JsonObject comandoObj : comandosArray)
      {
        // Validar tipo do comando
        if (!comandoObj.containsKey("tipo"))
        {
          logErro("Comando sem campo 'tipo'");
          Serial.println("❌ Erro: Comando sem campo 'tipo'");
          client->text("{\"status\":\"error\",\"message\":\"Comando sem campo 'tipo'\"}");
          return;
        }

        ComandoRota comando;
        comando.tipo = comandoObj["tipo"].as<String>();

        if (comando.tipo == "MOVE")
        {
          // Validar campo valor
          if (!comandoObj.containsKey("valor") || !comandoObj["valor"].is<int>())
          {
            logErro("Comando MOVE sem 'valor' válido");
            Serial.println("❌ Erro: Comando MOVE sem 'valor' válido");
            client->text("{\"status\":\"error\",\"message\":\"Comando MOVE precisa de 'valor' inteiro\"}");
            return;
          }

          int valor = comandoObj["valor"];

          // Validar limites
          if (valor <= 0 || valor > MAX_DISTANCIA)
          {
            logErro("Valor de distância inválido: " + String(valor));
            Serial.printf("❌ Erro: Distância deve estar entre 1 e %d cm\n", MAX_DISTANCIA);
            client->text("{\"status\":\"error\",\"message\":\"Distância inválida\"}");
            return;
          }

          comando.valor = valor;
          distanciaTotal += valor; // Acumula a distância
          Serial.printf("  - MOVE: %d\n", comando.valor);
        }
        else if (comando.tipo == "ROTATE")
        {
          // Validar campo angulo
          if (!comandoObj.containsKey("angulo") || !comandoObj["angulo"].is<int>())
          {
            logErro("Comando ROTATE sem 'angulo' válido");
            Serial.println("❌ Erro: Comando ROTATE sem 'angulo' válido");
            client->text("{\"status\":\"error\",\"message\":\"Comando ROTATE precisa de 'angulo' inteiro\"}");
            return;
          }

          int angulo = comandoObj["angulo"];

          // Validar limites
          if (angulo <= 0 || angulo > MAX_ANGULO)
          {
            logErro("Valor de ângulo inválido: " + String(angulo));
            Serial.printf("❌ Erro: Ângulo deve estar entre 1 e %d graus\n", MAX_ANGULO);
            client->text("{\"status\":\"error\",\"message\":\"Ângulo inválido\"}");
            return;
          }

          // Validar direção
          if (!comandoObj.containsKey("direcao"))
          {
            logErro("Comando ROTATE sem 'direcao'");
            Serial.println("❌ Erro: Comando ROTATE sem 'direcao'");
            client->text("{\"status\":\"error\",\"message\":\"Comando ROTATE precisa de 'direcao'\"}");
            return;
          }

          String direcao = comandoObj["direcao"].as<String>();
          if (direcao != "direita" && direcao != "esquerda")
          {
            logErro("Direção inválida: " + direcao);
            Serial.println("❌ Erro: Direção deve ser 'direita' ou 'esquerda'");
            client->text("{\"status\":\"error\",\"message\":\"Direção deve ser 'direita' ou 'esquerda'\"}");
            return;
          }

          comando.valor = angulo;
          comando.direcao = direcao;
          Serial.printf("  - ROTATE: %d° para %s\n", comando.valor, comando.direcao.c_str());
        }
        else
        {
          logErro("Tipo de comando inválido: " + comando.tipo);
          Serial.printf("❌ Erro: Tipo de comando inválido: %s\n", comando.tipo.c_str());
          client->text("{\"status\":\"error\",\"message\":\"Tipo de comando deve ser MOVE ou ROTATE\"}");
          return;
        }

        novaRota.comandos.push_back(comando);
      }

      // Definir a distância total calculada como destino
      definirDistanciaDestino(distanciaTotal);
      Serial.printf("📏 Distância total da rota: %.2f cm\n", distanciaTotal);

      // Verificar limite de rotas e aplicar FIFO se necessário
      if (rotasArmazenadas.size() >= MAX_ROTAS)
      {
        Serial.printf("⚠️ Limite de rotas atingido (%d). Removendo rota mais antiga.\n", MAX_ROTAS);
        rotasArmazenadas.erase(rotasArmazenadas.begin()); // Remove a primeira (mais antiga)
      }

      // Adicionar rota ao armazenamento
      rotasArmazenadas.push_back(novaRota);
      Serial.printf("✅ Rota armazenada! Total de rotas: %d\n", rotasArmazenadas.size());

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
      for (const auto &cmd : novaRota.comandos)
      {
        JsonObject elemObj = elementosArray.createNestedObject();
        elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
        elemObj["valor"] = cmd.valor;
        elemObj["id"] = millis() + random(1000);
        if (cmd.tipo == "ROTATE")
        {
          elemObj["direcao"] = cmd.direcao;
        }
      }

      String notifString;
      serializeJson(notifDoc, notifString);
      ws.textAll(notifString);
    }

    // Processar comando para limpar rotas
    else if (strcmp(channel, "LIMPAR_ROTAS") == 0)
    {
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
    else if (strcmp(channel, "LISTAR_ROTAS") == 0)
    {
      Serial.println("=== Rotas Armazenadas ===");
      Serial.printf("Total: %d rotas\n", rotasArmazenadas.size());

      for (size_t i = 0; i < rotasArmazenadas.size(); i++)
      {
        Serial.printf("\nRota %d (ID: %lu):\n", i + 1, rotasArmazenadas[i].id);
        Serial.printf("  Comandos: %d\n", rotasArmazenadas[i].comandos.size());

        for (size_t j = 0; j < rotasArmazenadas[i].comandos.size(); j++)
        {
          ComandoRota cmd = rotasArmazenadas[i].comandos[j];
          if (cmd.tipo == "MOVE")
          {
            Serial.printf("    %d. MOVE %d\n", j + 1, cmd.valor);
          }
          else
          {
            Serial.printf("    %d. ROTATE %d° %s\n", j + 1, cmd.valor, cmd.direcao.c_str());
          }
        }
      }
      Serial.println("=========================\n");
    }

    // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

    // Processar comando para ABRIR A PORTA
    else if (strcmp(channel, "ABRIR") == 0)
    {
      Serial.println("Comando: ABRIR. Iniciando abertura...");
      estadoPorta = ESTADO_PORTA_ABRINDO; // Muda o estado
      tempoInicioMovimento = millis();    // Marca o tempo de início
      client->text("{\"status\":\"ok\",\"message\":\"Comando 'ABRIR' recebido. Abrindo...\"}");
    }

    // Processar comando para FECHAR A PORTA
    else if (strcmp(channel, "FECHAR") == 0)
    {
      Serial.println("Comando: FECHAR. Iniciando fechamento...");
      estadoPorta = ESTADO_PORTA_FECHANDO; // Muda o estado
      tempoInicioMovimento = millis();     // Marca o tempo de início
      client->text("{\"status\":\"ok\",\"message\":\"Comando 'FECHAR' recebido. Fechando...\"}");
    }

    // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

    // Processar comando para DEFINIR DISTÂNCIA DESTINO
    else if (strcmp(channel, "DEFINIR_DISTANCIA") == 0)
    {
      if (doc.containsKey("value"))
      {
        float distancia = doc["value"];
        definirDistanciaDestino(distancia);
        client->text("{\"status\":\"ok\",\"message\":\"Distância até destino definida\"}");
      }
      else
      {
        Serial.println("Erro: 'value' não encontrado para DEFINIR_DISTANCIA");
        client->text("{\"status\":\"error\",\"message\":\"Valor não encontrado\"}");
      }
    }

    // Comando genérico
    else
    {
      float value = doc["value"];
      Serial.printf("Valor recebido: %f\n", value);
    }
  }
}

String setupVariables(const String &var)
{
  if (var == "VARIAVEL1")
  { // Ai coloa %VARIAVEL1% no HTML, que ai vai ser substituida
    return "Valor da variável";
  }
  return String(); // para não crashar se nao existir a variável
}
void notifyClients(String value)
{
  ws.textAll(String(value));
}

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);

  Serial.println("\n\n=================================");
  Serial.println("    CEGOINHA ESP32 - Iniciando");
  Serial.println("=================================");

  // Configurar Watchdog Timer
  Serial.println("\n--- Configurando Watchdog Timer ---");
  esp_task_wdt_init(WATCHDOG_TIMEOUT, true); // timeout em segundos, enable panic
  esp_task_wdt_add(NULL);                    // adicionar task atual
  Serial.printf("✅ Watchdog Timer configurado: %d segundos\n", WATCHDOG_TIMEOUT);
  Serial.println("-----------------------------------\n");

  // Inicializar LittleFS
  Serial.println("\n--- Inicializando LittleFS ---");
  if (!LittleFS.begin(true))
  {
    Serial.println("❌ Erro ao montar LittleFS");
    Serial.println("⚠️ Sistema continuará sem persistência");
  }
  else
  {
    Serial.println("✅ LittleFS montado com sucesso");

    // Mostrar informações do sistema de arquivos
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("📊 Espaço total: %d bytes\n", totalBytes);
    Serial.printf("📊 Espaço usado: %d bytes (%.1f%%)\n", usedBytes, (usedBytes * 100.0) / totalBytes);
  }
  Serial.println("------------------------------\n");

  // ===== INÍCIO: CÓDIGO DO ENCODER =====
  Serial.println("--- Setup do Encoder ---");
  // Configura os pinos do encoder como entrada
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);

  // Configura a interrupção no canal A do encoder
  // FALLING = detecta quando o sinal muda de HIGH para LOW
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), encoderISR, FALLING);

  // Inicializa variáveis de tempo
  ultimoTempoCalculo = millis();
  ultimoTempoCalculoETA = millis();

  Serial.println("✅ Encoder configurado nos pinos IO34 e IO35");
  Serial.println("✅ Interrupção anexada ao canal A");
  Serial.println("------------------------------\n");
  // ===== FIM: CÓDIGO DO ENCODER =====

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
  Serial.println("--- Setup do Motor da Porta ---");
  // --- Setup do Motor ---
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  // IMPORTANTE: Conecte o pino ENA do L298N direto ao VCC (5V ou 12V)
  // para que o motor sempre tenha potência máxima

  // Garante que o motor comece parado
  pararPorta();
  Serial.println("✅ Driver L298N (Porta) configurado.");
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
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(404, "text/plain", "Arquivo não encontrado. Faça upload dos arquivos da pasta data/ para o LittleFS."); });

  // Start server
  server.begin();

  Serial.println("Servidor Web iniciado!");
  Serial.println("Aguardando conexões...\n");
  Serial.println("=================================\n");
}

void loop()
{
  // Resetar watchdog timer a cada iteração
  esp_task_wdt_reset();

  ws.cleanupClients();

  // ===== INÍCIO: CÓDIGO DO ENCODER =====
  // Calcula e envia a velocidade periodicamente
  calcularEEnviarVelocidade();
  // ===== FIM: CÓDIGO DO ENCODER =====

  // ===== INÍCIO: CÓDIGO DO ETA =====
  // Calcula e envia o ETA periodicamente
  calcularEEnviarETA();
  // ===== FIM: CÓDIGO DO ETA =====

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

  // --- MÁQUINA DE ESTADOS DO MOTOR DA PORTA ---
  // Esta parte roda continuamente, verificando o estado da porta
  // sem usar 'delay()' ou 'while()', permitindo que o WebSocket
  // e o servidor web continuem funcionando.

  switch (estadoPorta)
  {

  case ESTADO_PORTA_ABRINDO:
  {
    unsigned long tempoDecorrido = millis() - tempoInicioMovimento;

    // Verifica se o tempo de abertura foi atingido
    if (tempoDecorrido >= TEMPO_ABERTURA_MS)
    {
      Serial.println("✅ Tempo de abertura atingido. Segurando posição.");
      segurarPosicaoPorta(); // Segura a posição
      estadoPorta = ESTADO_PORTA_SEGURANDO;
      tempoInicioSegurar = millis(); // Marca quando começou a segurar
      ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"ABERTA\"}");
    }
    // Caso contrário, continua abrindo
    else
    {
      abrirPortaLogica();
    }
  }
  break;

  case ESTADO_PORTA_FECHANDO:
  {
    unsigned long tempoDecorrido = millis() - tempoInicioMovimento;

    // Verifica se o tempo de fechamento foi atingido
    if (tempoDecorrido >= TEMPO_FECHAMENTO_MS)
    {
      Serial.println("✅ Tempo de fechamento atingido. Segurando posição.");
      segurarPosicaoPorta(); // Segura a posição
      estadoPorta = ESTADO_PORTA_SEGURANDO;
      tempoInicioSegurar = millis(); // Marca quando começou a segurar
      ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"FECHADA\"}");
    }
    // Caso contrário, continua fechando
    else
    {
      fecharPortaLogica();
    }
  }
  break;

  case ESTADO_PORTA_SEGURANDO:
  {
    // Verificar timeout para economizar energia
    unsigned long tempoSegurando = millis() - tempoInicioSegurar;

    if (tempoSegurando >= TEMPO_SEGURAR_MS)
    {
      Serial.println("⏱️ Timeout de segurar atingido. Parando motor para economizar energia.");
      pararPorta();
      estadoPorta = ESTADO_PORTA_PARADO;
      ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"PARADO\"}");
    }
    else
    {
      // Mantém a posição ativa (freio do motor)
      segurarPosicaoPorta();
    }
  }
  break;

  case ESTADO_PORTA_PARADO:
    // Motor completamente desligado
    pararPorta();
    break;
  }
  // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
}

// Função auxiliar para obter informações de uma rota específica
String getRotaInfo(size_t indice)
{
  if (indice >= rotasArmazenadas.size())
  {
    return "Rota não encontrada";
  }

  Rota rota = rotasArmazenadas[indice];
  String info = "Rota ID: " + String(rota.id) + "\n";
  info += "Comandos: " + String(rota.comandos.size()) + "\n";

  for (size_t i = 0; i < rota.comandos.size(); i++)
  {
    ComandoRota cmd = rota.comandos[i];
    info += "  " + String(i + 1) + ". ";
    if (cmd.tipo == "MOVE")
    {
      info += "MOVE " + String(cmd.valor) + "\n";
    }
    else
    {
      info += "ROTATE " + String(cmd.valor) + "° " + cmd.direcao + "\n";
    }
  }

  return info;
}