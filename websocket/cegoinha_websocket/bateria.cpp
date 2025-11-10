#include <Arduino.h>  // Necessário para funções Arduino (embora não usemos aqui)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "motores_andar.h"
#include "structures_projeto.h"
#include "dados_littlefs.h"
#include "driver/pcnt.h"

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

//websocket
static AsyncWebSocket *ws = nullptr;

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
  ws->textAll(jsonString);
}

/**
 * Verifica nível crítico da bateria
 */
void verificarBateriaCritica() {
  if (batterySOC <= 10.0) {
    Serial.println("⚠️ BATERIA CRÍTICA! SOC: " + String(batterySOC) + "%");
    
    // Para o carrinho se estiver em movimento
    if (passoRota() >= 0) {
      pararMotores();
      
      // Notifica clientes
      ws->textAll("{\"channel\":\"BATTERY_CRITICAL\",\"status\":\"stopped\",\"message\":\"Bateria crítica! Carrinho parado.\"}");
    }
  } else if (batterySOC <= 20.0) {
    // Apenas aviso
    Serial.println("⚠️ Bateria baixa: " + String(batterySOC) + "%");
  }
}

void setupBateria(AsyncWebSocket *pws){
  ws = pws;

  // ===== INÍCIO: CONFIGURAÇÃO ADC BATERIA =====
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
  // ===== FIM: CONFIGURAÇÃO ADC BATERIA =====
}