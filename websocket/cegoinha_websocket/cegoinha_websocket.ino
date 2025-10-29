#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>

const char* ssid = "cegoinha";
const char* password = "cegoinha123";

#define ROTAS_FILE "/rotas.json"

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

// --- Pinos do Motor da Porta (L298N) ---
// Mude estes pinos conforme a sua ligação real
#define PIN_IN1 25
#define PIN_IN2 26
#define PIN_ENA 27 // Pino para controle de velocidade (PWM)

// --- Pinos dos Sensores da Porta (Fim de Curso) ---
// Mude estes pinos conforme a sua ligação real
#define SENSOR_PORTA_ABERTA 34
#define SENSOR_PORTA_FECHADA 35

// --- Configuração do PWM da Porta ---
int freqPWM_Porta = 5000;
int canalPWM_Porta = 0; // Canal PWM 0 (verificar se não há conflito com PWM das rodas)
int resolucaoPWM_Porta = 8; // 8 bits (0-255)
int velocidadeMotorPorta = 200; // Velocidade de 0-255

// --- Controle de Estado da Porta (Lógica Não-Bloqueante) ---
#define ESTADO_PORTA_PARADO 0
#define ESTADO_PORTA_ABRINDO 1
#define ESTADO_PORTA_FECHANDO 2

int estadoPorta = ESTADO_PORTA_PARADO; // Estado atual da porta

// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====

// Estrutura para armazenar informações do dispositivo conectado
struct DispositivoConectado {
  uint32_t clientId;
  String deviceId;
  String sessionId;
  unsigned long lastSeen;
};

// Mapa de dispositivos conectados
std::vector<DispositivoConectado> dispositivosConectados;

// Estrutura para armazenar comandos de rota
struct ComandoRota {
  String tipo;      // "MOVE" ou "ROTATE"
  int valor;        // distância ou ângulo em graus
  String direcao;   // "direita" ou "esquerda" (apenas para ROTATE)
};

// Estrutura para armazenar uma rota completa
struct Rota {
  unsigned long id;
  std::vector<ComandoRota> comandos;
  String dataHora;
  String deviceId;    // ID do dispositivo que enviou a rota
};

// Vetor para armazenar todas as rotas recebidas
std::vector<Rota> rotasArmazenadas;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Cegoinha - Tela Principal</title>
    <style>
* { margin: 0; padding: 0; box-sizing: border-box; }

body {
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #fff;
    color: #000;
    line-height: 1.5;
    overflow-x: hidden;
}

.main-container {
    width: 100%;
    min-height: 100vh;
    padding: 20px;
    max-width: 1400px;
    margin: 0 auto;
}

.header { padding: 32px 0 20px 70px; }

.title, .section-title {
    font-size: 32px;
    font-weight: 400;
    margin-bottom: 20px;
}

.divider {
    width: calc(100% + 40px);
    height: 5px;
    background: #313C41;
    margin: 20px -20px;
}

.divider-top { margin-top: 0; }

.section {
    padding: 20px 0;
    margin-bottom: 20px;
    position: relative;
}

.section-subtitle {
    font-size: 20px;
    font-weight: 400;
    text-align: right;
    position: absolute;
    top: 20px;
    right: 100px;
}

.section-header {
    margin-bottom: 30px;
    position: relative;
}

/* Rotas Anteriores */
.rotas-anteriores, .consumo-carga, .status-carrinho { padding-left: 100px; }

.rotas-grid {
    display: flex;
    gap: 20px;
    margin-bottom: 20px;
    flex-wrap: wrap;
}

.card-rota {
    width: 491px;
    height: 534px;
    border-radius: 10px;
    overflow: hidden;
    flex-shrink: 0;
}

.rota-header {
    background: #313C41;
    padding: 10px 40px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-radius: 10px 10px 0 0;
    height: 46px;
}

.rota-name { font-size: 32px; color: #F6F6F6; }
.rota-distance { font-size: 24px; color: #F6F6F6; }

.rota-body {
    background: #C8C8C8;
    padding: 20px;
    height: 488px;
    border-radius: 0 0 10px 10px;
}

.rota-time, .rota-vmedia, .rota-consumo {
    font-size: 24px;
    text-align: center;
    margin-bottom: 8px;
}

.rota-consumo { margin-bottom: 15px; }

.mapa-container {
    width: 100%;
    height: 320px;
    margin-top: 10px;
}

.mapa-placeholder, .mapa-grande {
    background: #EC8A8A;
    border-radius: 15px;
    width: 100%;
    height: 100%;
    position: relative;
    display: flex;
    align-items: center;
    justify-content: center;
    overflow: hidden;
}

.mapa-text {
    font-size: 32px;
    text-align: center;
    position: relative;
    z-index: 2;
}

.map-route, .map-route-large {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    z-index: 1;
    pointer-events: none;
}

.truck-icon, .award-icon, .truck-icon-large, .award-icon-large {
    position: absolute;
    font-size: 32px;
    z-index: 3;
    transition: all 0.3s ease;
}

.truck-icon-large, .award-icon-large { font-size: 40px; }

.btn-rotas-limpar {
    margin-left: 460px;
    margin-top: 10px;
}

/* Consumo e Status */
.consumo-list {
    list-style: disc;
    padding-left: 48px;
    font-size: 32px;
}

.consumo-list li { margin-bottom: 8px; }

.status-grid {
    display: flex;
    gap: 17px;
    flex-wrap: wrap;
}

.status-card {
    width: 350px;
    height: 127px;
    border-radius: 10px;
    overflow: hidden;
    flex-shrink: 0;
}

.status-header {
    background: #313C41;
    color: #F6F6F6;
    font-size: 30px;
    text-align: center;
    padding: 8px;
    border-radius: 10px 10px 0 0;
    height: 49px;
    display: flex;
    align-items: center;
    justify-content: center;
}

.status-value {
    background: #C8C8C8;
    font-size: 40px;
    text-align: center;
    padding: 20px;
    border-radius: 0 0 10px 10px;
    height: 78px;
    display: flex;
    align-items: center;
    justify-content: center;
}

/* Main Content */
.main-content {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 50px;
    padding: 0 80px;
    margin-bottom: 30px;
}

.enviar-rota, .trajetoria { padding: 0; }

.elementos-adicionados {
    display: flex;
    flex-direction: column;
    gap: 10px;
    margin-bottom: 20px;
    min-height: 80px;
}

.elementos-adicionados:empty::before {
    content: "Adicione elementos para criar uma rota";
    display: flex;
    align-items: center;
    justify-content: center;
    height: 80px;
    color: #999;
    font-size: 16px;
    font-style: italic;
}

.elemento-item {
    height: 77px;
    border-radius: 0 10px 10px 0;
    display: flex;
    align-items: center;
    padding: 0 10px 0 15px;
    position: relative;
}

.elemento-item.distancia {
    background-color: #31A208;
    width: 213px;
}

.elemento-item.rotacao {
    background-color: #0026B8;
    width: 399px;
}

.elemento-content {
    flex: 1;
    margin-right: 10px;
}

.elemento-label, .elemento-label-small, .elemento-value {
    font-size: 18px;
    color: #F6F6F6;
    line-height: 1.3;
}

.elemento-label-small { margin: 0; }

.btn-remover, .btn-direcao, .btn-add {
    width: 30px;
    height: 30px;
    min-width: 30px;
    background: #F6F6F6;
    border: none;
    border-radius: 3px;
    color: #000;
    cursor: pointer;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-shrink: 0;
}

.btn-remover { font-size: 28px; line-height: 1; }
.btn-direcao { font-size: 16px; margin-left: 5px; }
.btn-add {
    font-size: 22px;
    position: absolute;
    left: 10px;
    top: 50%;
    transform: translateY(-50%);
}

.elemento-numero {
    position: absolute;
    right: 8px;
    bottom: 5px;
    font-size: 12px;
    color: #F6F6F6;
}

/* Adicionar */
.adicionar-elementos {
    display: flex;
    gap: 20px;
    margin-bottom: 30px;
    flex-wrap: wrap;
}

.adicionar-card {
    border-radius: 10px;
    padding: 10px;
    position: relative;
    height: 77px;
    background: #C8C8C8;
}

.adicionar-card.rotacao-add { width: 399px; }
.adicionar-card.distancia-add { width: 213px; }

.add-label, .add-label-top, .add-label-left {
    font-size: 18px;
    position: absolute;
}

.add-label { top: 10px; left: 50px; }
.add-label-top { top: 8px; right: 50px; }
.add-label-left { left: 50px; top: 8px; }

.add-input {
    position: absolute;
    bottom: 15px;
    left: 50px;
    right: 50px;
    background: transparent;
    border: none;
    border-bottom: 1px solid #666;
    font-size: 18px;
    outline: none;
    padding: 3px 5px;
}

.distancia-add .add-input { right: 15px; }
.add-input-graus { right: 130px; width: 80px; }

.add-select-direcao {
    position: absolute;
    bottom: 15px;
    right: 10px;
    width: 115px;
    height: 30px;
    background: #F6F6F6;
    border: 1px solid #666;
    border-radius: 5px;
    font-size: 16px;
    outline: none;
    padding: 3px 8px;
    cursor: pointer;
}

.add-select-direcao:focus {
    border-color: #0026B8;
    box-shadow: 0 0 3px rgba(0, 38, 184, 0.3);
}

/* Botões */
.acoes { display: flex; gap: 15px; }

.btn-concluir, .btn-limpar {
    color: #F6F6F6;
    font-size: 18px;
    padding: 12px 30px;
    border: none;
    border-radius: 10px;
    cursor: pointer;
}

.btn-concluir { background: #31A208; }
.btn-limpar { background: #A21508; }

/* Trajetória */
.trajetoria-card {
    background: #0026B8;
    border-radius: 20px;
    padding: 30px;
    color: #F6F6F6;
}

.trajetoria-info {
    font-size: 18px;
    color: #F6F6F6;
    margin-bottom: 8px;
}

.mapa-grande {
    max-width: 427px;
    height: 358px;
    margin-top: 20px;
    margin: 20px auto 0 auto;
}

/* Status Percurso */
.status-percurso {
    padding-left: 80px;
    clear: both;
}

.status-percurso-section { margin-top: 40px; }

.status-percurso-grid {
    display: grid;
    grid-template-columns: repeat(2, 234px);
    gap: 32px;
}

.status-percurso-card {
    width: 234px;
    height: 161px;
    border-radius: 10px;
    overflow: hidden;
}

.status-percurso-card .status-header {
    height: 65px;
    padding: 12px;
}

.status-percurso-card .status-value {
    font-size: 30px;
    height: 96px;
}

/* Interações */
button:hover { opacity: 0.85; transition: opacity 0.2s; }
button:active { transform: scale(0.98); }

/* Responsive adjustments */
@media (max-width: 1400px) {
    .main-content {
        grid-template-columns: 1fr;
    }
    
    .status-percurso-grid {
        grid-template-columns: repeat(2, 234px);
    }
}

@media (max-width: 1100px) {
    .rotas-grid {
        justify-content: center;
    }
}

@media (max-width: 768px) {
    .header {
        padding: 20px 0 20px 20px;
    }
    
    .rotas-anteriores,
    .consumo-carga,
    .status-carrinho,
    .status-percurso {
        padding-left: 20px;
    }
    
    .main-content {
        padding: 0 20px;
    }
    
    .card-rota {
        width: 100%;
        max-width: 491px;
    }
    
    .adicionar-elementos {
        flex-direction: column;
    }
    
    .adicionar-card {
        width: 100% !important;
        max-width: 399px;
    }
    
    .elemento-item {
        width: 100% !important;
        max-width: 399px;
    }
    
    .status-grid {
        justify-content: center;
    }
    
    .status-percurso-grid {
        grid-template-columns: repeat(auto-fit, 234px);
        justify-content: center;
    }
    
    .section-subtitle {
        position: static;
        text-align: center;
        margin-top: 10px;
    }
}

/* ===== INÍCIO: CSS ADICIONADO PARA A PORTA ===== */
.section-porta {
    padding-left: 100px;
}

@media (max-width: 768px) {
    .section-porta {
        padding-left: 20px;
    }
}

#status-porta-info {
    margin-top: 15px;
    font-size: 18px;
    color: #313C41;
    font-weight: 500;
}
/* ===== FIM: CSS ADICIONADO PARA A PORTA ===== */

    </style>
</head>
<body>
    <div class="main-container" data-name="Tela principal" data-node-id="64:174">
        <div class="header">
            <p class="title">Cegoinha 🕊️</p>
        </div>
        <div class="divider divider-top"></div>

        <section class="section rotas-anteriores" data-node-id="82:416">
            <div class="section-header">
                <p class="section-title">Rotas Anteriores</p>
            </div>
            
            <div class="rotas-grid">
                </div>

            <button class="btn-limpar btn-rotas-limpar">Limpar</button>
        </section>

        <div class="divider"></div>

        <section class="section consumo-carga" data-node-id="82:417">
            <p class="section-title">Consumo desde a última carga</p>
            <ul class="consumo-list">
                <li><span id="rotas-concluidas">0</span> rotas concluídas</li>
                <li><span id="bateria-gasta">0</span> Wh de bateria gastos (<span id="porcentagem-gasta">0</span>% da bateria)</li>
                <li><span id="distancia-total">0</span> cm andados</li>
            </ul>
        </section>

        <div class="divider"></div>

        <section class="section status-carrinho" data-node-id="179:434">
            <p class="section-title">Status carrinho</p>
            <div class="status-grid">
                <div class="status-card" data-node-id="81:376">
                    <div class="status-header">Consumo motor 1</div>
                    <div class="status-value">0 W</div>
                </div>
                <div class="status-card" data-node-id="179:435">
                    <div class="status-header">Consumo motor 2</div>
                    <div class="status-value">0 W</div>
                </div>
                <div class="status-card" data-node-id="179:436">
                    <div class="status-header">Bateria restante</div>
                    <div class="status-value"><span id="bateria-soc">100</span>%</div>
                </div>
            </div>
        </section>

        <div class="divider"></div>

        <section class="section section-porta">
            <p class="section-title">Controle da Porta</p>
            <div class="acoes">
                <button class="btn-concluir" id="btn-abrir-porta" style="background-color: #0026B8;">Abrir Porta</button>
                <button class="btn-limpar" id="btn-fechar-porta">Fechar Porta</button>
            </div>
            <p id="status-porta-info"></p>
        </section>
        
        <div class="divider"></div>
        <div class="main-content">
            <section class="section enviar-rota" data-node-id="82:418">
                <p class="section-title">Enviar rota</p>
                
                <div class="elementos-adicionados">
                    </div>

                <div class="adicionar-elementos">
                    <div class="adicionar-card rotacao-add">
                        <p class="add-label-top">Direção</p>
                        <p class="add-label-left">Girar X°</p>
                        <input type="number" class="add-input add-input-graus" id="input-graus" placeholder="90" min="0" max="360">
                        <select class="add-select-direcao" id="select-direcao">
                            <option value="direita">Direita ➡️</option>
                            <option value="esquerda">Esquerda ⬅️</option>
                        </select>
                        <button class="btn-add">+</button>
                    </div>

                    <div class="adicionar-card distancia-add">
                        <p class="add-label">Distância</p>
                        <input type="number" class="add-input" id="input-distancia" placeholder="10" min="0">
                        <button class="btn-add">+</button>
                    </div>
                </div>

                <div class="acoes">
                    <button class="btn-concluir">Concluir</button>
                    <button class="btn-limpar">Limpar</button>
                </div>
            </section>

            <section class="section trajetoria" data-node-id="64:546">
                <p class="section-title">Trajetória</p>
                
                <div class="trajetoria-card">
                    <p class="trajetoria-info">Distância do percurso: 90m</p>
                    <p class="trajetoria-info">Tempo estimado do percurso: 60s</p>
                    
                    <div class="mapa-grande">
                        <svg class="map-route-large" width="100%" height="100%" viewBox="0 0 400 350" preserveAspectRatio="xMidYMid meet" fill="none">
                            </svg>
                        <div class="truck-icon-large">🚚</div>
                        <div class="award-icon-large">🏆</div>
                    </div>
                </div>

                <div class="status-percurso-section" data-node-id="64:612">
                    <p class="section-title" style="margin-top: 40px;">Status percurso</p>
                    <div class="status-percurso-grid">
                        <div class="status-percurso-card">
                            <div class="status-header">Velocidade</div>
                            <div class="status-value">4 km/h</div>
                        </div>
                        <div class="status-percurso-card">
                            <div class="status-header">ETA</div>
                            <div class="status-value">10 min</div>
                        </div>
                        <div class="status-percurso-card">
                            <div class="status-header">Consumo</div>
                            <div class="status-value">5 Wh</div>
                        </div>
                        <div class="status-percurso-card">
                            <div class="status-header">ΔT</div>
                            <div class="status-value">5 min 3 seg</div>
                        </div>
                    </div>
                </div>
            </section>
        </div>
    </div>

    <script>

// Cegoinha - Tela Principal
document.addEventListener('DOMContentLoaded', () => {
    let rotas = [];
    let rotaAtual = { elementos: [] };

    // Seletores
    const $ = (sel) => document.querySelector(sel);
    const $$ = (sel) => document.querySelectorAll(sel);
    
    const btnAddDistancia = $('.distancia-add .btn-add');
    const btnAddRotacao = $('.rotacao-add .btn-add');
    const inputDistancia = $('#input-distancia');
    const inputRotacao = $('#input-graus');
    const selectDirecao = $('#select-direcao');
    const btnConcluir = $('.btn-concluir');
    const btnLimparRota = $('.enviar-rota .btn-limpar');
    const btnLimparRotas = $('.btn-rotas-limpar');
    const elementosContainer = $('.elementos-adicionados');
    const rotasAnterioresContainer = $('.rotas-grid');
    const svgTrajetoria = $('.map-route-large');
    const mapaInfo = {
        distancia: $$('.trajetoria-info')[0],
        tempo: $$('.trajetoria-info')[1]
    };
    
    // ===== INÍCIO: JS ADICIONADO PARA A PORTA =====
    const btnAbrirPorta = $('#btn-abrir-porta');
    const btnFecharPorta = $('#btn-fechar-porta');
    const statusPortaInfo = $('#status-porta-info');
    // ===== FIM: JS ADICIONADO PARA A PORTA =====

    // Calcular trajetória com auto-escala
    const calcularTrajetoria = (elementos, viewBoxWidth, viewBoxHeight, margem) => {
        let pontos = [{x: 0, y: 0}];
        let x = 0, y = 0, angulo = 0;
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const distancia = parseFloat(el.valor);
                const radianos = (angulo - 90) * Math.PI / 180;
                x += distancia * Math.cos(radianos);
                y += distancia * Math.sin(radianos);
                pontos.push({x, y});
            } else if (el.tipo === 'rotacao') {
                angulo += parseFloat(el.valor) * (el.direcao === 'direita' ? 1 : -1);
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        const minX = Math.min(...pontos.map(p => p.x));
        const maxX = Math.max(...pontos.map(p => p.x));
        const minY = Math.min(...pontos.map(p => p.y));
        const maxY = Math.max(...pontos.map(p => p.y));
        const largura = maxX - minX;
        const altura = maxY - minY;
        
        const escala = (largura < 1 && altura < 1) ? 10 : 
            Math.min(
                largura > 0 ? (viewBoxWidth - 2 * margem) / largura : 10,
                altura > 0 ? (viewBoxHeight - 2 * margem) / altura : 10
            );
        
        const offsetX = viewBoxWidth / 2 - ((minX + maxX) / 2) * escala;
        const offsetY = viewBoxHeight / 2 - ((minY + maxY) / 2) * escala;
        
        return { pontos, escala, offsetX, offsetY };
    };

    // Desenhar trajetória no mapa principal
    const desenharTrajetoria = (elementos) => {
        if (!svgTrajetoria) return;
        
        if (elementos.length === 0) {
            svgTrajetoria.innerHTML = '';
            if (mapaInfo.distancia) mapaInfo.distancia.textContent = 'Distância do percurso: 0m';
            if (mapaInfo.tempo) mapaInfo.tempo.textContent = 'Tempo estimado do percurso: 0s';
            
            const caminhao = $('.trajetoria .truck-icon-large');
            const trofeu = $('.trajetoria .award-icon-large');
            if (caminhao) { caminhao.style.left = '15%'; caminhao.style.bottom = '10%'; }
            if (trofeu) { trofeu.style.left = '85%'; trofeu.style.bottom = '10%'; }
            return;
        }
        
        const viewBoxWidth = 400, viewBoxHeight = 350;
        const { pontos, escala, offsetX, offsetY } = calcularTrajetoria(elementos, viewBoxWidth, viewBoxHeight, 30);
        
        let x = 0, y = 0, angulo = 0;
        const pontoInicialX = offsetX, pontoInicialY = offsetY;
        let path = `M ${pontoInicialX} ${pontoInicialY}`;
        let pontoFinal = {x: pontoInicialX, y: pontoInicialY};
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const radianos = (angulo - 90) * Math.PI / 180;
                x += parseFloat(el.valor) * Math.cos(radianos);
                y += parseFloat(el.valor) * Math.sin(radianos);
                const novoX = x * escala + offsetX;
                const novoY = y * escala + offsetY;
                path += ` L ${novoX} ${novoY}`;
                pontoFinal = {x: novoX, y: novoY};
            } else if (el.tipo === 'rotacao') {
                angulo += parseFloat(el.valor) * (el.direcao === 'direita' ? 1 : -1);
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        svgTrajetoria.innerHTML = `<path d="${path}" stroke="#313C41" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`;
        
        const atualizarIcone = (icone, ponto) => {
            if (icone) {
                icone.style.left = `${(ponto.x / viewBoxWidth) * 100}%`;
                icone.style.bottom = `${((viewBoxHeight - ponto.y) / viewBoxHeight) * 100}%`;
                icone.style.transform = 'translate(-50%, 50%)';
            }
        };
        
        atualizarIcone($('.trajetoria .truck-icon-large'), {x: pontoInicialX, y: pontoInicialY});
        atualizarIcone($('.trajetoria .award-icon-large'), pontoFinal);
        
        const stats = calcularEstatisticasRota(elementos);
        if (mapaInfo.distancia) mapaInfo.distancia.textContent = `Distância do percurso: ${stats.distanciaMetros}m`;
        if (mapaInfo.tempo) mapaInfo.tempo.textContent = `Tempo estimado do percurso: ${Math.round(stats.distanciaTotal / 50)}s`;
    };

    // Gerar SVG para rotas anteriores
    const gerarSVGTrajetoria = (elementos) => {
        const viewBoxWidth = 316, viewBoxHeight = 211;
        if (elementos.length === 0) {
            return {
                path: '<path d="" stroke="#313C41" stroke-width="2" fill="none"/>',
                pontoInicial: {x: 158, y: 180},
                pontoFinal: {x: 158, y: 180},
                viewBox: {width: viewBoxWidth, height: viewBoxHeight}
            };
        }
        
        const { escala, offsetX, offsetY } = calcularTrajetoria(elementos, viewBoxWidth, viewBoxHeight, 30);
        let x = 0, y = 0, angulo = 0;
        const pontoInicialX = offsetX, pontoInicialY = offsetY;
        let path = `M ${pontoInicialX} ${pontoInicialY}`;
        let pontoFinal = {x: pontoInicialX, y: pontoInicialY};
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const radianos = (angulo - 90) * Math.PI / 180;
                x += parseFloat(el.valor) * Math.cos(radianos);
                y += parseFloat(el.valor) * Math.sin(radianos);
                const novoX = x * escala + offsetX;
                const novoY = y * escala + offsetY;
                path += ` L ${novoX} ${novoY}`;
                pontoFinal = {x: novoX, y: novoY};
            } else if (el.tipo === 'rotacao') {
                angulo += parseFloat(el.valor) * (el.direcao === 'direita' ? 1 : -1);
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        return {
            path: `<path d="${path}" stroke="#313C41" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`,
            pontoInicial: {x: pontoInicialX, y: pontoInicialY},
            pontoFinal,
            viewBox: {width: viewBoxWidth, height: viewBoxHeight}
        };
    };
    // Calcular estatísticas da rota
    const calcularEstatisticasRota = (elementos) => {
        let distanciaTotal = 0, tempoEstimado = 0;
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const dist = parseFloat(el.valor);
                distanciaTotal += dist;
                tempoEstimado += dist / 50; // 1.8 km/h = 50 cm/s
            } else if (el.tipo === 'rotacao') {
                tempoEstimado += parseFloat(el.valor) / 90; // 1s por 90°
            }
        });
        
        return {
            distanciaTotal: Math.round(distanciaTotal),
            distanciaMetros: Math.round(distanciaTotal / 10) / 10,
            tempoMinutos: Math.round(tempoEstimado / 60),
            velocidadeMedia: 1.8,
            consumo: Math.round(distanciaTotal * 0.5) / 10 // 50 Wh/m
        };
    };

    // Renderizar rotas anteriores
    const renderizarRotasAnteriores = () => {
        if (!rotasAnterioresContainer) return;
        
        if (rotas.length === 0) {
            rotasAnterioresContainer.innerHTML = '<div style="padding: 40px; text-align: center; color: #666; font-size: 20px; grid-column: 1 / -1;">Nenhuma rota registrada ainda</div>';
            atualizarEstatisticasGlobais();
            return;
        }
        
        rotasAnterioresContainer.innerHTML = rotas.slice(-5).reverse().map((rota, index) => {
            const stats = calcularEstatisticasRota(rota.elementos);
            const svg = gerarSVGTrajetoria(rota.elementos);
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
                                <svg class="map-route" width="100%" height="100%" viewBox="0 0 316 211" preserveAspectRatio="xMidYMid meet" fill="none">${svg.path}</svg>
                                <div class="truck-icon" style="left:${calcPos(svg.pontoInicial.x, svg.viewBox.width)}%; bottom:${calcBottom(svg.pontoInicial.y, svg.viewBox.height)}%; transform:translate(-50%,50%)">🚚</div>
                                <div class="award-icon" style="left:${calcPos(svg.pontoFinal.x, svg.viewBox.width)}%; bottom:${calcBottom(svg.pontoFinal.y, svg.viewBox.height)}%; transform:translate(-50%,50%)">🏆</div>
                            </div>
                        </div>
                    </div>
                </div>
            `;
        }).join('');
        
        atualizarEstatisticasGlobais();
    };

    // Atualizar estatísticas globais (SOC)
    const atualizarEstatisticasGlobais = () => {
        const capacidadeBateria = 10000;
        let consumoTotal = 0, distanciaTotal = 0;
        
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
    };

    // Adicionar elemento
    const adicionarElemento = (tipo, valor, direcao = null) => {
        const elemento = { tipo, valor, id: Date.now() };
        if (tipo === 'rotacao') elemento.direcao = direcao;
        rotaAtual.elementos.push(elemento);
        renderizarElementos();
        desenharTrajetoria(rotaAtual.elementos);
    };

    // Event listeners - Adicionar
    if (btnAddDistancia) {
        btnAddDistancia.addEventListener('click', () => {
            const valor = inputDistancia.value.trim();
            if (valor && parseFloat(valor) > 0) {
                adicionarElemento('distancia', valor);
                inputDistancia.value = '';
            } else alert('Insira um valor válido para a distância');
        });
    }

    if (btnAddRotacao) {
        btnAddRotacao.addEventListener('click', () => {
            const valor = inputRotacao.value.trim();
            if (valor && parseFloat(valor) > 0) {
                adicionarElemento('rotacao', valor, selectDirecao.value);
                inputRotacao.value = '';
            } else alert('Insira um valor válido para a rotação');
        });
    }

    // Renderizar elementos
    const renderizarElementos = () => {
        elementosContainer.innerHTML = rotaAtual.elementos.map((el, i) => {
            const content = el.tipo === 'distancia' 
                ? `<p class="elemento-label">Distância</p><p class="elemento-value">${el.valor}</p>`
                : `<p class="elemento-label">Girar ${el.valor}°</p><p class="elemento-value">Direção: ${el.direcao === 'direita' ? 'Direita ➡️' : 'Esquerda ⬅️'}</p>`;
            
            return `<div class="elemento-item ${el.tipo}" data-id="${el.id}">
                <div class="elemento-content">${content}</div>
                <button class="btn-remover" data-id="${el.id}">-</button>
                <span class="elemento-numero">${i + 1}</span>
            </div>`;
        }).join('');

        $$('.btn-remover').forEach(btn => {
            btn.addEventListener('click', () => {
                rotaAtual.elementos = rotaAtual.elementos.filter(el => el.id !== parseInt(btn.dataset.id));
                renderizarElementos();
                desenharTrajetoria(rotaAtual.elementos);
            });
        });
    };

    // Botões de ação
    if (btnConcluir) {
        btnConcluir.addEventListener('click', () => {
            if (rotaAtual.elementos.length === 0) return alert('Adicione pelo menos um elemento à rota');
            
            const rota = {id: Date.now(), elementos: [...rotaAtual.elementos], dataHora: new Date().toISOString() };
            enviarRota(rota);
            rotaAtual.elementos = [];
            renderizarElementos();
            desenharTrajetoria([]);
            alert('Rota enviada com sucesso!');
        });
    }

    if (btnLimparRota) {
        btnLimparRota.addEventListener('click', () => {
            if (confirm('Deseja limpar todos os elementos da rota atual?')) {
                rotaAtual.elementos = [];
                renderizarElementos();
                desenharTrajetoria([]);
            }
        });
    }

    if (btnLimparRotas) {
        btnLimparRotas.addEventListener('click', () => {
            if (confirm('Deseja limpar todas as rotas anteriores? Isso afetará todos os usuários conectados.')) {
                // Enviar comando para ESP32 limpar as rotas armazenadas
                if (wsConnected) {
                    websocket.send(JSON.stringify({ channel: "LIMPAR_ROTAS", deviceId: deviceId }));
                }
            }
        });
    }
    
    // ===== INÍCIO: JS ADICIONADO PARA A PORTA =====
    if (btnAbrirPorta) {
        btnAbrirPorta.addEventListener('click', () => {
            console.log('Enviando comando ABRIR');
            if (wsConnected) {
                // O backend espera um JSON com "channel"
                websocket.send(JSON.stringify({ channel: "ABRIR", deviceId: deviceId }));
            } else {
                alert('WebSocket não conectado!');
            }
        });
    }

    if (btnFecharPorta) {
        btnFecharPorta.addEventListener('click', () => {
            console.log('Enviando comando FECHAR');
            if (wsConnected) {
                // O backend espera um JSON com "channel"
                websocket.send(JSON.stringify({ channel: "FECHAR", deviceId: deviceId }));
            } else {
                alert('WebSocket não conectado!');
            }
        });
    }
    // ===== FIM: JS ADICIONADO PARA A PORTA =====


    // Inicialização
    renderizarRotasAnteriores();
    desenharTrajetoria([]);

    // Enter para adicionar
    if (inputDistancia) inputDistancia.addEventListener('keypress', e => e.key === 'Enter' && btnAddDistancia.click());
    if (inputRotacao) inputRotacao.addEventListener('keypress', e => e.key === 'Enter' && btnAddRotacao.click());

    // ===== Sistema de Identificação de Dispositivo =====
    // Gerar ou recuperar ID único do dispositivo
    function getDeviceId() {
        let deviceId = localStorage.getItem('cegoinha_device_id');
        if (!deviceId) {
            // Gerar novo ID único baseado em timestamp + random
            deviceId = 'device_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
            localStorage.setItem('cegoinha_device_id', deviceId);
            console.log('🆔 Novo dispositivo criado:', deviceId);
        } else {
            console.log('🆔 Dispositivo reconhecido:', deviceId);
        }
        return deviceId;
    }

    // WebSocket - Configuração com reconexão automática e identificação
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    var wsConnected = false;
    var reconnectInterval = null;
    var deviceId = getDeviceId();
    var sessionId = 'session_' + Date.now();
    
    window.addEventListener('load', onLoad);
    
    function initWebSocket() {
        console.log('🔄 Tentando conectar ao WebSocket...');
        console.log('🆔 Device ID:', deviceId);
        console.log('🔑 Session ID:', sessionId);
        websocket = new WebSocket(gateway);
        websocket.onopen    = onOpen;
        websocket.onclose   = onClose;
        websocket.onerror   = onError;
        websocket.onmessage = onMessage;
    }
    
    function onOpen(event) {
        console.log('✅ WebSocket conectado!');
        wsConnected = true;
        
        // Enviar identificação do dispositivo para ESP32
        const identificacao = {
            channel: 'DEVICE_ID',
            deviceId: deviceId,
            sessionId: sessionId,
            timestamp: Date.now()
        };
        websocket.send(JSON.stringify(identificacao));
        console.log('📤 Identificação enviada para ESP32');
        
        // Limpar intervalo de reconexão se existir
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
        
        // Atualizar UI para mostrar status conectado
        document.title = 'Cegoinha 🕊️ - Conectado';
    }
    
    function onClose(event) {
        console.log('❌ WebSocket desconectado');
        wsConnected = false;
        document.title = 'Cegoinha 🕊️ - Desconectado';
        
        // Tentar reconectar após 2 segundos
        console.log('⏳ Reconectando em 2 segundos...');
        setTimeout(initWebSocket, 2000);
    }
    
    function onError(event) {
        console.error('❌ Erro no WebSocket:', event);
        wsConnected = false;
    }
    
    function onMessage(event) {
        console.log('📨 Mensagem recebida:', event.data);
        
        try {
            const data = JSON.parse(event.data);
            
            // Sincronizar rotas do servidor
            if (data.channel === 'SYNC_ROTAS') {
                console.log('🔄 Sincronizando rotas do servidor...');
                rotas = data.rotas || [];
                renderizarRotasAnteriores();
                console.log(`✅ ${rotas.length} rotas sincronizadas`);
            }
            
            // Nova rota adicionada por outro usuário
            else if (data.channel === 'NOVA_ROTA') {
                console.log('📥 Nova rota recebida de outro usuário');
                rotas.push(data.rota);
                renderizarRotasAnteriores();
                atualizarEstatisticasGlobais();
            }
            
            // Rotas foram limpas
            else if (data.channel === 'ROTAS_LIMPAS') {
                console.log('🗑️ Rotas limpas por outro usuário');
                rotas = [];
                renderizarRotasAnteriores();
            }
            
            // Confirmação de dispositivo identificado
            else if (data.status === 'ok' && data.message === 'Dispositivo identificado') {
                console.log(`✅ Dispositivo identificado: ${data.deviceId}`);
            }

            // ===== INÍCIO: JS ADICIONADO PARA A PORTA =====
            // Receber status da porta (que o loop() envia)
            else if (data.channel === 'STATUS_PORTA') {
                console.log('Status da porta atualizado:', data.status);
                if (statusPortaInfo) {
                    statusPortaInfo.textContent = `A porta está: ${data.status}`;
                }
            }
            
            // Receber mensagens de info/status da porta
            else if (data.status === 'ok' || data.status === 'info') {
                 if (data.message && data.message.includes('Porta')) {
                    console.log('Info da porta:', data.message);
                    if (statusPortaInfo) {
                        statusPortaInfo.textContent = data.message;
                    }
                 }
            }
            // ===== FIM: JS ADICIONADO PARA A PORTA =====
            
        } catch (e) {
            // Mensagem não é JSON, tratar como texto simples
            console.log('Mensagem texto:', event.data);
        }
    }
    
    // Enviar rota
    const enviarRota = (rota) => {
        if (!wsConnected) {
            alert('⚠️ WebSocket não está conectado. Tentando reconectar...');
            initWebSocket();
            return;
        }
        const comandos = rota.elementos.map(el => ({
            tipo: el.tipo === 'distancia' ? 'MOVE' : 'ROTATE',
            ...(el.tipo === 'distancia' ? {valor: parseInt(el.valor)} : {angulo: parseInt(el.valor), direcao: el.direcao})
        }));
        console.log('📤 Enviando rota para ESP32:', comandos);
        websocket.send(JSON.stringify({ 
            channel: "ENVIAR_ROTAS", 
            value: comandos,
            deviceId: deviceId,
            rotaId: rota.id
        }));
    };

    function onLoad(event) {
        initWebSocket();
    }
    
    function initButton(){
        // Função mantida para compatibilidade
    }
    
    function toggle(id){
        if (wsConnected) {
            websocket.send(JSON.stringify({'message':'toggle', 'id': id}));
        }
    }
});
    </script>
</body>
</html>
)rawliteral";

// ===== Funções LittleFS para Persistência de Rotas =====

// Salvar rotas no LittleFS
void salvarRotasLittleFS() {
  DynamicJsonDocument doc(8192);
  JsonArray rotasArray = doc.createNestedArray("rotas");
  
  for (const auto& rota : rotasArmazenadas) {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;
    rotaObj["deviceId"] = rota.deviceId;
    
    JsonArray comandosArray = rotaObj.createNestedArray("comandos");
    for (const auto& cmd : rota.comandos) {
      JsonObject cmdObj = comandosArray.createNestedObject();
      cmdObj["tipo"] = cmd.tipo;
      cmdObj["valor"] = cmd.valor;
      if (cmd.tipo == "ROTATE") {
        cmdObj["direcao"] = cmd.direcao;
      }
    }
  }
  
  File file = LittleFS.open(ROTAS_FILE, "w");
  if (!file) {
    Serial.println("❌ Erro ao abrir arquivo para escrita");
    return;
  }
  
  serializeJson(doc, file);
  file.close();
  Serial.printf("💾 %d rotas salvas no LittleFS\n", rotasArmazenadas.size());
}

// Carregar rotas do LittleFS
void carregarRotasLittleFS() {
  if (!LittleFS.exists(ROTAS_FILE)) {
    Serial.println("📂 Nenhum arquivo de rotas encontrado");
    return;
  }
  
  File file = LittleFS.open(ROTAS_FILE, "r");
  if (!file) {
    Serial.println("❌ Erro ao abrir arquivo para leitura");
    return;
  }
  
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.print("❌ Erro ao parsear JSON: ");
    Serial.println(error.c_str());
    return;
  }
  
  rotasArmazenadas.clear();
  JsonArray rotasArray = doc["rotas"];
  
  for (JsonObject rotaObj : rotasArray) {
    Rota rota;
    rota.id = rotaObj["id"];
    rota.dataHora = rotaObj["dataHora"].as<String>();
    rota.deviceId = rotaObj["deviceId"].as<String>();
    
    JsonArray comandosArray = rotaObj["comandos"];
    for (JsonObject cmdObj : comandosArray) {
      ComandoRota cmd;
      cmd.tipo = cmdObj["tipo"].as<String>();
      cmd.valor = cmdObj["valor"];
      if (cmd.tipo == "ROTATE") {
        cmd.direcao = cmdObj["direcao"].as<String>();
      }
      rota.comandos.push_back(cmd);
    }
    
    rotasArmazenadas.push_back(rota);
  }
  
  Serial.printf("✅ %d rotas carregadas do LittleFS\n", rotasArmazenadas.size());
}

// Enviar todas as rotas para um cliente específico
void enviarRotasParaCliente(AsyncWebSocketClient *client) {
  DynamicJsonDocument doc(8192);
  doc["channel"] = "SYNC_ROTAS";
  JsonArray rotasArray = doc.createNestedArray("rotas");
  
  for (const auto& rota : rotasArmazenadas) {
    JsonObject rotaObj = rotasArray.createNestedObject();
    rotaObj["id"] = rota.id;
    rotaObj["dataHora"] = rota.dataHora;
    
    JsonArray elementosArray = rotaObj.createNestedArray("elementos");
    for (const auto& cmd : rota.comandos) {
      JsonObject elemObj = elementosArray.createNestedObject();
      elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
      elemObj["valor"] = cmd.valor;
      elemObj["id"] = millis();
      if (cmd.tipo == "ROTATE") {
        elemObj["direcao"] = cmd.direcao;
      }
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  client->text(jsonString);
  Serial.printf("📤 Rotas sincronizadas para cliente #%u\n", client->id());
}

// ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
// --- Funções de Baixo Nível do Motor da Porta ---

// Para o motor (freio)
void pararPorta() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  ledcWrite(canalPWM_Porta, 0); // Desliga a velocidade
}

// Gira em um sentido (Ex: Abrir)
void abrirPortaLogica(int velocidade) { 
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  ledcWrite(canalPWM_Porta, velocidade); // Define a velocidade
}

// Gira no outro sentido (Ex: Fechar)
void fecharPortaLogica(int velocidade) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  ledcWrite(canalPWM_Porta, velocidade); // Define a velocidade
}
// ===== FIM: CÓDIGO DA PORTA ADICIONADO =====


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
  void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT: //executado quando cliente novo entra
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      // Enviar rotas existentes para o novo cliente após um pequeno delay
      // (aguardar identificação do dispositivo)
      break;
    case WS_EVT_DISCONNECT: //executado quando cliente desconecta
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      // Remover dispositivo da lista ao desconectar
      for (size_t i = 0; i < dispositivosConectados.size(); i++) {
        if (dispositivosConectados[i].clientId == client->id()) {
          Serial.printf("📤 Dispositivo %s desconectado\n", dispositivosConectados[i].deviceId.c_str());
          dispositivosConectados.erase(dispositivosConectados.begin() + i);
          break;
        }
      }
      break;
    case WS_EVT_DATA: //executado quando chega mensagem
      mensagemRecebida(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}
void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


void mensagemRecebida(AsyncWebSocketClient *client, void *metadados, uint8_t *mensagem, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)metadados;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) { //verifica se recebe só texto
    
    // Aumentar tamanho do buffer JSON para acomodar rotas maiores
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, mensagem, len);

    if (error) {
      Serial.print("Falha ao ler JSON: ");
      Serial.println(error.c_str());
      return;
    }
    
    if (!doc.containsKey("channel")) {
      Serial.println("JSON recebido não contém a chave 'channel'.");
      return;
    }

    const char* channel = doc["channel"];
    Serial.printf("Canal recebido: %s\n", channel);

    // Processar identificação de dispositivo
    if (strcmp(channel, "DEVICE_ID") == 0) {
      String deviceId = doc["deviceId"].as<String>();
      String sessionId = doc["sessionId"].as<String>();
      
      // Verificar se dispositivo já existe
      bool dispositivoExistente = false;
      for (auto &disp : dispositivosConectados) {
        if (disp.deviceId == deviceId) {
          // Atualizar informações do dispositivo existente
          disp.clientId = client->id();
          disp.sessionId = sessionId;
          disp.lastSeen = millis();
          dispositivoExistente = true;
          Serial.printf("🔄 Dispositivo reconectado: %s (Client #%u)\n", deviceId.c_str(), client->id());
          break;
        }
      }
      
      if (!dispositivoExistente) {
        // Adicionar novo dispositivo
        DispositivoConectado novoDispositivo;
        novoDispositivo.clientId = client->id();
        novoDispositivo.deviceId = deviceId;
        novoDispositivo.sessionId = sessionId;
        novoDispositivo.lastSeen = millis();
        dispositivosConectados.push_back(novoDispositivo);
        Serial.printf("🆕 Novo dispositivo registrado: %s (Client #%u)\n", deviceId.c_str(), client->id());
      }
      
      Serial.printf("📊 Total de dispositivos: %d\n", dispositivosConectados.size());
      
      // Enviar confirmação
      String resposta = "{\"status\":\"ok\",\"message\":\"Dispositivo identificado\",\"deviceId\":\"" + deviceId + "\"}";
      client->text(resposta);
      
      // Enviar rotas existentes para sincronização
      enviarRotasParaCliente(client);
      return;
    }

    // Processar envio de rotas
    if (strcmp(channel, "ENVIAR_ROTAS") == 0) {
      if (!doc.containsKey("value")) {
        Serial.println("Erro: 'value' não encontrado para ENVIAR_ROTAS");
        return;
      }

      String deviceId = doc.containsKey("deviceId") ? doc["deviceId"].as<String>() : "unknown";
      JsonArray comandosArray = doc["value"].as<JsonArray>();
      
      // Criar nova rota
      Rota novaRota;
      novaRota.id = doc.containsKey("rotaId") ? doc["rotaId"].as<unsigned long>() : millis();
      novaRota.dataHora = String(novaRota.id);
      novaRota.deviceId = deviceId;
      
      Serial.println("=== Nova Rota Recebida ===");
      Serial.printf("Device ID: %s\n", deviceId.c_str());
      Serial.printf("ID da Rota: %lu\n", novaRota.id);
      Serial.printf("Total de comandos: %d\n", comandosArray.size());
      
      // Processar cada comando
      for (JsonObject comandoObj : comandosArray) {
        ComandoRota comando;
        comando.tipo = comandoObj["tipo"].as<String>();
        
        if (comando.tipo == "MOVE") {
          comando.valor = comandoObj["valor"];
          Serial.printf("  - MOVE: %d\n", comando.valor);
        } else if (comando.tipo == "ROTATE") {
          comando.valor = comandoObj["angulo"];
          comando.direcao = comandoObj["direcao"].as<String>();
          Serial.printf("  - ROTATE: %d° para %s\n", comando.valor, comando.direcao.c_str());
        }
        
        novaRota.comandos.push_back(comando);
      }
      
      // Adicionar rota ao armazenamento
      rotasArmazenadas.push_back(novaRota);
      Serial.printf("Rota armazenada! Total de rotas: %d\n", rotasArmazenadas.size());
      
      // Salvar no LittleFS
      salvarRotasLittleFS();
      
      Serial.println("==========================\n");
      
      // Notificar TODOS os clientes sobre a nova rota
      DynamicJsonDocument notifDoc(2048);
      notifDoc["channel"] = "NOVA_ROTA";
      notifDoc["rotaId"] = novaRota.id;
      notifDoc["totalRotas"] = rotasArmazenadas.size();
      
      JsonObject rotaObj = notifDoc.createNestedObject("rota");
      rotaObj["id"] = novaRota.id;
      rotaObj["dataHora"] = novaRota.dataHora;
      
      JsonArray elementosArray = rotaObj.createNestedArray("elementos");
      for (const auto& cmd : novaRota.comandos) {
        JsonObject elemObj = elementosArray.createNestedObject();
        elemObj["tipo"] = (cmd.tipo == "MOVE") ? "distancia" : "rotacao";
        elemObj["valor"] = cmd.valor;
        elemObj["id"] = millis() + random(1000);
        if (cmd.tipo == "ROTATE") {
          elemObj["direcao"] = cmd.direcao;
        }
      }
      
      String notifString;
      serializeJson(notifDoc, notifString);
      ws.textAll(notifString);
    }
    
    // Processar comando para limpar rotas
    else if (strcmp(channel, "LIMPAR_ROTAS") == 0) {
      String deviceId = doc.containsKey("deviceId") ? doc["deviceId"].as<String>() : "unknown";
      int totalRotasAntes = rotasArmazenadas.size();
      rotasArmazenadas.clear();
      
      // Limpar arquivo LittleFS
      LittleFS.remove(ROTAS_FILE);
      
      Serial.println("=== Rotas Limpas ===");
      Serial.printf("Device ID: %s\n", deviceId.c_str());
      Serial.printf("Rotas removidas: %d\n", totalRotasAntes);
      Serial.println("====================\n");
      
      // Notificar TODOS os clientes
      String resposta = "{\"channel\":\"ROTAS_LIMPAS\",\"status\":\"ok\",\"rotasRemovidas\":" + String(totalRotasAntes) + "}";
      ws.textAll(resposta);
    }
    
    // Processar comando para listar rotas armazenadas
    else if (strcmp(channel, "LISTAR_ROTAS") == 0) {
      Serial.println("=== Rotas Armazenadas ===");
      Serial.printf("Total: %d rotas\n", rotasArmazenadas.size());
      
      for (size_t i = 0; i < rotasArmazenadas.size(); i++) {
        Serial.printf("\nRota %d (ID: %lu):\n", i + 1, rotasArmazenadas[i].id);
        Serial.printf("  Comandos: %d\n", rotasArmazenadas[i].comandos.size());
        
        for (size_t j = 0; j < rotasArmazenadas[i].comandos.size(); j++) {
          ComandoRota cmd = rotasArmazenadas[i].comandos[j];
          if (cmd.tipo == "MOVE") {
            Serial.printf("    %d. MOVE %d\n", j + 1, cmd.valor);
          } else {
            Serial.printf("    %d. ROTATE %d° %s\n", j + 1, cmd.valor, cmd.direcao.c_str());
          }
        }
      }
      Serial.println("=========================\n");
    }

    // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====

    // Processar comando para ABRIR A PORTA
    else if (strcmp(channel, "ABRIR") == 0) {
      // Verifica se a porta já não está aberta (sensor HIGH = não pressionado)
      if (digitalRead(SENSOR_PORTA_ABERTA) == HIGH) { 
        Serial.println("Comando: ABRIR. Iniciando abertura...");
        estadoPorta = ESTADO_PORTA_ABRINDO; // Muda o estado
        client->text("{\"status\":\"ok\",\"message\":\"Comando 'ABRIR' recebido. Abrindo...\"}");
      } else {
        Serial.println("Comando: ABRIR. Porta já está aberta.");
        client->text("{\"status\":\"info\",\"message\":\"Porta ja esta aberta.\"}");
      }
    }

    // Processar comando para FECHAR A PORTA
    else if (strcmp(channel, "FECHAR") == 0) {
      // Verifica se a porta já não está fechada (sensor HIGH = não pressionado)
      if (digitalRead(SENSOR_PORTA_FECHADA) == HIGH) { 
        Serial.println("Comando: FECHAR. Iniciando fechamento...");
        estadoPorta = ESTADO_PORTA_FECHANDO; // Muda o estado
        client->text("{\"status\":\"ok\",\"message\":\"Comando 'FECHAR' recebido. Fechando...\"}");
      } else {
        Serial.println("Comando: FECHAR. Porta já está fechada.");
        client->text("{\"status\":\"info\",\"message\":\"Porta ja esta fechada.\"}");
      }
    }

    // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
    
    // Comando genérico
    else {
      float value = doc["value"];
      Serial.printf("Valor recebido: %f\n", value);
    }
  }
}


String setupVariables(const String& var){
  if (var == "VARIAVEL1"){ //Ai coloa %VARIAVEL1% no HTML, que ai vai ser substituida
    return "Valor da variável";
  }
  return String(); //para não crashar se nao existir a variável
}
void notifyClients(String value){
ws.textAll(String(value));
}


void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);
  
  Serial.println("\n\n=================================");
  Serial.println("    CEGOINHA ESP32 - Iniciando");
  Serial.println("=================================");
  
  // Inicializar LittleFS
  Serial.println("\n--- Inicializando LittleFS ---");
  if (!LittleFS.begin(true)) {
    Serial.println("❌ Erro ao montar LittleFS");
    Serial.println("⚠️ Sistema continuará sem persistência");
  } else {
    Serial.println("✅ LittleFS montado com sucesso");
    
    // Mostrar informações do sistema de arquivos
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    Serial.printf("📊 Espaço total: %d bytes\n", totalBytes);
    Serial.printf("📊 Espaço usado: %d bytes (%.1f%%)\n", usedBytes, (usedBytes * 100.0) / totalBytes);
  }
  Serial.println("------------------------------\n");

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
  Serial.println("--- Setup do Motor da Porta ---");
  // --- Setup do Motor ---
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  // Configura o PWM para o pino ENA
  ledcSetup(canalPWM_Porta, freqPWM_Porta, resolucaoPWM_Porta);
  ledcAttachPin(PIN_ENA, canalPWM_Porta);
  // Garante que o motor comece parado
  pararPorta(); 
  Serial.println("✅ Driver L298N (Porta) configurado.");

  // --- Setup dos Sensores ---
  // INPUT_PULLUP: O pino fica em HIGH (1) por padrão.
  // Quando o sensor é pressionado, ele aterra o pino, que lê LOW (0).
  pinMode(SENSOR_PORTA_ABERTA, INPUT_PULLUP);
  pinMode(SENSOR_PORTA_FECHADA, INPUT_PULLUP);
  Serial.println("✅ Sensores Fim de Curso (Porta) configurados.");
  Serial.println("------------------------------\n");
  // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
  
  // Inicializar vetores
  rotasArmazenadas.clear();
  dispositivosConectados.clear();
  Serial.println("✅ Sistema de armazenamento de rotas inicializado");
  Serial.println("✅ Sistema de identificação de dispositivos inicializado");
  
  // Carregar rotas salvas
  carregarRotasLittleFS();
  
  // Connect to Wi-Fi
  WiFi.softAP(ssid,password);
  
  // Print IP address and start web server
  Serial.println("\n--- Configuração de Rede ---");
  Serial.println("Modo: Access Point");
  Serial.printf("SSID: %s\n", ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("----------------------------\n");

  initWebSocket();

  // Rota para enviar a página web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html, setupVariables);
  });

  // Start server
  server.begin();
  
  Serial.println("Servidor Web iniciado!");
  Serial.println("Aguardando conexões...\n");
  Serial.println("=================================\n");
}

void loop() {
  ws.cleanupClients();

  // ===== INÍCIO: CÓDIGO DA PORTA ADICIONADO =====
  
  // --- MÁQUINA DE ESTADOS DO MOTOR DA PORTA ---
  // Esta parte roda continuamente, verificando o estado da porta
  // sem usar 'delay()' ou 'while()', permitindo que o WebSocket
  // e o servidor web continuem funcionando.

  switch (estadoPorta) {
    
    case ESTADO_PORTA_ABRINDO:
      // Se estamos abrindo, verificamos o sensor de porta aberta
      if (digitalRead(SENSOR_PORTA_ABERTA) == LOW) { // LOW = Pressionado
        // Chegamos ao fim!
        Serial.println("Fim de curso: Porta totalmente aberta.");
        pararPorta();
        estadoPorta = ESTADO_PORTA_PARADO;
        // Avisa todos os clientes que a porta terminou de abrir
        ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"ABERTA\"}"); 
      } else {
        // Ainda não chegamos, continuar abrindo
        abrirPortaLogica(velocidadeMotorPorta);
      }
      break;

    case ESTADO_PORTA_FECHANDO:
      // Se estamos fechando, verificamos o sensor de porta fechada
      if (digitalRead(SENSOR_PORTA_FECHADA) == LOW) { // LOW = Pressionado
        // Chegamos ao fim!
        Serial.println("Fim de curso: Porta totalmente fechada.");
        pararPorta();
        estadoPorta = ESTADO_PORTA_PARADO;
        // Avisa todos os clientes que a porta terminou de fechar
        ws.textAll("{\"channel\":\"STATUS_PORTA\",\"status\":\"FECHADA\"}");
      } else {
        // Ainda não chegamos, continuar fechando
        fecharPortaLogica(velocidadeMotorPorta);
      }
      break;

    case ESTADO_PORTA_PARADO:
      // Não faz nada. O motor já está parado.
      break;
  }
  // ===== FIM: CÓDIGO DA PORTA ADICIONADO =====
}

// Função auxiliar para obter informações de uma rota específica
String getRotaInfo(size_t indice) {
  if (indice >= rotasArmazenadas.size()) {
    return "Rota não encontrada";
  }
  
  Rota rota = rotasArmazenadas[indice];
  String info = "Rota ID: " + String(rota.id) + "\n";
  info += "Comandos: " + String(rota.comandos.size()) + "\n";
  
  for (size_t i = 0; i < rota.comandos.size(); i++) {
    ComandoRota cmd = rota.comandos[i];
    info += "  " + String(i + 1) + ". ";
    if (cmd.tipo == "MOVE") {
      info += "MOVE " + String(cmd.valor) + "\n";
    } else {
      info += "ROTATE " + String(cmd.valor) + "° " + cmd.direcao + "\n";
    }
  }
  
  return info;
}