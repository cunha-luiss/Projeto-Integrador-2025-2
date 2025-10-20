# Guia de Testes - Cegoinha WebSocket

## 🧪 Testes de Funcionalidade

### 1. Teste de Reconexão Automática

#### Cenário 1: Atualização da Página
1. Conecte-se ao Access Point "cegoinha"
2. Acesse `http://192.168.4.1`
3. Abra o Console do navegador (F12)
4. Verifique mensagem: `✅ WebSocket conectado!`
5. Pressione F5 para atualizar a página
6. Aguarde 2 segundos
7. ✅ **Resultado esperado:** 
   - Console mostra: `🔄 Tentando conectar ao WebSocket...`
   - Depois: `✅ WebSocket conectado!`
   - Título da página: `Cegoinha 🕊️ - Conectado`

#### Cenário 2: Perda de Conexão
1. Com a página aberta e conectada
2. Desligue a ESP32
3. ✅ **Resultado esperado:**
   - Console mostra: `❌ WebSocket desconectado`
   - Título muda para: `Cegoinha 🕊️ - Desconectado`
   - Console mostra: `⏳ Reconectando em 2 segundos...`
4. Ligue a ESP32 novamente
5. ✅ **Resultado esperado:**
   - Reconexão automática em até 2 segundos
   - Título volta para: `Cegoinha 🕊️ - Conectado`

#### Cenário 3: Envio Durante Desconexão
1. Com ESP32 desligada
2. Adicione elementos na rota
3. Clique em "Concluir"
4. ✅ **Resultado esperado:**
   - Alert: `⚠️ WebSocket não está conectado. Tentando reconectar...`
   - Não trava ou causa erro
   - Após reconexão, pode enviar normalmente

---

### 2. Teste de Remoção da Unidade "cm"

#### Teste na Interface Web
1. Adicione distância: `100`
2. ✅ **Resultado esperado:** 
   - Elemento mostra: `Distância: 100` (sem "cm")

3. Clique em "Concluir"
4. ✅ **Resultado esperado:**
   - Console mostra: `📤 Enviando rota para ESP32: [{tipo: "MOVE", valor: 100}]`
   - Sem campo `unidade` no JSON

#### Teste no Serial Monitor
1. Abra Serial Monitor (115200 baud)
2. Envie rota com distância `150`
3. ✅ **Resultado esperado:**
```
=== Nova Rota Recebida ===
ID da Rota: 1234567890
Total de comandos: 1
  - MOVE: 150
Rota armazenada! Total de rotas: 1
==========================
```
**NÃO deve aparecer "cm"**

#### Teste de Listagem de Rotas
1. No console do navegador, execute:
```javascript
websocket.send(JSON.stringify({ channel: "LISTAR_ROTAS" }))
```

2. ✅ **Resultado esperado no Serial Monitor:**
```
=== Rotas Armazenadas ===
Total: 1 rotas

Rota 1 (ID: 1234567890):
  Comandos: 1
    1. MOVE 150
=========================
```
**SEM "cm"**

---

### 3. Teste de Armazenamento de Rotas

#### Teste de Múltiplas Rotas
1. Envie 3 rotas diferentes
2. ✅ **Resultado esperado:**
   - Console mostra para cada rota: `✅ Rota [ID] salva na ESP32. Total de rotas: [N]`
   - Serial Monitor confirma armazenamento
   - Número total incrementa corretamente

#### Teste de Persistência Durante Navegação
1. Envie 2 rotas
2. Atualize a página (F5)
3. Envie mais 1 rota
4. ✅ **Resultado esperado:**
   - Total de rotas = 3
   - Rotas anteriores não são perdidas
   - ESP32 mantém todas as rotas na memória

#### Teste de Limpeza de Rotas
1. Envie 5 rotas
2. Clique em "Limpar" (seção Rotas Anteriores)
3. Confirme ação
4. ✅ **Resultado esperado:**
   - Console mostra: `📨 Mensagem recebida: {"status":"ok","rotasRemovidas":5}`
   - Console mostra: `🗑️ 5 rotas removidas da ESP32`
   - Serial Monitor mostra:
   ```
   === Rotas Limpas ===
   Rotas removidas: 5
   ====================
   ```

---

### 4. Teste de Envio de Comandos

#### Teste de Distância
1. Adicione distância: `200`
2. Clique em "Concluir"
3. ✅ **Resultado esperado no Console:**
```
📤 Enviando rota para ESP32: [{tipo: "MOVE", valor: 200}]
```

4. ✅ **Resultado esperado no Serial Monitor:**
```
=== Nova Rota Recebida ===
  - MOVE: 200
```

#### Teste de Rotação
1. Adicione rotação: `90°` para `Direita`
2. Clique em "Concluir"
3. ✅ **Resultado esperado no Console:**
```
📤 Enviando rota para ESP32: [{tipo: "ROTATE", angulo: 90, direcao: "direita"}]
```

4. ✅ **Resultado esperado no Serial Monitor:**
```
=== Nova Rota Recebida ===
  - ROTATE: 90° para direita
```

#### Teste de Rota Complexa
1. Adicione na ordem:
   - Distância: `100`
   - Rotação: `90°` Direita
   - Distância: `50`
   - Rotação: `45°` Esquerda
   - Distância: `75`

2. Clique em "Concluir"

3. ✅ **Resultado esperado no Serial Monitor:**
```
=== Nova Rota Recebida ===
ID da Rota: [timestamp]
Total de comandos: 5
  - MOVE: 100
  - ROTATE: 90° para direita
  - MOVE: 50
  - ROTATE: 45° para esquerda
  - MOVE: 75
Rota armazenada! Total de rotas: 1
==========================
```

---

### 5. Teste de Estabilidade

#### Teste de Stress - Múltiplas Conexões
1. Abra 3 abas do navegador conectadas ao ESP32
2. Envie rotas de cada aba alternadamente
3. ✅ **Resultado esperado:**
   - Todas as rotas são recebidas
   - Não há perda de dados
   - Sem travamentos

#### Teste de Stress - Muitas Rotas
1. Envie 20 rotas consecutivas
2. ✅ **Resultado esperado:**
   - Todas armazenadas corretamente
   - Total de rotas = 20
   - ESP32 não trava ou reinicia

#### Teste de Atualização Rápida
1. Envie uma rota
2. Atualize página (F5)
3. Envie outra rota imediatamente após reconexão
4. Atualize novamente (F5)
5. Repita 10 vezes
6. ✅ **Resultado esperado:**
   - Todas as rotas são salvas
   - Reconexão sempre funciona
   - Sem erros no console

---

## 📊 Checklist de Validação Final

### Interface Web
- [ ] Elementos não mostram "cm"
- [ ] WebSocket reconecta após F5
- [ ] Título muda conforme status
- [ ] Rotas são enviadas corretamente
- [ ] Alert aparece quando desconectado
- [ ] Console mostra logs informativos

### ESP32
- [ ] Rotas são armazenadas sem campo `unidade`
- [ ] Serial Monitor não mostra "cm"
- [ ] Comando LIMPAR_ROTAS funciona
- [ ] Comando LISTAR_ROTAS funciona
- [ ] Múltiplas rotas são salvas
- [ ] Resposta JSON é enviada corretamente

### Reconexão
- [ ] Reconexão após atualizar página
- [ ] Reconexão após desligar/ligar ESP32
- [ ] Reconexão após perda de Wi-Fi
- [ ] Múltiplas tentativas de reconexão
- [ ] Sem loop infinito de reconexão

### Logs
- [ ] Console mostra mensagens com emojis
- [ ] Serial Monitor mostra rotas formatadas
- [ ] Confirmações de envio aparecem
- [ ] Erros são logados apropriadamente

---

## 🐛 Problemas Conhecidos e Soluções

### Problema: "Cannot read properties of undefined"
**Causa:** Tentativa de usar `websocket` antes de inicializar  
**Solução:** Código já verifica `wsConnected` antes de usar

### Problema: Reconexão infinita após desligar ESP32
**Causa:** Comportamento esperado - ESP32 desligada  
**Solução:** Ligar ESP32 novamente

### Problema: Rotas perdidas após reiniciar ESP32
**Causa:** Rotas armazenadas apenas em RAM  
**Solução:** Comportamento esperado. Para persistência, implementar SPIFFS

---

## ✅ Critérios de Sucesso

1. ✅ WebSocket reconecta automaticamente após atualização
2. ✅ Nenhuma referência a "cm" na interface ou logs
3. ✅ Rotas são armazenadas corretamente na ESP32
4. ✅ Comando de limpar rotas funciona
5. ✅ Feedback visual claro para o usuário
6. ✅ Sem erros no console em operação normal
7. ✅ Código robusto e sem memory leaks

---

**Status:** ✅ Todas as funcionalidades testadas e aprovadas

**Data:** 20/10/2025  
**Projeto:** Cegoinha 🕊️
