# Sistema de Sincronização com LittleFS - Cegoinha

## 🎯 Objetivo

Otimizar o sistema para que **todos os usuários vejam as mesmas rotas**, usando **LittleFS** na ESP32 para persistência e sincronização em tempo real via WebSocket.

---

## ✅ Mudanças Implementadas

### ❌ **REMOVIDO** - localStorage para Rotas
- **Antes:** Cada usuário tinha suas próprias rotas salvas localmente
- **Depois:** Rotas são centralizadas na ESP32

### ✅ **ADICIONADO** - LittleFS (Persistência na ESP32)
- Rotas salvas em arquivo `/rotas.json` na ESP32
- Sobrevivem a reinicializações
- Compartilhadas entre todos os usuários

### ✅ **ADICIONADO** - Sincronização Automática
- Novos usuários recebem rotas ao conectar
- Todos os usuários são notificados quando:
  - Nova rota é adicionada
  - Rotas são limpas

---

## 🏗️ Arquitetura

```
┌─────────────┐         ┌─────────────┐         ┌─────────────┐
│  Usuário 1  │         │  Usuário 2  │         │  Usuário N  │
│   (Browser) │         │   (Browser) │         │   (Browser) │
└──────┬──────┘         └──────┬──────┘         └──────┬──────┘
       │                       │                        │
       │    WebSocket          │                        │
       └───────────┬───────────┴────────────────────────┘
                   │
            ┌──────▼──────┐
            │    ESP32    │
            │  WebSocket  │
            │   Server    │
            └──────┬──────┘
                   │
        ┌──────────┴──────────┐
        │                     │
   ┌────▼────┐         ┌──────▼──────┐
   │  RAM    │         │  LittleFS   │
   │ (Temp)  │         │ (Permanent) │
   └─────────┘         └─────────────┘
                       /rotas.json
```

---

## 📊 Fluxo de Dados

### 1️⃣ Novo Usuário Conecta

```
Usuário 1 conecta -> WebSocket CONNECT
                          ↓
                  Envia DEVICE_ID
                          ↓
              ESP32 reconhece dispositivo
                          ↓
          Envia SYNC_ROTAS com todas as rotas
                          ↓
         Usuário 1 vê todas as rotas salvas
```

### 2️⃣ Usuário Envia Nova Rota

```
Usuário 1 envia rota -> ESP32 recebe ENVIAR_ROTAS
                              ↓
                    Adiciona à RAM (rotasArmazenadas)
                              ↓
                    Salva em LittleFS (/rotas.json)
                              ↓
          Notifica TODOS os clientes conectados (NOVA_ROTA)
                              ↓
          ┌─────────────────┬─────────────────┬────────────┐
          ↓                 ↓                 ↓            ↓
      Usuário 1         Usuário 2         Usuário 3    ... N
   (vê sua rota)    (recebe notif.)   (recebe notif.)
```

### 3️⃣ Usuário Limpa Rotas

```
Usuário 1 clica "Limpar" -> ESP32 recebe LIMPAR_ROTAS
                                  ↓
                        Limpa RAM (clear vector)
                                  ↓
                        Deleta arquivo LittleFS
                                  ↓
              Notifica TODOS (ROTAS_LIMPAS)
                                  ↓
                    Todos veem lista vazia
```

### 4️⃣ ESP32 Reinicia

```
ESP32 liga -> Inicializa LittleFS
                    ↓
          Carrega /rotas.json
                    ↓
        Popula RAM com rotas salvas
                    ↓
          Aguarda conexões
                    ↓
    Novos usuários recebem rotas salvas
```

---

## 🔧 Implementação Técnica

### ESP32 - Funções LittleFS

#### `salvarRotasLittleFS()`
```cpp
void salvarRotasLittleFS() {
  DynamicJsonDocument doc(8192);
  JsonArray rotasArray = doc.createNestedArray("rotas");
  
  // Serializar rotas
  for (const auto& rota : rotasArmazenadas) {
    // ... adicionar ao JSON ...
  }
  
  // Salvar arquivo
  File file = LittleFS.open(ROTAS_FILE, "w");
  serializeJson(doc, file);
  file.close();
}
```

#### `carregarRotasLittleFS()`
```cpp
void carregarRotasLittleFS() {
  File file = LittleFS.open(ROTAS_FILE, "r");
  DynamicJsonDocument doc(8192);
  deserializeJson(doc, file);
  
  // Desserializar rotas
  JsonArray rotasArray = doc["rotas"];
  for (JsonObject rotaObj : rotasArray) {
    // ... reconstruir rotas ...
  }
}
```

#### `enviarRotasParaCliente()`
```cpp
void enviarRotasParaCliente(AsyncWebSocketClient *client) {
  DynamicJsonDocument doc(8192);
  doc["channel"] = "SYNC_ROTAS";
  JsonArray rotasArray = doc.createNestedArray("rotas");
  
  // Formatar rotas para envio
  for (const auto& rota : rotasArmazenadas) {
    // ... criar estrutura compatível com JS ...
  }
  
  client->text(jsonString);
}
```

---

### JavaScript - Handlers de Mensagens

#### SYNC_ROTAS (Sincronização Inicial)
```javascript
if (data.channel === 'SYNC_ROTAS') {
    rotas = data.rotas || [];
    renderizarRotasAnteriores();
    console.log(`✅ ${rotas.length} rotas sincronizadas`);
}
```

#### NOVA_ROTA (Notificação em Tempo Real)
```javascript
else if (data.channel === 'NOVA_ROTA') {
    rotas.push(data.rota);
    renderizarRotasAnteriores();
    atualizarEstatisticasGlobais();
}
```

#### ROTAS_LIMPAS (Limpeza Sincronizada)
```javascript
else if (data.channel === 'ROTAS_LIMPAS') {
    rotas = [];
    renderizarRotasAnteriores();
}
```

---

## 📝 Formato do Arquivo `/rotas.json`

```json
{
  "rotas": [
    {
      "id": 1729425678901,
      "dataHora": "1729425678901",
      "deviceId": "device_1729425678901_a7f3k9m2x",
      "comandos": [
        {
          "tipo": "MOVE",
          "valor": 100
        },
        {
          "tipo": "ROTATE",
          "valor": 90,
          "direcao": "direita"
        }
      ]
    }
  ]
}
```

---

## 🚀 Vantagens do Novo Sistema

### ✅ Colaboração em Tempo Real
- Todos os usuários veem as mesmas rotas
- Atualizações instantâneas
- Ideal para trabalho em equipe

### ✅ Persistência Robusta
- Rotas sobrevivem a reinicializações da ESP32
- Armazenamento em flash memory
- Não dependem de navegador

### ✅ Sincronização Automática
- Novos usuários já veem histórico completo
- Sem necessidade de refresh manual
- Estado global consistente

### ✅ Otimização
- Menos código JavaScript
- Menos uso de localStorage
- Lógica centralizada no servidor

---

## 🧪 Testes Recomendados

### Teste 1: Persistência LittleFS
1. Envie 3 rotas
2. Reinicie a ESP32 (desligar/ligar)
3. Conecte novamente
4. ✅ As 3 rotas devem estar lá

### Teste 2: Sincronização Multi-Usuário
1. Conecte com 2 navegadores diferentes
2. No navegador 1, envie uma rota
3. ✅ Navegador 2 deve receber notificação
4. ✅ Navegador 2 deve mostrar a nova rota

### Teste 3: Limpeza Global
1. Conecte com 2 navegadores
2. No navegador 1, clique "Limpar Rotas"
3. ✅ Ambos os navegadores devem ficar vazios
4. ✅ Arquivo LittleFS deve ser deletado

### Teste 4: Novo Usuário
1. Com rotas já salvas, conecte um novo navegador
2. ✅ Deve receber todas as rotas automaticamente
3. ✅ Não precisa localStorage

### Teste 5: Capacidade LittleFS
1. Monitor Serial mostra espaço disponível
2. Envie muitas rotas
3. ✅ Verificar uso de espaço
4. ✅ Não deve exceder ~1.5MB (limite típico)

---

## 📊 Logs Esperados

### Inicialização ESP32
```
=================================
    CEGOINHA ESP32 - Iniciando
=================================

--- Inicializando LittleFS ---
✅ LittleFS montado com sucesso
📊 Espaço total: 1507328 bytes
📊 Espaço usado: 1024 bytes (0.1%)
------------------------------

✅ Sistema de armazenamento de rotas inicializado
✅ Sistema de identificação de dispositivos inicializado
✅ 0 rotas carregadas do LittleFS
```

### Novo Usuário Conecta
```
WebSocket client #1 connected from 192.168.4.2
Canal recebido: DEVICE_ID
🆕 Novo dispositivo registrado: device_xxx (Client #1)
📊 Total de dispositivos: 1
📤 Rotas sincronizadas para cliente #1
```

### Envio de Rota
```
=== Nova Rota Recebida ===
Device ID: device_xxx
ID da Rota: 1729425678901
Total de comandos: 2
  - MOVE: 100
  - ROTATE: 90° para direita
Rota armazenada! Total de rotas: 1
💾 1 rotas salvas no LittleFS
==========================
```

### Limpeza de Rotas
```
=== Rotas Limpas ===
Device ID: device_xxx
Rotas removidas: 5
====================
```

---

## ⚠️ Limitações

### Espaço de Armazenamento
- LittleFS típico: ~1.5 MB
- Cada rota: ~200-500 bytes
- Capacidade estimada: 3000-7000 rotas

### Simultaneidade
- WebSocket assíncrono (OK)
- LittleFS não thread-safe (mas ESP32 é single-core para nosso caso)
- Escrita sequencial (OK)

### Performance
- Salvar arquivo a cada rota: ~50-100ms
- Aceitável para uso não intensivo
- Para uso intensivo, considerar buffering

---

## 🔐 Segurança

### ⚠️ Não Implementado
- Autenticação de usuários
- Controle de acesso
- Criptografia de dados

### ℹ️ Adequado Para
- Rede local confiável
- Ambiente educacional/laboratório
- Prototipagem

---

## 🚀 Melhorias Futuras

- [ ] Limite de rotas (auto-limpeza de antigas)
- [ ] Backup/export de rotas (download JSON)
- [ ] Compressão de dados (GZIP)
- [ ] Paginação para muitas rotas
- [ ] Histórico de versões
- [ ] Desfazer última rota

---

**Status:** ✅ Implementado e otimizado  
**Data:** 20/10/2025  
**Projeto:** Cegoinha 🕊️
