#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "driver/pcnt.h"
#include "structures_projeto.h"
#include "motores_andar.h"
#include "dados_littlefs.h"
#include "bateria.h"


const char *ssid = "cegoinha";
const char *password = "cegoinha123";

unsigned long lastBatteryUpdate = 0;
const unsigned long BATTERY_UPDATE_INTERVAL = 60000; // Atualiza a cada 1 min

// para contar pulsos
volatile int64_t total_pulsos_esq = 0;
volatile int64_t total_pulsos_dir = 0;

// --- "Bandeiras" (Flags) de Meta ---
volatile bool meta_esq_atingida = false;
volatile bool meta_dir_atingida = false;

int64_t META_PULSOS = 0;

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

// ===== INÍCIO: CÓDIGO DO ENCODER E VELOCIDADE =====

// --- Parâmetros do Motor e Encoder ---
#define PULSOS_POR_REVOLUCAO 20.0  // Número de pulsos por revolução do encoder (ajuste conforme seu motor)
#define DIAMETRO_RODA 6.5          // Diâmetro da roda em cm (ajuste conforme seu carrinho)
#define PI 3.14159265359

// --- Variáveis de Contagem do Encoder ---
volatile long contadorPulsos = 0;  // Contador de pulsos do encoder

// --- Variáveis para Cálculo de Velocidade ---
unsigned long ultimoTempoCalculo = 0;
const unsigned long intervaloCalculo = 100;  // Calcular velocidade a cada 100ms
float velocidadeAtual = 0.0;                 // Velocidade em cm/s

// --- Variáveis para Cálculo de ETA ---
float distanciaDestino = 0.0;     // Distância total até o destino em cm
float distanciaPercorrida = 0.0;  // Distância acumulada percorrida desde o início da rota em cm
unsigned long ultimoTempoCalculoETA = 0;
const unsigned long intervaloCalculoETA = 500;  // Calcular ETA a cada 500ms
float etaSegundos = 0.0;                        // ETA em segundos

// ===== FIM: CÓDIGO DO ENCODER E VELOCIDADE =====

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

// --- Pinos do Motor da Porta (L298N) ---
// Mude estes pinos conforme a sua ligação real
#define PIN_FRENTE_ESQ 36
#define PIN_TRAS_ESQ 37
// Pino ENA deve estar conectado direto ao 5V ou 12V para velocidade máxima

// --- Configuração de Tempo da Porta ---
#define TEMPO_ABERTURA_MS 3000    // Tempo para abrir completamente (3 segundos)
#define TEMPO_FECHAMENTO_MS 3000  // Tempo para fechar completamente (3 segundos)

// --- Controle de Estado da Porta (Lógica Não-Bloqueante) ---
#define ESTADO_PORTA_PARADO 0
#define ESTADO_PORTA_ABRINDO 1
#define ESTADO_PORTA_FECHANDO 2
#define ESTADO_PORTA_SEGURANDO 3  // Novo estado: segurar posição

int estadoPorta = ESTADO_PORTA_PARADO;   // Estado atual da porta
unsigned long tempoInicioMovimento = 0;  // Marca quando o movimento começou

// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

// Estrutura para armazenar informações do dispositivo conectado


// Mapa de dispositivos conectados
std::vector<DispositivoConectado> dispositivosConectados;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML, CSS e JS agora são servidos do LittleFS (pasta data/)
// Para fazer upload dos arquivos para a placa:
// 1. Arduino IDE: Instale o plugin "ESP32 Sketch Data Upload"
// Os arquivos em cegoinha_websocket/data/ serão enviados para o LittleFS

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
// --- Funções de Baixo Nível do Motor da Porta ---

// Para o motor completamente (desliga)
void pararPorta() {
  digitalWrite(PIN_FRENTE_ESQ, LOW);
  digitalWrite(PIN_TRAS_ESQ, LOW);
}

// Gira em um sentido (Ex: Abrir)
void abrirPortaLogica() {
  digitalWrite(PIN_FRENTE_ESQ, HIGH);
  digitalWrite(PIN_TRAS_ESQ, LOW);
}

// Gira no outro sentido (Ex: Fechar)
void fecharPortaLogica() {
  digitalWrite(PIN_FRENTE_ESQ, LOW);
  digitalWrite(PIN_TRAS_ESQ, HIGH);
}

// Segura a posição (freio do motor - ambos HIGH)
void segurarPosicaoPorta() {
  digitalWrite(PIN_FRENTE_ESQ, HIGH);
  digitalWrite(PIN_TRAS_ESQ, HIGH);
}
// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====


// ===== INÍCIO: FUNÇÕES DO ETA =====

// Função para definir a distância até o destino
void definirDistanciaDestino(float distancia) {
  distanciaDestino = distancia;
  distanciaPercorrida = 0.0;  // Reseta a distância percorrida ao definir novo destino
  Serial.printf("Distância até destino definida: %.2f cm\n", distanciaDestino);
}

// ===== FIM: FUNÇÕES DO ETA =====

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
      void apagarRotas();

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
      Serial.println("Comando: ABRIR. Iniciando abertura...");
      estadoPorta = ESTADO_PORTA_ABRINDO;  // Muda o estado
      tempoInicioMovimento = millis();     // Marca o tempo de início
      client->text("{\"status\":\"ok\",\"message\":\"Comando 'ABRIR' recebido. Abrindo...\"}");
    }

    // Processar comando para FECHAR A PORTA
    else if (strcmp(channel, "FECHAR") == 0) {
      Serial.println("Comando: FECHAR. Iniciando fechamento...");
      estadoPorta = ESTADO_PORTA_FECHANDO;  // Muda o estado
      tempoInicioMovimento = millis();      // Marca o tempo de início
      client->text("{\"status\":\"ok\",\"message\":\"Comando 'FECHAR' recebido. Fechando...\"}");
    }

    // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

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
  setupLittleFS(&rotasArmazenadas);
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
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Arquivo não encontrado. Faça upload dos arquivos da pasta data/ para o LittleFS.");
  });

  // Start server
  server.begin();

  Serial.println("Servidor Web iniciado!");
  Serial.println("Aguardando conexões...\n");
  Serial.println("=================================\n");


  //andar
  motoresSetup(&ws, &PASSO_ROTA, &movimento, &META_PULSOS, &ROTA_ATUAL, &total_pulsos_esq, &total_pulsos_dir);

  setupBateria(&ws);

  // ENCODER

  configuraEncoderEsquerdoPCNT();
  configuraEncoderDireitoPCNT();
}

void loop() {
  ws.cleanupClients();

  // ===== MONITORAMENTO DE BATERIA =====
  unsigned long currentTime = millis();
  if (currentTime - lastBatteryUpdate >= BATTERY_UPDATE_INTERVAL) {
    lastBatteryUpdate = currentTime;
    
    atualizarBateria();
    enviarDadosBateria();
    verificarBateriaCritica();
  }
  // ===== FIM MONITORAMENTO DE BATERIA =====

  // --- MÁQUINA DE ESTADOS DO MOTOR DA PORTA ---
  // Esta parte roda continuamente, verificando o estado da porta
  // sem usar 'delay()' ou 'while()', permitindo que o WebSocket
  // e o servidor web continuem funcionando.

  switch (estadoPorta) {

    case ESTADO_PORTA_ABRINDO:
      {
        unsigned long tempoDecorrido = millis() - tempoInicioMovimento;

        // Verifica se o tempo de abertura foi atingido
        if (tempoDecorrido >= TEMPO_ABERTURA_MS) {
          Serial.println("Tempo de abertura atingido. Segurando posição.");
          segurarPosicaoPorta();  // Segura a posição
          estadoPorta = ESTADO_PORTA_SEGURANDO;
          ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"ABERTA\"}");
        }
        // Caso contrário, continua abrindo
        else {
          abrirPortaLogica();
        }
      }
      break;

    case ESTADO_PORTA_FECHANDO:
      {
        unsigned long tempoDecorrido = millis() - tempoInicioMovimento;

        // Verifica se o tempo de fechamento foi atingido
        if (tempoDecorrido >= TEMPO_FECHAMENTO_MS) {
          Serial.println("Tempo de fechamento atingido. Segurando posição.");
          segurarPosicaoPorta();  // Segura a posição
          estadoPorta = ESTADO_PORTA_SEGURANDO;
          ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"FECHADA\"}");
        }
        // Caso contrário, continua fechando
        else {
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
        moverMotorEsq(0);
        Serial.println(">>> META ESQUERDA ATINGIDA! <<<");
        ws.textAll(">>> META ESQUERDA ATINGIDA! <<<");
      }
      if (meta_esq_atingida) {
        // prepara para próxima rota
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
        moverMotorDir(0);
        Serial.println(">>> META DIREITA ATINGIDA! <<<");
        ws.textAll(">>> META DIREITA ATINGIDA! <<<");
      }
      if (meta_dir_atingida) {
        // prepara para próxima rota
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

    // Usa Serial.printf() para formatar a saída.
    // %lld é o especificador para 'long long int' (que é o int64_t)
    // O '\n' no final significa "pular linha"
    Serial.printf("ESQ: %lld | DIR: %lld \n",
                  total_pulsos_esq,
                  total_pulsos_dir);
    // ...existing code...
    ws.textAll("ESQ: " + String(total_pulsos_esq) + " | DIR: " + String(total_pulsos_dir) + " \n");
  }
  tempoAtual = millis();
  if (aguardandoProximoComando && (tempoAtual - tempoTerminoComandoAnterior >= intervaloEsperaEntreComandos)) {
    aguardandoProximoComando = false;
    ws.textAll(String(intervaloEsperaEntreComandos) + " segundos esperados");
    proximoComando(ROTA_ATUAL);
  }
}


//TESTAR SE TEM COMO ENVIAR UMA ROTA ENQUANTO ELA ESTÁ EM EXECUÇÃO

//tirou trens do ETA e da Velocidade