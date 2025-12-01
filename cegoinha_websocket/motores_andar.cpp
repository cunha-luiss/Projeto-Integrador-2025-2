#include <Arduino.h>  // Necessário para funções Arduino (embora não usemos aqui)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <PID_v1_bc.h>
#include "motores_andar.h" // Inclui as declarações do .h
#include "structures_projeto.h"
#include "driver/pcnt.h"

// Define os pinos para o Motor esquerdo (A)
#define FRENTE_ESQ 22
#define TRAS_ESQ 23
#define ENC_A_ESQ 34
#define ENC_B_ESQ 35

// Define os pinos para o Motor direito (B)
#define FRENTE_DIR 26
#define TRAS_DIR 25
#define ENC_A_DIR 33
#define ENC_B_DIR 32

#define VELOCIDADE 100

#define CIRCUNFERENCIA_RDOA (6.5 * PI)

static AsyncWebSocket *ws = nullptr;
static int *PASSO_ROTA = 0;
static String *movimento = nullptr;
static int64_t *META_PULSOS = nullptr;
static Rota *ROTA_ATUAL = nullptr;
static volatile int64_t *total_pulsos_esq = nullptr;
static volatile int64_t *total_pulsos_dir = nullptr;
static const int *sampleTime = nullptr;

// Configurações do PWM 
const int freq = 30000;
const int pwmChannelA = 0;
const int pwmChannelB = 1;
const int resolution = 8; // 0 a 255
const int PPR = 1050;  // 7 pulsos * redução 150

// Variáveis do PID
double Setpoint_A, Input_A, Output_A;
double Setpoint_B, Input_B, Output_B;
volatile long contadorA = 0;
volatile long contadorB = 0;

// Ajustes do PID (Kp, Ki, Kd) -> VOCÊ PRECISARÁ AJUSTAR ISSO DEPOIS
double Kp = 1, Ki = 2 , Kd = 0.1;

// Criando os objetos PID
PID pidMotorA(&Input_A, &Output_A, &Setpoint_A, Kp, Ki, Kd, DIRECT);
PID pidMotorB(&Input_B, &Output_B, &Setpoint_B, Kp, Ki, Kd, DIRECT);

void motoresSetup(AsyncWebSocket *pws, int *pPASSO_ROTA, String *pmovimento, int64_t *pMETA_PULSOS, Rota *pROTA_ATUAL, volatile int64_t *ptotal_pulsos_esq, volatile int64_t *ptotal_pulsos_dir, const int *psampleTimePID){
  
  ws = pws;
  PASSO_ROTA = pPASSO_ROTA;
  movimento = pmovimento;
  META_PULSOS = pMETA_PULSOS;
  ROTA_ATUAL = pROTA_ATUAL;
  total_pulsos_esq = ptotal_pulsos_esq;
  total_pulsos_dir = ptotal_pulsos_dir;
  sampleTime = psampleTimePID;
  
  // andar
  pinMode(FRENTE_ESQ, OUTPUT);
  pinMode(TRAS_ESQ, OUTPUT);
  pinMode(FRENTE_DIR, OUTPUT);
  pinMode(TRAS_DIR, OUTPUT);

  pinMode(ENC_A_ESQ, INPUT); // Lembrete: Pino 34 não tem Pullup interno
  pinMode(ENC_B_DIR, INPUT_PULLUP); // Pino 32 tem Pullup

  // Ativa as interrupções para contar os pulsos automaticamente
  attachInterrupt(digitalPinToInterrupt(ENC_A_ESQ), readEncoderA, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_B_DIR), readEncoderB, RISING);

  ledcSetup(pwmChannelA, freq, resolution);
  ledcSetup(pwmChannelB, freq, resolution);

  // Associa o PWM aos pinos que controlam a velocidade (IN1 e IN3 assumindo frente)
  // Nota: Em pontes H simples, aplicamos PWM no pino HIGH e 0 no LOW.
  ledcAttachPin(FRENTE_ESQ, pwmChannelA); 
  ledcAttachPin(FRENTE_DIR, pwmChannelB);

   // Inicializa PIDs
  Setpoint_A = 28; // [rpm] Setpoint máximo de 145.71 rpm(se manter esse valor por muito tempo vai queimar)
  Setpoint_B = 28;
  
  pidMotorA.SetMode(AUTOMATIC);
  pidMotorB.SetMode(AUTOMATIC);
  
  // Limites do PWM (0 a 255)
  pidMotorA.SetOutputLimits(0, 255); //transforma em voltas por segundo
  pidMotorB.SetOutputLimits(0, 255);
  
  // Tempo de amostragem do PID
  pidMotorA.SetSampleTime(*sampleTime);
  pidMotorB.SetSampleTime(*sampleTime);
}

// ===== Funções Auxiliares PWM =====
void IRAM_ATTR readEncoderA() {
  // Simples incremento. Para saber direção, precisaria ler o canal B também.
  contadorA++; 
}
void IRAM_ATTR readEncoderB() {
  contadorB++;
}
void moverMotorA(int pwmVal) {
  // Sentido Horário
  ledcWrite(pwmChannelA, pwmVal); // Aplica PWM no IN1
  digitalWrite(TRAS_ESQ, LOW);         // IN2 fica em 0
}

void moverMotorB(int pwmVal) {
  // Sentido Horário
  ledcWrite(pwmChannelB, pwmVal); // Aplica PWM no IN3
  digitalWrite(TRAS_DIR, LOW);         // IN4 fica em 0
}

// funcoes de andar
void moverMotorEsq(int direcao) {
  if (direcao == 1) {
    moverMotorA(Output_A);

  } else if (direcao == 2) { //curva
    analogWrite(FRENTE_ESQ, VELOCIDADE);
    digitalWrite(TRAS_ESQ, LOW);
  } else {
    ledcWrite(pwmChannelA, 0);      // Zera o PWM do IN1
    digitalWrite(FRENTE_ESQ, LOW);  // IN1 em LOW
    digitalWrite(TRAS_ESQ, LOW);  
  }
}

/**
 * Controla o Motor B
 * direcao: 1 (frente), -1 (trás), 0 (parar/frear)
 */
void moverMotorDir(int direcao) {
  if (direcao == 1) {
    moverMotorB(Output_B);
  } else if (direcao == 2) { //curva
    analogWrite(FRENTE_DIR, VELOCIDADE);
    digitalWrite(TRAS_DIR, LOW);
  } else {
    ledcWrite(pwmChannelB, 0);      // Zera o PWM do IN3
    digitalWrite(FRENTE_DIR, LOW);  // IN3 em LOW
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
      moverMotorEsq(2);
      *META_PULSOS = 100;
      ws->textAll("VIRAR A DIREITA \n\n\n\n\n");
    }

    else if (cmd.direcao == "esquerda") {
      *movimento = "ROTATE_E";
      *total_pulsos_esq = 0;
      *total_pulsos_dir = 0;
      moverMotorDir(2);
      *META_PULSOS = 100;  // VV VER QUANTIDADE BOA AQUI
      ws->textAll("VIRAR A ESQUERDA \n\n\n\n\n");
    }
  } else if (cmd.tipo == "MOVE") {
    *movimento = "MOVE";
    *META_PULSOS = (int64_t)ceil(cmd.valor * 2.91);
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

void calcularPID()
{
  //Motores em RPM
  Input_A = (contadorA * 600.0) / PPR; 
  Input_B = (contadorB * 600.0) / PPR;

  // Reseta os contadores para o próximo ciclo
  contadorA = 0;
  contadorB = 0;
  
  // 2. Calcular o PID
  pidMotorA.Compute();
  pidMotorB.Compute();

  // 3. Debug no Serial Plotter (Muito útil!)
  // Mostra: Meta vs Realidade
  Serial.print("Setpoint_A:");        Serial.print(Setpoint_A);
  Serial.print("  ,A_Vel:");        Serial.print(Input_A);        Serial.print(" rpm");
  Serial.print("  ,A_PWM:");        Serial.print(Output_A);

  Serial.print("    |    ");

  Serial.print("Setpoint_B:");        Serial.print(Setpoint_B);
  Serial.print("  ,B_Vel:");        Serial.print(Input_B);        Serial.print(" rpm");
  Serial.print("  ,B_PWM:");        Serial.println(Output_B);

  // 4. Aplicar o PWM nos motores
  if (*META_PULSOS != 0 && *META_PULSOS != -1) {
    moverMotorA(Output_A);
    moverMotorB(Output_B);
  }
  else {
    pararMotores();
  }

}

float calcularVelocidadeInstantanea() {
  float velocidade_media_rpm = (Input_A + Input_B) / 2.0;
  
  //converte RPM para cm/s
  float velocidade_cm_s = (velocidade_media_rpm / 60.0) * CIRCUNFERENCIA_RDOA;
  
  return velocidade_cm_s;
}