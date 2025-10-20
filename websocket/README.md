# Cegoinha - Sistema Integrado com WebSocket

## 📋 Descrição

Sistema completo de controle do robô Cegoinha com interface web e comunicação WebSocket para ESP32.

## 🔧 Componentes

- **cegoinha_websocket.ino**: Código Arduino para ESP32 com interface web integrada
- **Interface Web**: HTML/CSS/JS embutido no código (otimizado para memória ESP32)
- **WebSocket**: Comunicação bidirecional em tempo real

## ⚙️ Funcionalidades

### Interface Web
- ✅ Criação de rotas com distâncias e rotações
- ✅ Visualização de trajetória em tempo real
- ✅ Histórico de rotas anteriores
- ✅ Cálculo de consumo de bateria (SOC)
- ✅ Estatísticas de distância e tempo

### Controle do LED (Testes)
- **Botão "Concluir"**: Liga o LED (GPIO 2) ao enviar rota
- **Botão "Limpar" (Rotas Anteriores)**: Desliga o LED

### Comunicação WebSocket
- Comandos enviados:
  - `LED_ON`: Liga o LED
  - `LED_OFF`: Desliga o LED
  - `{tipo:'ROTA', comandos:[...]}`: Envia comandos de movimento

## 📦 Bibliotecas Necessárias

Instale via Arduino IDE (Sketch > Include Library > Manage Libraries):

1. **WiFi** (built-in ESP32)
2. **AsyncTCP** 
   - Autor: me-no-dev
   - URL: https://github.com/me-no-dev/AsyncTCP
3. **ESPAsyncWebServer**
   - Autor: me-no-dev
   - URL: https://github.com/me-no-dev/ESPAsyncWebServer

## 🚀 Como Usar

### 1. Upload do Código

```cpp
// Configure as credenciais WiFi (já estão no código):
const char* ssid = "cegoinha";
const char* password = "cegoinha123";
```

1. Abra `cegoinha_websocket.ino` no Arduino IDE
2. Selecione a placa: **ESP32 Dev Module**
3. Selecione a porta COM correta
4. Clique em **Upload**

### 2. Conectar ao ESP32

1. Procure a rede WiFi: **cegoinha**
2. Senha: **cegoinha123**
3. Acesse no navegador: **http://192.168.4.1**

### 3. Testar Funcionalidades

#### Teste do LED:
1. Adicione elementos na rota (distâncias e rotações)
2. Clique em **"Concluir"** → LED acende
3. Clique em **"Limpar"** (na seção Rotas Anteriores) → LED apaga

#### Criar Rota:
1. Digite um valor de distância (ex: 50 cm) e clique no **+**
2. Digite um ângulo de rotação (ex: 90°), escolha direção e clique no **+**
3. Repita para criar sequência de movimentos
4. Clique em **"Concluir"** para enviar

## 🔌 Hardware

- **LED**: GPIO 2 (LED embutido na maioria das placas ESP32)
- **Alimentação**: USB ou bateria (5V)

## 📊 Monitoramento Serial

Abra o Serial Monitor (115200 baud) para ver:
```
WiFi AP iniciado
SSID: cegoinha
IP: 192.168.4.1
Servidor HTTP iniciado
WebSocket cliente #1 conectado de 192.168.4.2
Mensagem recebida: LED_ON
LED ligado pelo botão Concluir
```

## 🎯 Próximos Passos

Para implementar controle real do robô:

1. **Adicione motores**: Configure os GPIOs dos motores
2. **Parse comandos**: Processe o JSON de rotas recebido
3. **Implemente movimentos**: 
   ```cpp
   void executarComando(String tipo, int valor) {
     if (tipo == "MOVE") {
       // Mover motores por 'valor' cm
     } else if (tipo == "ROTATE") {
       // Girar 'valor' graus
     }
   }
   ```
4. **Feedback em tempo real**: Use `ws.textAll()` para atualizar status

## 📝 Estrutura dos Dados

### Comando de Rota (JSON):
```json
{
  "tipo": "ROTA",
  "comandos": [
    {"tipo": "MOVE", "valor": 50, "unidade": "cm"},
    {"tipo": "ROTATE", "angulo": 90, "direcao": "direita"},
    {"tipo": "MOVE", "valor": 30, "unidade": "cm"}
  ]
}
```

## 🐛 Troubleshooting

**LED não acende/apaga:**
- Verifique se o LED embutido está no GPIO 2
- Algumas placas usam GPIO 22 ou outro pino
- Altere `const int ledPin = 2;` se necessário

**Não conecta ao WiFi:**
- Verifique se o ESP32 está criando o AP
- Tente esquecer/reconectar a rede
- Reinicie o ESP32

**Interface não carrega:**
- Verifique o IP no Serial Monitor
- Tente `http://192.168.4.1`
- Limpe cache do navegador

## 📄 Licença

Projeto educacional - Projeto Integrador 2025-2

---

**Desenvolvido para ESP32 com interface otimizada (~30KB)**
