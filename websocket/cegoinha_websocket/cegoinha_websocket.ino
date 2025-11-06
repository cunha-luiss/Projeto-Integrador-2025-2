#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "driver/pcnt.h"

const char *ssid = "cegoinha";
const char *password = "cegoinha123";

#define ROTAS_FILE "/rotas.json"
// andar

// Define os pinos para o Motor esquerdo
#define FRENTE_ESQ 14
#define TRAS_ESQ 27
#define ENC_A_ESQ 34
#define ENC_B_ESQ 35

// Define os pinos para o Motor direito
#define FRENTE_DIR 26
#define TRAS_DIR 25
#define ENC_A_DIR 33
#define ENC_B_DIR 32

// para contar pulsos
volatile int64_t total_pulsos_esq = 0;
volatile int64_t total_pulsos_dir = 0;

// --- "Bandeiras" (Flags) de Meta ---
volatile bool meta_esq_atingida = false;
volatile bool meta_dir_atingida = false;

int64_t META_PULSOS = 0;

// --- Variáveis para o Timer de Impressão ---
unsigned long tempoPrintAnterior = 0;
const unsigned long intervaloPrint = 250; // Imprime a cada 250ms (4x por segundo)

// ===== INÍCIO: CÓDIGO DO ENCODER E VELOCIDADE =====

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
#define PIN_FRENTE_ESQ 36
#define PIN_TRAS_ESQ 37
// Pino ENA deve estar conectado direto ao 5V ou 12V para velocidade máxima

// --- Configuração de Tempo da Porta ---
#define TEMPO_ABERTURA_MS 3000   // Tempo para abrir completamente (3 segundos)
#define TEMPO_FECHAMENTO_MS 3000 // Tempo para fechar completamente (3 segundos)

// --- Controle de Estado da Porta (Lógica Não-Bloqueante) ---
#define ESTADO_PORTA_PARADO 0
#define ESTADO_PORTA_ABRINDO 1
#define ESTADO_PORTA_FECHANDO 2
#define ESTADO_PORTA_SEGURANDO 3 // Novo estado: segurar posição

int estadoPorta = ESTADO_PORTA_PARADO;  // Estado atual da porta
unsigned long tempoInicioMovimento = 0; // Marca quando o movimento começou

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

// Salvar rotas no LittleFS
void salvarRotasLittleFS()
{
  DynamicJsonDocument doc(8192);
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
    Serial.println("❌ Erro ao abrir arquivo para escrita");
    return;
  }

  serializeJson(doc, file);
  file.close();
  Serial.printf("💾 %d rotas salvas no LittleFS\n", rotasArmazenadas.size());
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
    Serial.println("❌ Erro ao abrir arquivo para leitura");
    return;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
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
  DynamicJsonDocument doc(8192);
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
  digitalWrite(PIN_FRENTE_ESQ, LOW);
  digitalWrite(PIN_TRAS_ESQ, LOW);
}

// Gira em um sentido (Ex: Abrir)
void abrirPortaLogica()
{
  digitalWrite(PIN_FRENTE_ESQ, HIGH);
  digitalWrite(PIN_TRAS_ESQ, LOW);
}

// Gira no outro sentido (Ex: Fechar)
void fecharPortaLogica()
{
  digitalWrite(PIN_FRENTE_ESQ, LOW);
  digitalWrite(PIN_TRAS_ESQ, HIGH);
}

// Segura a posição (freio do motor - ambos HIGH)
void segurarPosicaoPorta()
{
  digitalWrite(PIN_FRENTE_ESQ, HIGH);
  digitalWrite(PIN_TRAS_ESQ, HIGH);
}
// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

// ===== INÍCIO: FUNÇÕES DO ENCODER =====

// Função para calcular e enviar a velocidade
void calcularEEnviarVelocidade()
{
  unsigned long tempoAtual = millis();

  // Verifica se já passou o intervalo de cálculo
  if (tempoAtual - ultimoTempoCalculo >= intervaloCalculo)
  {
    // Calcula o tempo decorrido em segundos
    float tempoDecorrido = (tempoAtual - ultimoTempoCalculo) / 1000.0;

    // Desabilita interrupções temporariamente para ler o contador
    noInterrupts();
    long pulsos = contadorPulsos;
    contadorPulsos = 0; // Reseta o contador
    interrupts();

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
    // DESCOMENTAR
    // Serial.printf("Velocidade: %.2f cm/s\n", abs(velocidadeAtual));

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
// DESCOMENTAR DPS
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
      // Serial.printf("ETA: %.2f segundos (Distância restante: %.2f cm, Velocidade: %.2f cm/s)\n",
      //               etaSegundos, distanciaRestante, abs(velocidadeAtual));
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
// funcoes de andar
void moverMotorA(int direcao)
{
  if (direcao == 1)
  {
    digitalWrite(FRENTE_ESQ, HIGH);
    digitalWrite(TRAS_ESQ, LOW);
  }
  else if (direcao == -1)
  {
    digitalWrite(FRENTE_ESQ, LOW);
    digitalWrite(TRAS_ESQ, HIGH);
  }
  else
  {
    digitalWrite(FRENTE_ESQ, LOW);
    digitalWrite(TRAS_ESQ, LOW);
  }
}

/**
 * Controla o Motor B
 * direcao: 1 (frente), -1 (trás), 0 (parar/frear)
 */
void moverMotorB(int direcao)
{
  if (direcao == 1)
  {
    digitalWrite(FRENTE_DIR, HIGH);
    digitalWrite(TRAS_DIR, LOW);
  }
  else if (direcao == -1)
  {
    digitalWrite(FRENTE_DIR, LOW);
    digitalWrite(TRAS_DIR, HIGH);
  }
  else
  {
    digitalWrite(FRENTE_DIR, LOW);
    digitalWrite(TRAS_DIR, LOW);
  }
}

// Função auxiliar para parar tudo
void pararMotores()
{
  moverMotorA(0);
  moverMotorB(0);
}

void executarRota(JsonArray elementosArray, Rota novaRota)
{
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
    else if (cmd.tipo == "MOVE")
    {
      META_PULSOS = cmd.valor * 1000;
      total_pulsos_esq = 0;
      total_pulsos_dir = 0;
      
      moverMotorA(1);
      moverMotorB(1);

      Serial.printf("andou\n");

    }
  }
}

void mensagemRecebida(AsyncWebSocketClient *client, void *metadados, uint8_t *mensagem, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)metadados;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  { // verifica se recebe só texto

    // Aumentar tamanho do buffer JSON para acomodar rotas maiores
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, mensagem, len);

    if (error)
    {
      Serial.print("Falha ao ler JSON: ");
      Serial.println(error.c_str());
      return;
    }

    if (!doc.containsKey("channel"))
    {
      Serial.println("JSON recebido não contém a chave 'channel'.");
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
      for (JsonObject comandoObj : comandosArray)
      {
        ComandoRota comando;
        comando.tipo = comandoObj["tipo"].as<String>();

        if (comando.tipo == "MOVE")
        {
          comando.valor = comandoObj["valor"];
          distanciaTotal += comando.valor; // Acumula a distância
          Serial.printf("  - MOVE: %d\n", comando.valor);
        }
        else if (comando.tipo == "ROTATE")
        {
          comando.valor = comandoObj["angulo"];
          comando.direcao = comandoObj["direcao"].as<String>();
          Serial.printf("  - ROTATE: %d° para %s\n", comando.valor, comando.direcao.c_str());
        }

        novaRota.comandos.push_back(comando);
      }

      // Definir a distância total calculada como destino
      definirDistanciaDestino(distanciaTotal);
      Serial.printf("📏 Distância total da rota: %.2f cm\n", distanciaTotal);

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

      executarRota(elementosArray, novaRota);
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

void configuraEncoderEsquerdoPCNT()
{
  pcnt_config_t configEncoder = {};
  configEncoder.pulse_gpio_num = ENC_A_ESQ;
  configEncoder.ctrl_gpio_num = ENC_B_ESQ;
  configEncoder.channel = PCNT_CHANNEL_0;
  configEncoder.unit = PCNT_UNIT_0;
  configEncoder.pos_mode = PCNT_COUNT_INC;
  configEncoder.neg_mode = PCNT_COUNT_DIS;
  configEncoder.hctrl_mode = PCNT_MODE_KEEP;
  configEncoder.lctrl_mode = PCNT_MODE_REVERSE;
  pcnt_unit_config(&configEncoder);
  pcnt_set_filter_value(PCNT_UNIT_0, 1023);
  pcnt_filter_enable(PCNT_UNIT_0);
  pcnt_counter_pause(PCNT_UNIT_0);
  pcnt_counter_clear(PCNT_UNIT_0);
  pcnt_counter_resume(PCNT_UNIT_0);
}

void configuraEncoderDireitoPCNT()
{
  pcnt_config_t configEncoder = {};
  configEncoder.pulse_gpio_num = ENC_A_DIR;
  configEncoder.ctrl_gpio_num = ENC_B_DIR;
  configEncoder.channel = PCNT_CHANNEL_0;
  configEncoder.unit = PCNT_UNIT_1;
  configEncoder.pos_mode = PCNT_COUNT_INC;
  configEncoder.neg_mode = PCNT_COUNT_DIS;
  configEncoder.hctrl_mode = PCNT_MODE_KEEP;
  configEncoder.lctrl_mode = PCNT_MODE_REVERSE;
  pcnt_unit_config(&configEncoder);
  pcnt_set_filter_value(PCNT_UNIT_1, 1023);
  pcnt_filter_enable(PCNT_UNIT_1);
  pcnt_counter_pause(PCNT_UNIT_1);
  pcnt_counter_clear(PCNT_UNIT_1);
  pcnt_counter_resume(PCNT_UNIT_1);
}

void setup()
{
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Inicializar LittleFS
  Serial.println("\n--- Inicializando LittleFS ---");
  if (!LittleFS.begin(true))
  {
    Serial.println("❌ Erro ao montar LittleFS");
    Serial.println("⚠ Sistema continuará sem persistência");
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

  Serial.println("✅ Encoder configurado nos pinos IO34 e IO35");
  Serial.println("✅ Interrupção anexada ao canal A");
  Serial.println("------------------------------\n");
  // ===== FIM: CÓDIGO DO ENCODER =====

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
  Serial.println("--- Setup do Motor da Porta ---");
  // --- Setup do Motor ---
  pinMode(PIN_FRENTE_ESQ, OUTPUT);
  pinMode(PIN_TRAS_ESQ, OUTPUT);
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

  // andar
  pinMode(FRENTE_ESQ, OUTPUT);
  pinMode(TRAS_ESQ, OUTPUT);
  pinMode(FRENTE_DIR, OUTPUT);
  pinMode(TRAS_DIR, OUTPUT);

  // ENCODER

  configuraEncoderEsquerdoPCNT();
  configuraEncoderDireitoPCNT();
}

void loop()
{
  ws.cleanupClients();

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
      Serial.println("Tempo de abertura atingido. Segurando posição.");
      segurarPosicaoPorta(); // Segura a posição
      estadoPorta = ESTADO_PORTA_SEGURANDO;
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
      Serial.println("Tempo de fechamento atingido. Segurando posição.");
      segurarPosicaoPorta(); // Segura a posição
      estadoPorta = ESTADO_PORTA_SEGURANDO;
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
    // Mantém a posição ativa (freio do motor)
    // O motor fica energizado segurando a porta na posição
    segurarPosicaoPorta();
    break;

  case ESTADO_PORTA_PARADO:
    // Motor completamente desligado
    pararPorta();
    break;
  }
  // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

  // 1. Variáveis para guardar as leituras parciais
  int16_t parcial_esq = 0;
  int16_t parcial_dir = 0;

  // --- Leitura Atômica do Encoder ESQUERDO ---
  if (!meta_esq_atingida)
  {
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_get_counter_value(PCNT_UNIT_0, &parcial_esq);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);

    if (parcial_esq != 0)
    {
      total_pulsos_esq += (int64_t)parcial_esq;
    }
  }

  // --- Leitura Atômica do Encoder DIREITO ---
  if (!meta_dir_atingida)
  {
    pcnt_counter_pause(PCNT_UNIT_1);
    pcnt_get_counter_value(PCNT_UNIT_1, &parcial_dir);
    pcnt_counter_clear(PCNT_UNIT_1);
    pcnt_counter_resume(PCNT_UNIT_1);

    if (parcial_dir != 0)
    {
      total_pulsos_dir += (int64_t)parcial_dir;
    }
  }

  // --- Lógica de Parada (individual) ---
  if (META_PULSOS != 0) {
  if ((total_pulsos_esq >= META_PULSOS) && (!meta_esq_atingida))
  {
    meta_esq_atingida = true;
    digitalWrite(FRENTE_ESQ, LOW);
    Serial.println(">>> META ESQUERDA ATINGIDA! <<<");
    ws.textAll(">>> META ESQUERDA ATINGIDA! <<<");
  }

  if ((total_pulsos_dir >= META_PULSOS) && (!meta_dir_atingida))
  {
    meta_dir_atingida = true;
    digitalWrite(FRENTE_DIR, LOW);
    Serial.println(">>> META DIREITA ATINGIDA! <<<");
    ws.textAll(">>> META DIREITA ATINGIDA! <<<");
  }

  // --- Verificação Final ---
  if (meta_esq_atingida && meta_dir_atingida)
  {
    //prepara para próxima rota
    META_PULSOS = 0;
    meta_dir_atingida = false;
    meta_esq_atingida = false;
    total_pulsos_dir = 0;
    total_pulsos_esq = 0;
    parcial_dir = 0;
    parcial_esq = 0;
  }
  }
  // --- Bloco de Impressão (Debug) ---
  unsigned long tempoAtual = millis();
  if (tempoAtual - tempoPrintAnterior >= intervaloPrint)
  {
    tempoPrintAnterior = tempoAtual; // Reinicia o timer de print

    // Usa Serial.printf() para formatar a saída.
    // %lld é o especificador para 'long long int' (que é o int64_t)
    // O '\n' no final significa "pular linha"
    Serial.printf("ESQ: %lld | DIR: %lld \n",
                  total_pulsos_esq,
                  total_pulsos_dir);
    // ...existing code...
    ws.textAll("ESQ: " + String(total_pulsos_esq) + " | DIR: " + String(total_pulsos_dir) + " \n");
  }
}