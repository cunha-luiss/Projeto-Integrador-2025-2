#ifndef STRUCTURES_PROJETO_H
#define STRUCTURES_PROJETO_H

struct ComandoRota {
  String tipo;     // "MOVE" ou "ROTATE"
  int valor;       // distância ou ângulo em graus
  String direcao;  // "direita" ou "esquerda" (apenas para ROTATE)
};

struct Rota {
  unsigned long id;
  std::vector<ComandoRota> comandos;
  String dataHora;
  String deviceId;  // ID do dispositivo que enviou a rota
};

struct DispositivoConectado {
  uint32_t clientId;
  String deviceId;
  String sessionId;
  unsigned long lastSeen;
};

#endif