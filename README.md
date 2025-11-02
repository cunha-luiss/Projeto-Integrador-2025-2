# 🕊️ Projeto Integrador 2025-2 - Cegoinha

Sistema de controle e monitoramento de rotas para o carrinho autônomo Cegoinha, utilizando ESP32 com interface web via WebSocket.

## 📁 Estrutura do Projeto

```
Projeto-Integrador-2025-2/
├── websocket/
│   └── cegoinha_websocket/
│       ├── cegoinha_websocket.ino    ⭐ ARQUIVO PRINCIPAL
│       ├── LITTLEFS_SYNC.md          (Documentação)
│       └── TESTES.md                 (Documentação de testes)
│   ├── websocket.ino                 (Versão antiga - não usar)
│   └── README.md
├── figma/                            (Protótipos e testes separados)
└── README.md                         (Este arquivo)
```

> ⚠️ **IMPORTANTE**: Os arquivos nas pastas `figma/` e `websocket/websocket.ino` são apenas **protótipos, testes e tentativas antigas**. 
> 
> 🎯 **O ÚNICO arquivo que deve ser usado é**: `websocket/cegoinha_websocket/cegoinha_websocket.ino`

---

## 🚀 Início Rápido

### 1. Pré-requisitos

#### Hardware
- **ESP32** (qualquer modelo compatível com Arduino IDE)
- Cabo USB para conexão
- Computador ou smartphone para acessar a interface web

#### Software
- **Arduino IDE** (versão 1.8.x ou 2.x)
- **Bibliotecas Arduino** (instalação detalhada abaixo)

### 2. Instalação das Bibliotecas

Abra o Arduino IDE e instale as seguintes bibliotecas através do **Library Manager** (`Sketch → Include Library → Manage Libraries...`):

1. **ESP32 Board Support**
   - Em `File → Preferences`, adicione esta URL em "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
   - Vá em `Tools → Board → Boards Manager`, procure por "esp32" e instale

2. **AsyncTCP** (by me-no-dev)
   - Baixe manualmente de: https://github.com/me-no-dev/AsyncTCP
   - Extraia e coloque na pasta `libraries` do Arduino

3. **ESPAsyncWebServer** (by me-no-dev)
   - Baixe manualmente de: https://github.com/me-no-dev/ESPAsyncWebServer
   - Extraia e coloque na pasta `libraries` do Arduino

4. **ArduinoJson** (by Benoit Blanchon)
   - Versão: 6.x ou superior
   - Instale via Library Manager (procure por "ArduinoJson")

5. **LittleFS** (ESP32)
   - Já incluído no pacote ESP32 board support

### 3. Configuração do Arduino IDE

1. **Selecione a placa ESP32:**
   - `Tools → Board → ESP32 Arduino → ESP32 Dev Module`
   - (ou o modelo específico da sua placa)

2. **Selecione a porta COM:**
   - `Tools → Port → COMx` (Windows)
   - ou `/dev/ttyUSBx` (Linux/Mac)

### 4. Upload do Código

1. **Abra o arquivo principal:**
   ```
   Arquivo → Abrir → websocket/cegoinha_websocket/cegoinha_websocket.ino
   ```

2. **Verifique a compilação:**
   - Clique em "Verificar/Compilar" (✓) ou pressione `Ctrl+R`
   - Aguarde a compilação terminar sem erros

3. **Faça o upload:**
   - Clique em "Upload" (→) ou pressione `Ctrl+U`
   - Aguarde a mensagem "Upload completo"

4. **Monitore a inicialização:**
   - Abra o Monitor Serial (`Ctrl+Shift+M`)
   - Configure para **115200 baud**
   - Pressione o botão **RESET** no ESP32
   - Você deve ver:
   ```
   =================================
       CEGOINHA ESP32 - Iniciando
   =================================
   
   --- Inicializando LittleFS ---
   ✅ LittleFS montado com sucesso
   📊 Espaço total: 1441792 bytes
   📊 Espaço usado: 8192 bytes (0.6%)
   ------------------------------
   
   ✅ Sistema de armazenamento de rotas inicializado
   ✅ Sistema de identificação de dispositivos inicializado
   
   --- Configuração de Rede ---
   Modo: Access Point
   SSID: cegoinha
   IP address: 192.168.4.1
   ----------------------------
   
   Servidor Web iniciado!
   Aguardando conexões...
   ```

---

## 🌐 Uso da Interface Web

### 1. Conexão WiFi

1. **Conecte ao WiFi do ESP32:**
   - **Nome da rede (SSID):** `cegoinha`
   - **Senha:** `cegoinha123`

2. **Acesse a interface:**
   - Abra um navegador web
   - Digite: `http://192.168.4.1`
   - Pressione `Ctrl + F5` para forçar atualização

### 2. Funcionalidades

#### ✅ Status de Conexão
- **🟢 Conectado** - WebSocket ativo e funcional
- **🟡 Conectando...** - Tentando estabelecer conexão
- **🔴 Desconectado** - Sem conexão com o ESP32

#### 📍 Enviar Rota
1. **Adicionar distância:**
   - Digite o valor em centímetros
   - Clique no botão `+`
   
2. **Adicionar rotação:**
   - Digite o ângulo em graus (0-360)
   - Escolha a direção (Direita/Esquerda)
   - Clique no botão `+`

3. **Visualizar trajeto:**
   - O mapa mostra o caminho em tempo real
   - 🚚 marca o início
   - 🏆 marca o fim

4. **Concluir rota:**
   - Clique em "Concluir" para enviar ao ESP32
   - A rota é armazenada no LittleFS

5. **Limpar rota atual:**
   - Clique em "Limpar" para recomeçar

#### 📜 Rotas Anteriores
- Visualize até 5 últimas rotas
- Cada card mostra:
  - Distância total
  - Tempo estimado
  - Velocidade média
  - Consumo de bateria
  - Mapa do trajeto com 🚚 e 🏆

- **Botão Limpar:** Remove todas as rotas armazenadas (afeta todos os usuários)

#### 📊 Estatísticas
- **Rotas concluídas:** Total de rotas enviadas
- **Bateria gasta:** Consumo total em Wh
- **Distância total:** Soma de todas as rotas
- **Bateria restante (SOC):** Percentual restante

#### ⚡ Status do Carrinho
- Consumo dos motores (tempo real)
- Estado da bateria

---

## 🔧 Características Técnicas

### Arquitetura
- **Modo:** Access Point (ponto de acesso WiFi)
- **Protocolo:** WebSocket para comunicação em tempo real
- **Armazenamento:** LittleFS (persistente na flash do ESP32)
- **Formato de dados:** JSON

### Funcionalidades Implementadas
- ✅ Sincronização multi-usuário em tempo real
- ✅ Persistência de rotas na flash (sobrevive a reinicializações)
- ✅ Identificação única de dispositivos (via localStorage)
- ✅ Reconexão automática de WebSocket
- ✅ Visualização gráfica de trajetórias com SVG
- ✅ Cálculo automático de estatísticas (tempo, consumo, distância)
- ✅ Interface responsiva e moderna

### Limites do Sistema
- **Máximo de rotas armazenadas:** Limitado pela memória flash (~1.5MB)
- **Conexões simultâneas:** Limitado pela memória RAM do ESP32
- **Tamanho do JSON:** Buffer de 4KB a 8KB por operação

---

## 📚 Documentação Adicional

- **LITTLEFS_SYNC.md** - Detalhes do sistema de persistência

---

## 🔄 Atualizações e Manutenção

### Como atualizar o código

1. Baixe a versão mais recente do repositório
2. Abra o arquivo `cegoinha_websocket.ino` no Arduino IDE
3. Faça o upload normalmente
4. **IMPORTANTE:** Após o upload, acesse a interface e pressione `Ctrl + F5`

### Backup das rotas

As rotas são armazenadas automaticamente no arquivo `/rotas.json` na flash do ESP32. Para fazer backup:

1. Use ferramentas de acesso ao LittleFS (ESP32 Filesystem Uploader)
2. Ou implemente uma função de exportação via interface web (futuro)

---

## 👥 Suporte

Para problemas, dúvidas ou sugestões:

1. Verifique os logs do Monitor Serial
2. Verifique o console do navegador (F12)
3. Abra uma issue no repositório GitHub

---

## 📝 Notas Importantes

- **Não use os arquivos de teste:** Apenas `cegoinha_websocket.ino` é mantido e funcional
- **Sempre faça Ctrl+F5:** Após qualquer upload, force atualização no navegador
- **Mantenha o Monitor Serial aberto:** Útil para debug e monitoramento
- **Bateria:** O sistema calcula consumo estimado baseado em 1.8km/h e 50Wh/m

---

## 🎯 Roadmap Futuro

- [ ] Controle em tempo real dos motores via interface
- [ ] Exportação de rotas em JSON
- [ ] Logs de execução detalhados
- [ ] Integração com sensores (ultrassônico, giroscópio)

---

## 📜 Licença

Este projeto está sob a licença GNU AFFERO GENERAL PUBLIC LICENSE v3 especificada no arquivo LICENSE.

---

# Contribuição

Siga o workflow abaixo para contribuir:

1. Crie uma branch a partir da `development`:
    ```bash
    git checkout dev
    git pull
    git checkout -b sua-feature
    ```

2. Faça suas alterações e commits na nova branch.

3. Envie sua branch para o repositório remoto:
    ```bash
    git push origin sua-feature
    ```
4. Abra um Pull Request (PR) da sua branch para a branch `dev` (só ir no site e colocar Base: dev <-- compare: sua-feature).

**Desenvolvido para o Projeto Integrador 1 FCTE-UnB 2025-2** 🎓
