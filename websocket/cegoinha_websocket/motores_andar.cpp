#include <Arduino.h>  // Necessário para funções Arduino (embora não usemos aqui)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "motores_andar.h" // Inclui as declarações do .h
#include "structures_projeto.h"
#include "driver/pcnt.h"

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

static AsyncWebSocket *ws = nullptr;
static int *PASSO_ROTA = 0;
static String *movimento = nullptr;
static int64_t *META_PULSOS = nullptr;
static Rota *ROTA_ATUAL = nullptr;
static volatile int64_t *total_pulsos_esq = nullptr;
static volatile int64_t *total_pulsos_dir = nullptr;

void motoresSetup(AsyncWebSocket *pws, int *pPASSO_ROTA, String *pmovimento, int64_t *pMETA_PULSOS, Rota *pROTA_ATUAL, volatile int64_t *ptotal_pulsos_esq, volatile int64_t *ptotal_pulsos_dir){
  // andar
  pinMode(FRENTE_ESQ, OUTPUT);
  pinMode(TRAS_ESQ, OUTPUT);
  pinMode(FRENTE_DIR, OUTPUT);
  pinMode(TRAS_DIR, OUTPUT);

  ws = pws;
  PASSO_ROTA = pPASSO_ROTA;
  movimento = pmovimento;
  META_PULSOS = pMETA_PULSOS;
  ROTA_ATUAL = pROTA_ATUAL;
  total_pulsos_esq = ptotal_pulsos_esq;
  total_pulsos_dir = ptotal_pulsos_dir;
}


// funcoes de andar
void moverMotorEsq(int direcao) {
  if (direcao == 1) {
    digitalWrite(FRENTE_ESQ, HIGH);
    digitalWrite(TRAS_ESQ, LOW);
  } else if (direcao == -1) {
    digitalWrite(FRENTE_ESQ, LOW);
    digitalWrite(TRAS_ESQ, HIGH);
  } else {
    digitalWrite(FRENTE_ESQ, LOW);
    digitalWrite(TRAS_ESQ, LOW);
  }
}

/**
 * Controla o Motor B
 * direcao: 1 (frente), -1 (trás), 0 (parar/frear)
 */
void moverMotorDir(int direcao) {
  if (direcao == 1) {
    digitalWrite(FRENTE_DIR, HIGH);
    digitalWrite(TRAS_DIR, LOW);
  } else if (direcao == -1) {
    digitalWrite(FRENTE_DIR, LOW);
    digitalWrite(TRAS_DIR, HIGH);
  } else {
    digitalWrite(FRENTE_DIR, LOW);
    digitalWrite(TRAS_DIR, LOW);
  }
}

// Função auxiliar para parar tudo
void pararMotores() {
  moverMotorEsq(0);
  moverMotorDir(0);
}

void executarRota(Rota &novaRota) {
  if (*PASSO_ROTA < 0) {
    return;
  }
  const ComandoRota &cmd = novaRota.comandos[*PASSO_ROTA];

  if (cmd.tipo == "ROTATE") {
    if (cmd.direcao == "direita") {
      *movimento = "ROTATE_D";
      *total_pulsos_esq = 0;
      *total_pulsos_dir = 0;
      moverMotorEsq(1);
      *META_PULSOS = 500;
      ws->textAll("VIRAR A DIREITA \n\n\n\n\n");
    }

    else if (cmd.direcao == "esquerda") {
      *movimento = "ROTATE_E";
      *total_pulsos_esq = 0;
      *total_pulsos_dir = 0;
      moverMotorDir(1);
      *META_PULSOS = 500;  // VV VER QUANTIDADE BOA AQUI
      ws->textAll("VIRAR A ESQUERDA \n\n\n\n\n");
    }
  } else if (cmd.tipo == "MOVE") {
    *movimento = "MOVE";
    *META_PULSOS = cmd.valor * 1000;
    *total_pulsos_esq = 0;
    *total_pulsos_dir = 0;

    moverMotorEsq(1);
    moverMotorDir(1);

    ws->textAll("FRENTE \n\n\n\n\n");
  }
}

void proximoComando(Rota &novaRota) {
  (*PASSO_ROTA)++;
  ws->textAll("Passo rota: " + String(*PASSO_ROTA) + "\nquantidade comandos: " + String(novaRota.comandos.size()));
  if (*PASSO_ROTA < novaRota.comandos.size()) {
    ws->textAll("Indo para a instrução " + String(*PASSO_ROTA + 1));
    executarRota(novaRota);

  } else {
    Serial.println("✅ Rota completa!");
    ws->textAll("{\"channel\":\"ROTA_COMPLETA\",\"status\":\"ok\"}");
    *ROTA_ATUAL = Rota();
    *PASSO_ROTA = -1;
  }
}


void configuraEncoderEsquerdoPCNT() {
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

void configuraEncoderDireitoPCNT() {
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
