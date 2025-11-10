// ========================================
// CEGOINHA - Interface Web
// Sistema de controle de rotas para ESP32
// ========================================

// Configuração de Debug
const DEBUG = false; // Alterar para true para ativar logs detalhados
const log = DEBUG ? console.log.bind(console) : () => {};

// ========================================
// ESTADO GLOBAL
// ========================================
let rotas = [];
let rotaAtual = { elementos: [] };
let websocket;
let wsConnected = false;
let reconnectAttempts = 0;
const MAX_RECONNECT_ATTEMPTS = 10;
const deviceId = getDeviceId();
const sessionId = 'session_' + Date.now();
const gateway = `ws://${window.location.hostname}/ws`;

// ========================================
// SELETORES DOM
// ========================================
const $ = sel => document.querySelector(sel);
const $$ = sel => document.querySelectorAll(sel);

const elements = {
    // Status
    wsStatus: $('#ws-status'),
    statusDot: $('.status-dot'),
    statusText: $('.status-text'),
    
    // Botões de adicionar
    btnAddDistancia: $('.distancia-add .btn-add'),
    btnAddRotacao: $('.rotacao-add .btn-add'),
    
    // Inputs
    inputDistancia: $('#input-distancia'),
    inputRotacao: $('#input-graus'),
    selectDirecao: $('#select-direcao'),
    
    // Botões de ação
    btnConcluir: $('#btn-concluir-rota'),
    btnLimparRota: $('#btn-limpar-rota'),
    btnLimparRotas: $('.btn-rotas-limpar'),
    btnAbrirPorta: $('#btn-abrir-porta'),
    btnFecharPorta: $('#btn-fechar-porta'),
    
    // Containers
    elementosContainer: $('.elementos-adicionados'),
    rotasAnterioresContainer: $('.rotas-grid'),
    svgTrajetoria: $('.map-route-large'),
    statusPortaInfo: $('#status-porta-info'),
    
    // Info do mapa
    mapaInfo: {
        distancia: $$('.trajetoria-info')[0],
        tempo: $$('.trajetoria-info')[1]
    }
};

// ========================================
// GERENCIAMENTO DE DEVICE ID
// ========================================
function getDeviceId() {
    let deviceId = localStorage.getItem('cegoinha_device_id');
    
    if (!deviceId) {
        deviceId = 'device_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
        localStorage.setItem('cegoinha_device_id', deviceId);
        console.log('🆔 Novo dispositivo criado:', deviceId);
    } else {
        console.log('🆔 Dispositivo reconhecido:', deviceId);
    }
    
    return deviceId;
}

// ========================================
// ATUALIZAÇÃO DE STATUS DE CONEXÃO
// ========================================
function atualizarStatusConexao(status) {
    if (!elements.statusDot || !elements.statusText) return;
    
    // Remover todas as classes de status
    elements.statusDot.classList.remove('connecting', 'connected', 'disconnected');
    
    switch(status) {
        case 'connecting':
            elements.statusDot.classList.add('connecting');
            elements.statusText.textContent = 'Conectando...';
            document.title = 'Cegoinha 🕊️ - Conectando';
            break;
            
        case 'connected':
            elements.statusDot.classList.add('connected');
            elements.statusText.textContent = 'Conectado';
            document.title = 'Cegoinha 🕊️ - Conectado';
            reconnectAttempts = 0; // Reset contador
            break;
            
        case 'disconnected':
            elements.statusDot.classList.add('disconnected');
            elements.statusText.textContent = 'Desconectado';
            document.title = 'Cegoinha 🕊️ - Desconectado';
            break;
    }
}

// ========================================
// WEBSOCKET - CONEXÃO E EVENTOS
// ========================================
function initWebSocket() {
    console.log('🔄 Tentando conectar ao WebSocket...');
    log('🆔 Device ID:', deviceId);
    log('🔑 Session ID:', sessionId);
    
    atualizarStatusConexao('connecting');
    
    try {
        websocket = new WebSocket(gateway);
        websocket.onopen = onOpen;
        websocket.onclose = onClose;
        websocket.onerror = onError;
        websocket.onmessage = onMessage;
    } catch (error) {
        console.error('❌ Erro ao criar WebSocket:', error);
        scheduleReconnect();
    }
}

function onOpen(event) {
    console.log('✅ WebSocket conectado!');
    wsConnected = true;
    atualizarStatusConexao('connected');
    
    // Enviar identificação
    const identificacao = {
        channel: 'DEVICE_ID',
        deviceId: deviceId,
        sessionId: sessionId,
        timestamp: Date.now()
    };
    
    websocket.send(JSON.stringify(identificacao));
    console.log('📤 Identificação enviada para ESP32');
}

function onClose(event) {
    console.log('❌ WebSocket desconectado');
    wsConnected = false;
    atualizarStatusConexao('disconnected');
    
    scheduleReconnect();
}

function onError(event) {
    console.error('❌ Erro no WebSocket:', event);
    wsConnected = false;
}

function scheduleReconnect() {
    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        console.error('❌ Máximo de tentativas de reconexão atingido');
        alert('Não foi possível conectar ao ESP32. Por favor, verifique a conexão WiFi.');
        return;
    }
    
    reconnectAttempts++;
    const delay = Math.min(1000 * Math.pow(2, reconnectAttempts - 1), 30000); // Exponential backoff
    
    console.log(`⏳ Tentativa ${reconnectAttempts}/${MAX_RECONNECT_ATTEMPTS} - Reconectando em ${delay/1000}s...`);
    setTimeout(initWebSocket, delay);
}

function onMessage(event) {
    log('📨 Mensagem recebida:', event.data);
    
    try {
        const data = JSON.parse(event.data);
        
        // Processar mensagens por canal
        switch(data.channel) {
            case 'SYNC_ROTAS':
                console.log('🔄 Sincronizando rotas do servidor...');
                rotas = data.rotas || [];
                renderizarRotasAnteriores();
                console.log(`✅ ${rotas.length} rotas sincronizadas`);
                break;
                
            case 'NOVA_ROTA':
                console.log('📥 Nova rota recebida de outro usuário');
                rotas.push(data.rota);
                renderizarRotasAnteriores();
                atualizarEstatisticasGlobais();
                break;
                
            case 'ROTAS_LIMPAS':
                console.log('🗑️ Rotas limpas por outro usuário');
                rotas = [];
                renderizarRotasAnteriores();
                break;
                
            case 'STATUS_PORTA':
                console.log('🚪 Status da porta atualizado:', data.status);
                if (elements.statusPortaInfo) {
                    elements.statusPortaInfo.textContent = `A porta está: ${data.status}`;
                }
                break;
                
            default:
                // Mensagens de status
                if (data.status === 'ok' && data.message) {
                    log(`✅ ${data.message}`);
                    if (data.message.includes('Dispositivo identificado')) {
                        console.log(`✅ Dispositivo identificado: ${data.deviceId}`);
                    }
                } else if (data.status === 'error' && data.message) {
                    console.error(`❌ Erro: ${data.message}`);
                    alert(`Erro: ${data.message}`);
                }
        }
    } catch (e) {
        log('Mensagem texto (não-JSON):', event.data);
    }
}

// ========================================
// CÁLCULOS DE TRAJETÓRIA
// ========================================

/**
 * Calcula pontos da trajetória baseado em comandos de movimento e rotação
 * Refatorado para evitar duplicação de código
 */
function calcularTrajetoria(elementos, viewBoxWidth, viewBoxHeight, margem) {
    let pontos = [{x: 0, y: 0}];
    let x = 0, y = 0, angulo = 0;
    
    // Processar cada elemento
    elementos.forEach(el => {
        if (el.tipo === 'distancia') {
            const distancia = parseFloat(el.valor);
            const radianos = (angulo - 90) * Math.PI / 180;
            
            x += distancia * Math.cos(radianos);
            y += distancia * Math.sin(radianos);
            pontos.push({x, y});
            
        } else if (el.tipo === 'rotacao') {
            angulo += parseFloat(el.valor) * (el.direcao === 'direita' ? 1 : -1);
            angulo = ((angulo % 360) + 360) % 360; // Normalizar ângulo
        }
    });
    
    // Calcular bounding box
    const minX = Math.min(...pontos.map(p => p.x));
    const maxX = Math.max(...pontos.map(p => p.x));
    const minY = Math.min(...pontos.map(p => p.y));
    const maxY = Math.max(...pontos.map(p => p.y));
    
    const largura = maxX - minX;
    const altura = maxY - minY;
    
    // Calcular escala para caber no viewBox
    const escala = (largura < 1 && altura < 1) ? 10 : Math.min(
        largura > 0 ? (viewBoxWidth - 2 * margem) / largura : 10,
        altura > 0 ? (viewBoxHeight - 2 * margem) / altura : 10
    );
    
    // Calcular offset para centralizar
    const offsetX = viewBoxWidth / 2 - ((minX + maxX) / 2) * escala;
    const offsetY = viewBoxHeight / 2 - ((minY + maxY) / 2) * escala;
    
    return { pontos, escala, offsetX, offsetY };
}

/**
 * Gera o SVG path string para a trajetória
 */
function gerarPathSVG(elementos, viewBoxWidth, viewBoxHeight, margem) {
    if (elementos.length === 0) {
        return {
            path: '',
            pontoInicial: { x: viewBoxWidth / 2, y: viewBoxHeight - 30 },
            pontoFinal: { x: viewBoxWidth / 2, y: viewBoxHeight - 30 }
        };
    }
    
    const { escala, offsetX, offsetY } = calcularTrajetoria(elementos, viewBoxWidth, viewBoxHeight, margem);
    
    let x = 0, y = 0, angulo = 0;
    const pontoInicialX = offsetX;
    const pontoInicialY = offsetY;
    
    let path = `M ${pontoInicialX} ${pontoInicialY}`;
    let pontoFinal = { x: pontoInicialX, y: pontoInicialY };
    
    elementos.forEach(el => {
        if (el.tipo === 'distancia') {
            const radianos = (angulo - 90) * Math.PI / 180;
            x += parseFloat(el.valor) * Math.cos(radianos);
            y += parseFloat(el.valor) * Math.sin(radianos);
            
            const novoX = x * escala + offsetX;
            const novoY = y * escala + offsetY;
            
            path += ` L ${novoX} ${novoY}`;
            pontoFinal = { x: novoX, y: novoY };
            
        } else if (el.tipo === 'rotacao') {
            angulo += parseFloat(el.valor) * (el.direcao === 'direita' ? 1 : -1);
            angulo = ((angulo % 360) + 360) % 360;
        }
    });
    
    return {
        path: path,
        pontoInicial: { x: pontoInicialX, y: pontoInicialY },
        pontoFinal: pontoFinal
    };
}

/**
 * Desenha a trajetória no mapa grande
 */
function desenharTrajetoria(elementos) {
    if (!elements.svgTrajetoria) return;
    
    const viewBoxWidth = 400;
    const viewBoxHeight = 350;
    
    if (elementos.length === 0) {
        // Resetar visualização
        elements.svgTrajetoria.innerHTML = '';
        
        if (elements.mapaInfo.distancia) {
            elements.mapaInfo.distancia.textContent = 'Distância do percurso: 0m';
        }
        if (elements.mapaInfo.tempo) {
            elements.mapaInfo.tempo.textContent = 'Tempo estimado do percurso: 0s';
        }
        
        // Posicionar ícones no padrão
        const caminhao = $('.trajetoria .truck-icon-large');
        const trofeu = $('.trajetoria .award-icon-large');
        
        if (caminhao) {
            caminhao.style.left = '15%';
            caminhao.style.bottom = '10%';
        }
        if (trofeu) {
            trofeu.style.left = '85%';
            trofeu.style.bottom = '10%';
        }
        
        return;
    }
    
    // Gerar path SVG
    const { path, pontoInicial, pontoFinal } = gerarPathSVG(elementos, viewBoxWidth, viewBoxHeight, 30);
    
    elements.svgTrajetoria.innerHTML = `
        <path d="${path}" 
              stroke="#313C41" 
              stroke-width="3" 
              fill="none" 
              stroke-linecap="round" 
              stroke-linejoin="round"/>
    `;
    
    // Atualizar posições dos ícones
    function atualizarIcone(icone, ponto) {
        if (icone) {
            icone.style.left = `${(ponto.x / viewBoxWidth) * 100}%`;
            icone.style.bottom = `${((viewBoxHeight - ponto.y) / viewBoxHeight) * 100}%`;
            icone.style.transform = 'translate(-50%, 50%)';
        }
    }
    
    atualizarIcone($('.trajetoria .truck-icon-large'), pontoInicial);
    atualizarIcone($('.trajetoria .award-icon-large'), pontoFinal);
    
    // Atualizar estatísticas
    const stats = calcularEstatisticasRota(elementos);
    
    if (elements.mapaInfo.distancia) {
        elements.mapaInfo.distancia.textContent = `Distância do percurso: ${stats.distanciaMetros}m`;
    }
    if (elements.mapaInfo.tempo) {
        elements.mapaInfo.tempo.textContent = `Tempo estimado do percurso: ${Math.round(stats.distanciaTotal / 50)}s`;
    }
}

/**
 * Calcula estatísticas da rota (distância, tempo, consumo)
 */
function calcularEstatisticasRota(elementos) {
    let distanciaTotal = 0;
    let tempoEstimado = 0;
    
    elementos.forEach(el => {
        if (el.tipo === 'distancia') {
            const dist = parseFloat(el.valor);
            distanciaTotal += dist;
            tempoEstimado += dist / 50; // 50 cm/s velocidade estimada
            
        } else if (el.tipo === 'rotacao') {
            tempoEstimado += parseFloat(el.valor) / 90; // 90 graus/s velocidade estimada
        }
    });
    
    return {
        distanciaTotal: Math.round(distanciaTotal),
        distanciaMetros: Math.round(distanciaTotal / 10) / 10,
        tempoMinutos: Math.round(tempoEstimado / 60),
        velocidadeMedia: 1.8, // km/h
        consumo: Math.round(distanciaTotal * 0.5) / 10 // Wh
    };
}

// ========================================
// RENDERIZAÇÃO DE ROTAS
// ========================================

/**
 * Renderiza o histórico de rotas anteriores
 */
function renderizarRotasAnteriores() {
    if (!elements.rotasAnterioresContainer) return;
    
    if (rotas.length === 0) {
        elements.rotasAnterioresContainer.innerHTML = `
            <div style="padding: 40px; text-align: center; color: #666; font-size: 20px; grid-column: 1 / -1;">
                Nenhuma rota registrada ainda
            </div>
        `;
        atualizarEstatisticasGlobais();
        return;
    }
    
    // Mostrar apenas as últimas 5 rotas
    const rotasRecentes = rotas.slice(-5).reverse();
    
    elements.rotasAnterioresContainer.innerHTML = rotasRecentes.map((rota, index) => {
        const stats = calcularEstatisticasRota(rota.elementos);
        const { path, pontoInicial, pontoFinal } = gerarPathSVG(rota.elementos, 316, 211, 30);
        
        const calcPos = (coord, size) => (coord / size) * 100;
        const calcBottom = (y, height) => ((height - y) / height) * 100;
        
        return `
            <div class="card-rota" data-rota-id="${rota.id}">
                <div class="rota-header">
                    <p class="rota-name">Rota ${rotas.length - index}</p>
                    <p class="rota-distance">${stats.distanciaMetros}m</p>
                </div>
                <div class="rota-body">
                    <p class="rota-time">${stats.tempoMinutos} min</p>
                    <p class="rota-vmedia">Vmédia: ${stats.velocidadeMedia}km/h</p>
                    <p class="rota-consumo">Consumo de ${stats.consumo} Wh de bateria</p>
                    <div class="mapa-container">
                        <div class="mapa-placeholder">
                            <svg class="map-route" width="100%" height="100%" viewBox="0 0 316 211" 
                                 preserveAspectRatio="xMidYMid meet" fill="none">
                                <path d="${path}" 
                                      stroke="#313C41" 
                                      stroke-width="2" 
                                      fill="none" 
                                      stroke-linecap="round" 
                                      stroke-linejoin="round"/>
                            </svg>
                            <div class="truck-icon" 
                                 style="left: ${calcPos(pontoInicial.x, 316)}%; 
                                        bottom: ${calcBottom(pontoInicial.y, 211)}%; 
                                        transform: translate(-50%, 50%)">
                                🚚
                            </div>
                            <div class="award-icon" 
                                 style="left: ${calcPos(pontoFinal.x, 316)}%; 
                                        bottom: ${calcBottom(pontoFinal.y, 211)}%; 
                                        transform: translate(-50%, 50%)">
                                🏆
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `;
    }).join('');
    
    atualizarEstatisticasGlobais();
}

/**
 * Atualiza estatísticas globais (consumo total, distância total, etc)
 */
function atualizarEstatisticasGlobais() {
    const capacidadeBateria = 10000; // Wh
    let consumoTotal = 0;
    let distanciaTotal = 0;
    
    rotas.forEach(rota => {
        const stats = calcularEstatisticasRota(rota.elementos);
        consumoTotal += stats.consumo;
        distanciaTotal += stats.distanciaMetros;
    });
    
    const porcentagemGasta = Math.min((consumoTotal / capacidadeBateria) * 100, 100);
    const soc = Math.max(100 - porcentagemGasta, 0);
    
    const atualizar = (id, valor) => {
        const el = $('#' + id);
        if (el) el.textContent = Math.round(valor);
    };
    
    atualizar('rotas-concluidas', rotas.length);
    atualizar('bateria-gasta', consumoTotal);
    atualizar('porcentagem-gasta', porcentagemGasta);
    atualizar('distancia-total', distanciaTotal);
    atualizar('bateria-soc', soc);
}

/**
 * Renderiza os elementos da rota atual
 */
function renderizarElementos() {
    if (!elements.elementosContainer) return;
    
    elements.elementosContainer.innerHTML = rotaAtual.elementos.map((el, i) => {
        const content = el.tipo === 'distancia'
            ? `<p class="elemento-label">Distância</p>
               <p class="elemento-value">${el.valor}</p>`
            : `<p class="elemento-label">Girar ${el.valor}°</p>
               <p class="elemento-value">Direção: ${el.direcao === 'direita' ? 'Direita ➡️' : 'Esquerda ⬅️'}</p>`;
        
        return `
            <div class="elemento-item ${el.tipo}" data-id="${el.id}">
                <div class="elemento-content">${content}</div>
                <button class="btn-remover" data-id="${el.id}">-</button>
                <span class="elemento-numero">${i + 1}</span>
            </div>
        `;
    }).join('');
    
    // Adicionar event listeners aos botões de remover
    $$('.btn-remover').forEach(btn => {
        btn.addEventListener('click', () => {
            rotaAtual.elementos = rotaAtual.elementos.filter(el => el.id !== parseInt(btn.dataset.id));
            renderizarElementos();
            desenharTrajetoria(rotaAtual.elementos);
        });
    });
}

// ========================================
// MANIPULAÇÃO DE ELEMENTOS DE ROTA
// ========================================

/**
 * Adiciona um elemento (distância ou rotação) à rota atual
 */
function adicionarElemento(tipo, valor, direcao = null) {
    const elemento = {
        tipo: tipo,
        valor: valor,
        id: Date.now()
    };
    
    if (tipo === 'rotacao') {
        elemento.direcao = direcao;
    }
    
    rotaAtual.elementos.push(elemento);
    renderizarElementos();
    desenharTrajetoria(rotaAtual.elementos);
}

/**
 * Envia rota para o ESP32
 */
function enviarRota(rota) {
    if (!wsConnected) {
        alert('⚠️ WebSocket não está conectado. Tentando reconectar...');
        initWebSocket();
        return;
    }
    
    // Converter elementos para formato esperado pelo ESP32
    const comandos = rota.elementos.map(el => {
        if (el.tipo === 'distancia') {
            return {
                tipo: 'MOVE',
                valor: parseInt(el.valor)
            };
        } else {
            return {
                tipo: 'ROTATE',
                angulo: parseInt(el.valor),
                direcao: el.direcao
            };
        }
    });
    
    const payload = {
        channel: "ENVIAR_ROTAS",
        value: comandos,
        deviceId: deviceId,
        rotaId: rota.id
    };
    
    log('📤 Enviando rota para ESP32:', payload);
    websocket.send(JSON.stringify(payload));
}

// ========================================
// EVENT LISTENERS
// ========================================

// Adicionar distância
if (elements.btnAddDistancia) {
    elements.btnAddDistancia.addEventListener('click', () => {
        const valor = elements.inputDistancia.value.trim();
        
        if (valor && parseFloat(valor) > 0) {
            adicionarElemento('distancia', valor);
            elements.inputDistancia.value = '';
        } else {
            alert('Insira um valor válido para a distância');
        }
    });
}

// Adicionar rotação
if (elements.btnAddRotacao) {
    elements.btnAddRotacao.addEventListener('click', () => {
        const valor = elements.inputRotacao.value.trim();
        
        if (valor && parseFloat(valor) > 0) {
            adicionarElemento('rotacao', valor, elements.selectDirecao.value);
            elements.inputRotacao.value = '';
        } else {
            alert('Insira um valor válido para a rotação');
        }
    });
}

// Concluir rota
if (elements.btnConcluir) {
    elements.btnConcluir.addEventListener('click', () => {
        if (rotaAtual.elementos.length === 0) {
            return alert('Adicione pelo menos um elemento à rota');
        }
        
        const rota = {
            id: Date.now(),
            elementos: [...rotaAtual.elementos],
            dataHora: new Date().toISOString()
        };
        
        enviarRota(rota);
        
        // Limpar rota atual
        rotaAtual.elementos = [];
        renderizarElementos();
        desenharTrajetoria([]);
        
        alert('Rota enviada com sucesso!');
    });
}

// Limpar rota atual
if (elements.btnLimparRota) {
    elements.btnLimparRota.addEventListener('click', () => {
        if (confirm('Deseja limpar todos os elementos da rota atual?')) {
            rotaAtual.elementos = [];
            renderizarElementos();
            desenharTrajetoria([]);
        }
    });
}

// Limpar todas as rotas
if (elements.btnLimparRotas) {
    elements.btnLimparRotas.addEventListener('click', () => {
        if (confirm('Deseja limpar todas as rotas anteriores? Isso afetará todos os usuários conectados.')) {
            if (wsConnected) {
                websocket.send(JSON.stringify({
                    channel: "LIMPAR_ROTAS",
                    deviceId: deviceId
                }));
            }
        }
    });
}

// Controle da porta - Abrir
if (elements.btnAbrirPorta) {
    elements.btnAbrirPorta.addEventListener('click', () => {
        log('Enviando comando ABRIR');
        
        if (wsConnected) {
            websocket.send(JSON.stringify({
                channel: "ABRIR",
                deviceId: deviceId
            }));
        } else {
            alert('WebSocket não conectado!');
        }
    });
}

// Controle da porta - Fechar
if (elements.btnFecharPorta) {
    elements.btnFecharPorta.addEventListener('click', () => {
        log('Enviando comando FECHAR');
        
        if (wsConnected) {
            websocket.send(JSON.stringify({
                channel: "FECHAR",
                deviceId: deviceId
            }));
        } else {
            alert('WebSocket não conectado!');
        }
    });
}

// Enter para adicionar elementos
if (elements.inputDistancia) {
    elements.inputDistancia.addEventListener('keypress', e => {
        if (e.key === 'Enter') elements.btnAddDistancia.click();
    });
}

if (elements.inputRotacao) {
    elements.inputRotacao.addEventListener('keypress', e => {
        if (e.key === 'Enter') elements.btnAddRotacao.click();
    });
}

// ========================================
// INICIALIZAÇÃO
// ========================================
document.addEventListener('DOMContentLoaded', () => {
    console.log('=================================');
    console.log('  CEGOINHA 🕊️ - Interface Web');
    console.log('=================================');
    
    // Renderizar estado inicial
    renderizarRotasAnteriores();
    desenharTrajetoria([]);
    
    // Conectar ao WebSocket
    initWebSocket();
});
