#include <Arduino.h> // Necessário para funções Arduino (embora não usemos aqui)
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

#define CIRCUNFERENCIA_RODA (6.5 * PI)

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
const int pwmChannelA_Frente = 4; // canales 0-3 ficam livres para o servo
const int pwmChannelA_Tras = 5;
const int pwmChannelB_Frente = 6;
const int pwmChannelB_Tras = 7;
const int resolution = 8;         // 0 a 255
const int PPR = 1050;             // 7 pulsos * redução 150
const int PWM_CORRECAO_MAX = 120; // limite máx para correção do seguidor

// Variáveis do PID
double Setpoint_A, Input_A, Output_A;
double Setpoint_B, Input_B, Output_B;
volatile long contadorA = 0;
volatile long contadorB = 0;
static double rpmMotorB = 0.0;        // Guarda a medição real do motor direito
static bool modoSincronizado = false; // true quando o direito está seguindo o esquerdo

// Variáveis de estado de direção: 1 = frente, -1 = ré, 0 = freio
int direcao_motor_A = 0;
int direcao_motor_B = 0;

// Ajustes do PID (Kp, Ki, Kd) -> VOCÊ PRECISARÁ AJUSTAR ISSO DEPOIS
double Kp = 2, Ki = 2, Kd = 0.1;

// Criando os objetos PID
PID pidMotorA(&Input_A, &Output_A, &Setpoint_A, Kp, Ki, Kd, DIRECT);
PID pidMotorB(&Input_B, &Output_B, &Setpoint_B, Kp, Ki, Kd, DIRECT);

void motoresSetup(AsyncWebSocket *pws, int *pPASSO_ROTA, String *pmovimento, int64_t *pMETA_PULSOS, Rota *pROTA_ATUAL, volatile int64_t *ptotal_pulsos_esq, volatile int64_t *ptotal_pulsos_dir, const int *psampleTimePID)
{
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

  pinMode(ENC_A_ESQ, INPUT);        // Lembrete: Pino 34 não tem Pullup interno
  pinMode(ENC_B_DIR, INPUT_PULLUP); // Pino 32 tem Pullup

  // Ativa as interrupções para contar os pulsos automaticamente
  attachInterrupt(digitalPinToInterrupt(ENC_A_ESQ), readEncoderA, RISING);
  attachInterrupt(digitalPinToInterrupt(ENC_B_DIR), readEncoderB, RISING);

  // Configura canais PWM para o ESP32
  ledcSetup(pwmChannelA_Frente, freq, resolution);
  ledcSetup(pwmChannelA_Tras, freq, resolution);
  ledcSetup(pwmChannelB_Frente, freq, resolution);
  ledcSetup(pwmChannelB_Tras, freq, resolution);

  // Associa os canais PWM aos pinos
  ledcAttachPin(FRENTE_ESQ, pwmChannelA_Frente);
  ledcAttachPin(TRAS_ESQ, pwmChannelA_Tras);
  ledcAttachPin(FRENTE_DIR, pwmChannelB_Frente);
  ledcAttachPin(TRAS_DIR, pwmChannelB_Tras);

  // Inicializa PIDs
  Setpoint_A = 28; // [rpm] Setpoint máximo de 145.71 rpm(se manter esse valor por muito tempo vai queimar)
  Setpoint_B = 28;

  pidMotorA.SetMode(AUTOMATIC);
  pidMotorB.SetMode(AUTOMATIC);

  // Limites do PWM (0 a 255)
  pidMotorA.SetOutputLimits(0, 255); // transforma em voltas por segundo
  pidMotorB.SetOutputLimits(0, 255);

  // Tempo de amostragem do PID
  pidMotorA.SetSampleTime(*sampleTime);
  pidMotorB.SetSampleTime(*sampleTime);
}

// ===== Funções Auxiliares PWM =====
void IRAM_ATTR readEncoderA()
{
  // Simples incremento. Para saber direção, precisaria ler o canal B também.
  contadorA++;
}
void IRAM_ATTR readEncoderB()
{
  contadorB++;
}
void aplicarControleMotorA(int pwmVal, int direcao)
{
  if (direcao == 1)
  {
    // Frente: PWM em FRENTE_ESQ, TRAS_ESQ em 0
    ledcWrite(pwmChannelA_Frente, pwmVal);
    ledcWrite(pwmChannelA_Tras, 0);
  }
  else if (direcao == -1)
  {
    // Ré: PWM em TRAS_ESQ, FRENTE_ESQ em 0
    ledcWrite(pwmChannelA_Frente, 0);
    ledcWrite(pwmChannelA_Tras, pwmVal);
  }
  else
  {
    // Freio: ambos em HIGH (255) para freio ativo
    ledcWrite(pwmChannelA_Frente, 255);
    ledcWrite(pwmChannelA_Tras, 255);
  }
}

void aplicarControleMotorB(int pwmVal, int direcao)
{
  if (direcao == 1)
  {
    // Frente: PWM em FRENTE_DIR, TRAS_DIR em 0
    ledcWrite(pwmChannelB_Frente, pwmVal);
    ledcWrite(pwmChannelB_Tras, 0);
  }
  else if (direcao == -1)
  {
    // Ré: PWM em TRAS_DIR, FRENTE_DIR em 0
    ledcWrite(pwmChannelB_Frente, 0);
    ledcWrite(pwmChannelB_Tras, pwmVal);
  }
  else
  {
    // Freio: ambos em HIGH (255) para freio ativo
    ledcWrite(pwmChannelB_Frente, 255);
    ledcWrite(pwmChannelB_Tras, 255);
  }
}

// funcoes de andar
void moverMotorEsq(int direcao)
{ // A
  direcao_motor_A = direcao;

  if (direcao == 1)
  {
    // Frente
    pidMotorA.SetMode(AUTOMATIC);
    Setpoint_A = 28;
  }
  else if (direcao == -1)
  {
    // Ré
    pidMotorA.SetMode(AUTOMATIC);
    Setpoint_A = 28;
  }
  else if (direcao == 2)
  {                      // curva para direita - motor esquerdo anda, direito para
    direcao_motor_A = 1; // Motor esquerdo vai para frente
    pidMotorA.SetMode(AUTOMATIC);
    Setpoint_A = 28;
  }
  else
  {
    // Parar com freio
    pidMotorA.SetMode(MANUAL);
    Setpoint_A = 0;
    Output_A = 0;
    aplicarControleMotorA(0, 0); // Aplica o freio imediatamente
  }
}

/**
 * Controla o Motor B
 * direcao: 1 (frente), -1 (trás), 0 (parar/frear), 2 (curva)
 */
void moverMotorDir(int direcao)
{ // B
  direcao_motor_B = direcao;

  if (direcao == 1)
  {
    // Frente
    pidMotorB.SetMode(AUTOMATIC);
    Setpoint_B = 28;
  }
  else if (direcao == -1)
  {
    // Ré
    pidMotorB.SetMode(AUTOMATIC);
    Setpoint_B = 28;
  }
  else if (direcao == 2)
  {                      // curva para esquerda - motor direito anda, esquerdo para
    direcao_motor_B = 1; // Motor direito vai para frente
    pidMotorB.SetMode(AUTOMATIC);
    Setpoint_B = 28;
  }
  else
  {
    // Parar com freio
    pidMotorB.SetMode(MANUAL);
    Setpoint_B = 0;
    Output_B = 0;
    aplicarControleMotorB(0, 0); // Aplica o freio imediatamente
  }
}

// Função auxiliar para parar tudo com freio
void pararMotores()
{
  Setpoint_A = 0;
  Setpoint_B = 0;
  direcao_motor_A = 0;
  direcao_motor_B = 0;
  pidMotorA.SetMode(MANUAL);
  pidMotorB.SetMode(MANUAL);
  Output_A = 0;
  Output_B = 0;

  // Aplica o freio imediatamente
  aplicarControleMotorA(0, 0);
  aplicarControleMotorB(0, 0);
}

void executarRota(Rota &novaRota)
{
  if (*PASSO_ROTA < 0)
  {
    return;
  }
  const ComandoRota &cmd = novaRota.comandos[*PASSO_ROTA];

  if (cmd.tipo == "ROTATE")
  {
    if (cmd.direcao == "direita")
    {
      *movimento = "ROTATE_D";
      *total_pulsos_esq = 0;
      *total_pulsos_dir = 0;
      *META_PULSOS = 219; // Define META_PULSOS ANTES de mover

      // Motor direito para com freio
      moverMotorDir(0);
      // Motor esquerdo anda
      moverMotorEsq(2);

    ws->textAll("{\"channel\":\"UPDATE\",\"value\":\"DIREITA\"}");
    }

    else if (cmd.direcao == "esquerda")
    {
      *movimento = "ROTATE_E";
      *total_pulsos_esq = 0;
      *total_pulsos_dir = 0;
      *META_PULSOS = 219; // Define META_PULSOS ANTES de mover

      // Motor esquerdo para com freio
      moverMotorEsq(0);
      // Motor direito anda
      moverMotorDir(2);

    ws->textAll("{\"channel\":\"UPDATE\",\"value\":\"ESQUERDA\"}");
    }
  }
  else if (cmd.tipo == "MOVE")
  {
    *movimento = "MOVE";
    *META_PULSOS = (int64_t)floor(cmd.valor * 4.4); //6,36 quantidade de pulsos em 1 cm
    *total_pulsos_esq = 0;
    *total_pulsos_dir = 0;

    moverMotorEsq(1);
    moverMotorDir(1);

    ws->textAll("{\"channel\":\"UPDATE\",\"value\":\"FRENTE\"}");
  }
}

void proximoComando(Rota &novaRota)
{
  (*PASSO_ROTA)++;
  ws->textAll("Passo rota: " + String(*PASSO_ROTA) + "\nquantidade comandos: " + String(novaRota.comandos.size()));
  if (*PASSO_ROTA < novaRota.comandos.size())
  {
    ws->textAll("Indo para a instrução " + String(*PASSO_ROTA + 1));
    executarRota(novaRota);
  }
  else
  {
    Serial.println("✅ Rota completa!");
    ws->textAll("{\"channel\":\"ROTA_COMPLETA\",\"status\":\"ok\"}");
    *ROTA_ATUAL = Rota();
    *PASSO_ROTA = -1;
  }
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

void calcularPID()
{
  // Motores em RPM
  Input_A = (contadorA * 600.0) / PPR;
  rpmMotorB = (contadorB * 600.0) / PPR;

  // Reseta os contadores para o próximo ciclo
  contadorA = 0;
  contadorB = 0;

  bool motoresAtivos = (direcao_motor_A != 0 && direcao_motor_B != 0);
  bool movimentoLinear = (movimento != nullptr && *movimento == "MOVE");
  double diferencaPulsos = 0.0;
  modoSincronizado = motoresAtivos && movimentoLinear;

  if (modoSincronizado && total_pulsos_esq != nullptr && total_pulsos_dir != nullptr)
  {
    diferencaPulsos = (double)(*total_pulsos_esq - *total_pulsos_dir);
    // Input_B precisa ser o oposto da diferença para que o erro interno seja Esq-Dir
    Input_B = (double)(*total_pulsos_dir - *total_pulsos_esq);
    Setpoint_B = 0;
    pidMotorB.SetOutputLimits(-PWM_CORRECAO_MAX, PWM_CORRECAO_MAX);
  }
  else
  {
    Input_B = rpmMotorB;
    pidMotorB.SetOutputLimits(0, 255);
  }

  // 2. Calcular o PID
  pidMotorA.Compute();
  pidMotorB.Compute();

  double pwmMotorA = Output_A;
  double pwmMotorB = 0.0;

  if (modoSincronizado)
  {
    pwmMotorB = constrain(Output_A + Output_B, 0.0, 255.0);
  }
  else
  {
    pwmMotorB = Output_B;
  }

  // 3. Debug no Serial Plotter (Muito útil!)
  // Mostra: Meta vs Realidade
  Serial.print("Setpoint_A:");
  Serial.print(Setpoint_A);
  Serial.print("  ,A_Vel:");
  Serial.print(Input_A);
  Serial.print(" rpm");
  Serial.print("  ,A_PWM:");
  Serial.print(Output_A);
  Serial.print("  ,A_Dir:");
  Serial.print(direcao_motor_A);

  Serial.print("    |    ");

  Serial.print("Setpoint_B:");
  Serial.print(Setpoint_B);
  Serial.print("  ,B_Vel:");
  Serial.print(rpmMotorB);
  Serial.print(" rpm");
  Serial.print("  ,B_PWM:");
  Serial.print(modoSincronizado ? pwmMotorB : Output_B);
  Serial.print("  ,B_Dir:");
  Serial.print(direcao_motor_B);
  if (modoSincronizado)
  {
    Serial.print("  ,Diff:");
    Serial.print(diferencaPulsos);
  }
  Serial.println();

  // 4. Aplicar o PWM nos motores baseado no setpoint e direção
  if (*META_PULSOS != 0 && *META_PULSOS != -1)
  {
    // Motor A: Se setpoint é 0, aplica freio. Caso contrário, aplica PWM na direção correta
    if (Setpoint_A == 0)
    {
      aplicarControleMotorA(0, 0); // Freio
    }
    else
    {
      aplicarControleMotorA(pwmMotorA, direcao_motor_A);
    }

    // Motor B: Se setpoint é 0, aplica freio. Caso contrário, aplica PWM na direção correta
    if (direcao_motor_B == 0 || (!modoSincronizado && Setpoint_B == 0))
    {
      aplicarControleMotorB(0, 0); // Freio
    }
    else
    {
      aplicarControleMotorB(pwmMotorB, direcao_motor_B);
    }
  }
  else
  {
    // META_PULSOS é 0 ou -1: para tudo com freio
    aplicarControleMotorA(0, 0);
    aplicarControleMotorB(0, 0);
  }

  calcularVelocidadeInstantanea();
}

float calcularVelocidadeInstantanea()
{
  // Média das velocidades já calculadas em RPM
  float velocidade_media_rpm = (Input_A + rpmMotorB) / 2.0;

  // Converte RPM para cm/s
  // Formula: (RPM / 60) * Circunferência = velocidade linear
  float velocidadeInstantanea = (velocidade_media_rpm / 60.0) * CIRCUNFERENCIA_RODA;

  // Formata JSON com 1 casa decimal
  String json = "{\"channel\":\"VELOCIDADE\",\"value\":" + String(59, 1) + "}";

  // Enviar para todos os clientes conectados
  ws->textAll(json);

  return velocidadeInstantanea;
}

// trabalhando na velocidade