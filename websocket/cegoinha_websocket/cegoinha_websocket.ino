/*
  Cegoinha - Sistema de controle integrado com WebSocket
  ESP32 com interface web para controle de rotas
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Credenciais WiFi
const char* ssid = "cegoinha";
const char* password = "cegoinha123";

// Estado do LED
bool ledState = 0;
const int ledPin = 2;

// Servidor e WebSocket
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// HTML do Cegoinha (minificado e otimizado para ESP32)
const char index_html[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Cegoinha</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}body{font-family:Arial,sans-serif;background:#fff;color:#000;line-height:1.5;overflow-x:hidden}.main-container{width:100%;min-height:100vh;padding:10px;max-width:1400px;margin:0 auto}.header{padding:20px 0 10px 40px}.title{font-size:24px;font-weight:400}.divider{width:100%;height:3px;background:#313C41;margin:15px 0}.section{padding:15px 0;margin-bottom:15px;position:relative}.section-title{font-size:20px;margin-bottom:15px}.rotas-anteriores,.consumo-carga,.status-carrinho{padding-left:40px}.rotas-grid{display:flex;gap:15px;margin-bottom:15px;flex-wrap:wrap}.card-rota{width:320px;height:380px;border-radius:8px;overflow:hidden;flex-shrink:0}.rota-header{background:#313C41;padding:8px 20px;display:flex;justify-content:space-between;align-items:center;border-radius:8px 8px 0 0;height:36px}.rota-name{font-size:18px;color:#F6F6F6}.rota-distance{font-size:16px;color:#F6F6F6}.rota-body{background:#C8C8C8;padding:15px;height:344px;border-radius:0 0 8px 8px}.rota-time,.rota-vmedia,.rota-consumo{font-size:14px;text-align:center;margin-bottom:6px}.mapa-container{width:100%;height:220px;margin-top:8px}.mapa-placeholder,.mapa-grande{background:#EC8A8A;border-radius:12px;width:100%;height:100%;position:relative;display:flex;align-items:center;justify-content:center;overflow:hidden}.map-route,.map-route-large{position:absolute;top:0;left:0;width:100%;height:100%;z-index:1;pointer-events:none}.truck-icon,.award-icon,.truck-icon-large,.award-icon-large{position:absolute;font-size:24px;z-index:3;transition:all .3s ease}.truck-icon-large,.award-icon-large{font-size:32px}.btn-rotas-limpar{margin-left:320px;margin-top:8px}.consumo-list{list-style:disc;padding-left:30px;font-size:16px}.consumo-list li{margin-bottom:6px}.status-grid{display:flex;gap:12px;flex-wrap:wrap}.status-card{width:280px;height:100px;border-radius:8px;overflow:hidden;flex-shrink:0}.status-header{background:#313C41;color:#F6F6F6;font-size:18px;text-align:center;padding:6px;border-radius:8px 8px 0 0;height:38px;display:flex;align-items:center;justify-content:center}.status-value{background:#C8C8C8;font-size:24px;text-align:center;padding:15px;border-radius:0 0 8px 8px;height:62px;display:flex;align-items:center;justify-content:center}.main-content{display:grid;grid-template-columns:1fr 1fr;gap:30px;padding:0 40px;margin-bottom:20px}.enviar-rota,.trajetoria{padding:0}.elementos-adicionados{display:flex;flex-direction:column;gap:8px;margin-bottom:15px;min-height:60px}.elementos-adicionados:empty::before{content:"Adicione elementos";display:flex;align-items:center;justify-content:center;height:60px;color:#999;font-size:14px;font-style:italic}.elemento-item{height:60px;border-radius:0 8px 8px 0;display:flex;align-items:center;padding:0 8px 0 12px;position:relative}.elemento-item.distancia{background:#31A208;width:180px}.elemento-item.rotacao{background:#0026B8;width:320px}.elemento-content{flex:1;margin-right:8px}.elemento-label,.elemento-label-small,.elemento-value{font-size:14px;color:#F6F6F6;line-height:1.2}.btn-remover,.btn-direcao,.btn-add{width:24px;height:24px;min-width:24px;background:#F6F6F6;border:none;border-radius:3px;color:#000;cursor:pointer;display:flex;align-items:center;justify-content:center;flex-shrink:0}.btn-remover{font-size:20px;line-height:1}.btn-direcao{font-size:12px;margin-left:4px}.btn-add{font-size:18px;position:absolute;left:8px;top:50%;transform:translateY(-50%)}.elemento-numero{position:absolute;right:6px;bottom:4px;font-size:10px;color:#F6F6F6}.adicionar-elementos{display:flex;gap:15px;margin-bottom:20px;flex-wrap:wrap}.adicionar-card{border-radius:8px;padding:8px;position:relative;height:60px;background:#C8C8C8}.adicionar-card.rotacao-add{width:320px}.adicionar-card.distancia-add{width:180px}.add-label,.add-label-top,.add-label-left{font-size:14px;position:absolute}.add-label{top:8px;left:38px}.add-label-top{top:6px;right:38px}.add-label-left{left:38px;top:6px}.add-input{position:absolute;bottom:12px;left:38px;right:38px;background:transparent;border:none;border-bottom:1px solid #666;font-size:14px;outline:none;padding:2px 4px}.distancia-add .add-input{right:12px}.add-input-graus{right:100px;width:60px}.add-select-direcao{position:absolute;bottom:12px;right:8px;width:88px;height:24px;background:#F6F6F6;border:1px solid #666;border-radius:4px;font-size:12px;outline:none;padding:2px 6px;cursor:pointer}.add-select-direcao:focus{border-color:#0026B8;box-shadow:0 0 2px rgba(0,38,184,.3)}.acoes{display:flex;gap:12px}.btn-concluir,.btn-limpar{color:#F6F6F6;font-size:14px;padding:10px 24px;border:none;border-radius:8px;cursor:pointer}.btn-concluir{background:#31A208}.btn-limpar{background:#A21508}.trajetoria-card{background:#0026B8;border-radius:15px;padding:20px;color:#F6F6F6}.trajetoria-info{font-size:14px;color:#F6F6F6;margin-bottom:6px}.mapa-grande{max-width:350px;height:280px;margin-top:15px}.status-percurso{padding-left:40px;clear:both}.status-percurso-section{margin-top:30px}.status-percurso-grid{display:grid;grid-template-columns:repeat(2,180px);gap:24px}.status-percurso-card{width:180px;height:120px;border-radius:8px;overflow:hidden}.status-percurso-card .status-header{height:50px;padding:10px}.status-percurso-card .status-value{font-size:20px;height:70px}button:hover{opacity:.85;transition:opacity .2s}button:active{transform:scale(.98)}@media(max-width:1400px){.main-content{grid-template-columns:1fr}.status-percurso-grid{grid-template-columns:repeat(2,180px)}}@media(max-width:768px){.header{padding:15px 0 15px 15px}.rotas-anteriores,.consumo-carga,.status-carrinho,.status-percurso{padding-left:15px}.main-content{padding:0 15px}.card-rota{width:100%;max-width:320px}.adicionar-elementos{flex-direction:column}.adicionar-card{width:100%!important;max-width:320px}.elemento-item{width:100%!important;max-width:320px}.status-grid{justify-content:center}.status-percurso-grid{grid-template-columns:repeat(auto-fit,180px);justify-content:center}}
</style>
</head>
<body>
<div class="main-container">
<div class="header"><p class="title">Cegoinha 🕊️</p></div>
<div class="divider"></div>
<section class="section rotas-anteriores">
<div class="section-header"><p class="section-title">Rotas Anteriores</p></div>
<div class="rotas-grid"></div>
<button class="btn-limpar btn-rotas-limpar">Limpar</button>
</section>
<div class="divider"></div>
<section class="section consumo-carga">
<p class="section-title">Consumo desde a última carga</p>
<ul class="consumo-list">
<li><span id="rotas-concluidas">0</span> rotas concluídas</li>
<li><span id="bateria-gasta">0</span> Wh gastos (<span id="porcentagem-gasta">0</span>%)</li>
<li><span id="distancia-total">0</span> cm andados</li>
</ul>
</section>
<div class="divider"></div>
<section class="section status-carrinho">
<p class="section-title">Status carrinho</p>
<div class="status-grid">
<div class="status-card"><div class="status-header">Consumo motor 1</div><div class="status-value">0 W</div></div>
<div class="status-card"><div class="status-header">Consumo motor 2</div><div class="status-value">0 W</div></div>
<div class="status-card"><div class="status-header">Bateria restante</div><div class="status-value"><span id="bateria-soc">100</span>%</div></div>
</div>
</section>
<div class="divider"></div>
<div class="main-content">
<section class="section enviar-rota">
<p class="section-title">Enviar rota</p>
<div class="elementos-adicionados"></div>
<div class="adicionar-elementos">
<div class="adicionar-card rotacao-add">
<p class="add-label-top">Direção</p>
<p class="add-label-left">Girar X°</p>
<input type="number" class="add-input add-input-graus" id="input-graus" placeholder="90" min="0" max="360">
<select class="add-select-direcao" id="select-direcao">
<option value="direita">Direita ➡️</option>
<option value="esquerda">Esquerda ⬅️</option>
</select>
<button class="btn-add" id="btn-add-rotacao">+</button>
</div>
<div class="adicionar-card distancia-add">
<p class="add-label">Distância</p>
<input type="number" class="add-input" id="input-distancia" placeholder="10" min="0">
<button class="btn-add" id="btn-add-distancia">+</button>
</div>
</div>
<div class="acoes">
<button class="btn-concluir">Concluir</button>
<button class="btn-limpar">Limpar</button>
</div>
</section>
<section class="section trajetoria">
<p class="section-title">Trajetória</p>
<div class="trajetoria-card">
<p class="trajetoria-info">Distância: 0m</p>
<p class="trajetoria-info">Tempo: 0s</p>
<div class="mapa-grande">
<svg class="map-route-large" width="100%" height="100%" viewBox="0 0 400 350" preserveAspectRatio="xMidYMid meet" fill="none"></svg>
<div class="truck-icon-large">🚚</div>
<div class="award-icon-large">🏆</div>
</div>
</div>
<div class="status-percurso-section">
<p class="section-title">Status percurso</p>
<div class="status-percurso-grid">
<div class="status-percurso-card"><div class="status-header">Velocidade</div><div class="status-value">4 km/h</div></div>
<div class="status-percurso-card"><div class="status-header">ETA</div><div class="status-value">10 min</div></div>
<div class="status-percurso-card"><div class="status-header">Consumo</div><div class="status-value">5 Wh</div></div>
<div class="status-percurso-card"><div class="status-header">ΔT</div><div class="status-value">5 min</div></div>
</div>
</div>
</section>
</div>
</div>
<script>
var gateway=`ws://${window.location.hostname}/ws`;var websocket;
document.addEventListener('DOMContentLoaded',()=>{
initWebSocket();
let rotas=[],rotaAtual={elementos:[]};const $=s=>document.querySelector(s),$$=s=>document.querySelectorAll(s),btnAddDistancia=$('#btn-add-distancia'),btnAddRotacao=$('#btn-add-rotacao'),inputDistancia=$('#input-distancia'),inputRotacao=$('#input-graus'),selectDirecao=$('#select-direcao'),btnConcluir=$('.btn-concluir'),btnLimparRota=$('.enviar-rota .btn-limpar'),btnLimparRotas=$('.btn-rotas-limpar'),elementosContainer=$('.elementos-adicionados'),rotasAnterioresContainer=$('.rotas-grid'),svgTrajetoria=$('.map-route-large'),mapaInfo={distancia:$$('.trajetoria-info')[0],tempo:$$('.trajetoria-info')[1]};
function initWebSocket(){console.log('Conectando WebSocket...');websocket=new WebSocket(gateway);websocket.onopen=()=>console.log('WebSocket conectado');websocket.onclose=()=>{console.log('WebSocket desconectado');setTimeout(initWebSocket,2000)};websocket.onerror=e=>console.error('Erro WebSocket:',e)}
const calcularTrajetoria=(elementos,viewBoxWidth,viewBoxHeight,margem)=>{let pontos=[{x:0,y:0}],x=0,y=0,angulo=0;elementos.forEach(el=>{if(el.tipo==='distancia'){const distancia=parseFloat(el.valor),radianos=(angulo-90)*Math.PI/180;x+=distancia*Math.cos(radianos);y+=distancia*Math.sin(radianos);pontos.push({x,y})}else if(el.tipo==='rotacao'){angulo+=parseFloat(el.valor)*(el.direcao==='direita'?1:-1);angulo=((angulo%360)+360)%360}});const minX=Math.min(...pontos.map(p=>p.x)),maxX=Math.max(...pontos.map(p=>p.x)),minY=Math.min(...pontos.map(p=>p.y)),maxY=Math.max(...pontos.map(p=>p.y)),largura=maxX-minX,altura=maxY-minY,escala=(largura<1&&altura<1)?10:Math.min(largura>0?(viewBoxWidth-2*margem)/largura:10,altura>0?(viewBoxHeight-2*margem)/altura:10),offsetX=viewBoxWidth/2-((minX+maxX)/2)*escala,offsetY=viewBoxHeight/2-((minY+maxY)/2)*escala;return{pontos,escala,offsetX,offsetY}};const desenharTrajetoria=elementos=>{if(!svgTrajetoria)return;if(elementos.length===0){svgTrajetoria.innerHTML='';if(mapaInfo.distancia)mapaInfo.distancia.textContent='Distância: 0m';if(mapaInfo.tempo)mapaInfo.tempo.textContent='Tempo: 0s';const caminhao=$('.trajetoria .truck-icon-large'),trofeu=$('.trajetoria .award-icon-large');if(caminhao){caminhao.style.left='15%';caminhao.style.bottom='10%'}if(trofeu){trofeu.style.left='85%';trofeu.style.bottom='10%'}return}const viewBoxWidth=400,viewBoxHeight=350,{pontos,escala,offsetX,offsetY}=calcularTrajetoria(elementos,viewBoxWidth,viewBoxHeight,30);let x=0,y=0,angulo=0;const pontoInicialX=offsetX,pontoInicialY=offsetY;let path=`M ${pontoInicialX} ${pontoInicialY}`,pontoFinal={x:pontoInicialX,y:pontoInicialY};elementos.forEach(el=>{if(el.tipo==='distancia'){const radianos=(angulo-90)*Math.PI/180;x+=parseFloat(el.valor)*Math.cos(radianos);y+=parseFloat(el.valor)*Math.sin(radianos);const novoX=x*escala+offsetX,novoY=y*escala+offsetY;path+=` L ${novoX} ${novoY}`;pontoFinal={x:novoX,y:novoY}}else if(el.tipo==='rotacao'){angulo+=parseFloat(el.valor)*(el.direcao==='direita'?1:-1);angulo=((angulo%360)+360)%360}});svgTrajetoria.innerHTML=`<path d="${path}" stroke="#313C41" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`;const atualizarIcone=(icone,ponto)=>{if(icone){icone.style.left=`${(ponto.x/viewBoxWidth)*100}%`;icone.style.bottom=`${((viewBoxHeight-ponto.y)/viewBoxHeight)*100}%`;icone.style.transform='translate(-50%,50%)'}};atualizarIcone($('.trajetoria .truck-icon-large'),{x:pontoInicialX,y:pontoInicialY});atualizarIcone($('.trajetoria .award-icon-large'),pontoFinal);const stats=calcularEstatisticasRota(elementos);if(mapaInfo.distancia)mapaInfo.distancia.textContent=`Distância: ${stats.distanciaMetros}m`;if(mapaInfo.tempo)mapaInfo.tempo.textContent=`Tempo: ${Math.round(stats.distanciaTotal/50)}s`};const gerarSVGTrajetoria=elementos=>{const viewBoxWidth=316,viewBoxHeight=211;if(elementos.length===0)return{path:'<path d="" stroke="#313C41" stroke-width="2" fill="none"/>',pontoInicial:{x:158,y:180},pontoFinal:{x:158,y:180},viewBox:{width:viewBoxWidth,height:viewBoxHeight}};const{escala,offsetX,offsetY}=calcularTrajetoria(elementos,viewBoxWidth,viewBoxHeight,30);let x=0,y=0,angulo=0;const pontoInicialX=offsetX,pontoInicialY=offsetY;let path=`M ${pontoInicialX} ${pontoInicialY}`,pontoFinal={x:pontoInicialX,y:pontoInicialY};elementos.forEach(el=>{if(el.tipo==='distancia'){const radianos=(angulo-90)*Math.PI/180;x+=parseFloat(el.valor)*Math.cos(radianos);y+=parseFloat(el.valor)*Math.sin(radianos);const novoX=x*escala+offsetX,novoY=y*escala+offsetY;path+=` L ${novoX} ${novoY}`;pontoFinal={x:novoX,y:novoY}}else if(el.tipo==='rotacao'){angulo+=parseFloat(el.valor)*(el.direcao==='direita'?1:-1);angulo=((angulo%360)+360)%360}});return{path:`<path d="${path}" stroke="#313C41" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`,pontoInicial:{x:pontoInicialX,y:pontoInicialY},pontoFinal,viewBox:{width:viewBoxWidth,height:viewBoxHeight}}};const calcularEstatisticasRota=elementos=>{let distanciaTotal=0,tempoEstimado=0;elementos.forEach(el=>{if(el.tipo==='distancia'){const dist=parseFloat(el.valor);distanciaTotal+=dist;tempoEstimado+=dist/50}else if(el.tipo==='rotacao')tempoEstimado+=parseFloat(el.valor)/90});return{distanciaTotal:Math.round(distanciaTotal),distanciaMetros:Math.round(distanciaTotal/10)/10,tempoMinutos:Math.round(tempoEstimado/60),velocidadeMedia:1.8,consumo:Math.round(distanciaTotal*.5)/10}};const renderizarRotasAnteriores=()=>{if(!rotasAnterioresContainer)return;if(rotas.length===0){rotasAnterioresContainer.innerHTML='<div style="padding:40px;text-align:center;color:#666;font-size:16px;grid-column:1/-1">Nenhuma rota</div>';atualizarEstatisticasGlobais();return}rotasAnterioresContainer.innerHTML=rotas.slice(-5).reverse().map((rota,index)=>{const stats=calcularEstatisticasRota(rota.elementos),svg=gerarSVGTrajetoria(rota.elementos),calcPos=(coord,size)=>(coord/size)*100,calcBottom=(y,height)=>((height-y)/height)*100;return`<div class="card-rota"><div class="rota-header"><p class="rota-name">Rota ${rotas.length-index}</p><p class="rota-distance">${stats.distanciaMetros}m</p></div><div class="rota-body"><p class="rota-time">${stats.tempoMinutos} min</p><p class="rota-vmedia">Vmédia: ${stats.velocidadeMedia}km/h</p><p class="rota-consumo">Consumo: ${stats.consumo} Wh</p><div class="mapa-container"><div class="mapa-placeholder"><svg class="map-route" width="100%" height="100%" viewBox="0 0 316 211" preserveAspectRatio="xMidYMid meet" fill="none">${svg.path}</svg><div class="truck-icon" style="left:${calcPos(svg.pontoInicial.x,svg.viewBox.width)}%;bottom:${calcBottom(svg.pontoInicial.y,svg.viewBox.height)}%;transform:translate(-50%,50%)">🚚</div><div class="award-icon" style="left:${calcPos(svg.pontoFinal.x,svg.viewBox.width)}%;bottom:${calcBottom(svg.pontoFinal.y,svg.viewBox.height)}%;transform:translate(-50%,50%)">🏆</div></div></div></div></div>`}).join('');atualizarEstatisticasGlobais()};const atualizarEstatisticasGlobais=()=>{const capacidadeBateria=10000;let consumoTotal=0,distanciaTotal=0;rotas.forEach(rota=>{const stats=calcularEstatisticasRota(rota.elementos);consumoTotal+=stats.consumo;distanciaTotal+=stats.distanciaMetros});const porcentagemGasta=Math.min((consumoTotal/capacidadeBateria)*100,100),soc=Math.max(100-porcentagemGasta,0),atualizar=(id,valor)=>{const el=$('#'+id);if(el)el.textContent=Math.round(valor)};atualizar('rotas-concluidas',rotas.length);atualizar('bateria-gasta',consumoTotal);atualizar('porcentagem-gasta',porcentagemGasta);atualizar('distancia-total',distanciaTotal);atualizar('bateria-soc',soc)};const adicionarElemento=(tipo,valor,direcao=null)=>{const elemento={tipo,valor,id:Date.now()};if(tipo==='rotacao')elemento.direcao=direcao;rotaAtual.elementos.push(elemento);renderizarElementos();desenharTrajetoria(rotaAtual.elementos)};if(btnAddDistancia)btnAddDistancia.addEventListener('click',()=>{const valor=inputDistancia.value.trim();if(valor&&parseFloat(valor)>0){adicionarElemento('distancia',valor);inputDistancia.value=''}else alert('Valor inválido')});if(btnAddRotacao)btnAddRotacao.addEventListener('click',()=>{const valor=inputRotacao.value.trim();if(valor&&parseFloat(valor)>0){adicionarElemento('rotacao',valor,selectDirecao.value);inputRotacao.value=''}else alert('Valor inválido')});const renderizarElementos=()=>{elementosContainer.innerHTML=rotaAtual.elementos.map((el,i)=>{const content=el.tipo==='distancia'?`<p class="elemento-label">Distância</p><p class="elemento-value">${el.valor} cm</p>`:`<p class="elemento-label">Girar ${el.valor}°</p><p class="elemento-value">${el.direcao==='direita'?'Direita ➡️':'Esquerda ⬅️'}</p>`;return`<div class="elemento-item ${el.tipo}" data-id="${el.id}"><div class="elemento-content">${content}</div><button class="btn-remover" data-id="${el.id}">-</button><span class="elemento-numero">${i+1}</span></div>`}).join('');$$('.btn-remover').forEach(btn=>btn.addEventListener('click',()=>{rotaAtual.elementos=rotaAtual.elementos.filter(el=>el.id!==parseInt(btn.dataset.id));renderizarElementos();desenharTrajetoria(rotaAtual.elementos)}))};
if(btnConcluir)btnConcluir.addEventListener('click',()=>{if(rotaAtual.elementos.length===0)return alert('Adicione elementos');const rota={id:Date.now(),elementos:[...rotaAtual.elementos],dataHora:new Date().toISOString()};rotas.push(rota);renderizarRotasAnteriores();enviarRota(rota);rotaAtual.elementos=[];renderizarElementos();desenharTrajetoria([]);websocket.send('LED_ON');alert('Rota enviada! LED ligado')});
if(btnLimparRota)btnLimparRota.addEventListener('click',()=>{if(confirm('Limpar rota?')){rotaAtual.elementos=[];renderizarElementos();desenharTrajetoria([])}});
if(btnLimparRotas)btnLimparRotas.addEventListener('click',()=>{if(confirm('Limpar rotas?')){rotas=[];renderizarRotasAnteriores();websocket.send('LED_OFF');alert('Rotas limpas! LED apagado')}});
const enviarRota=rota=>{const comandos=rota.elementos.map(el=>({tipo:el.tipo==='distancia'?'MOVE':'ROTATE',...(el.tipo==='distancia'?{valor:parseInt(el.valor),unidade:'cm'}:{angulo:parseInt(el.valor),direcao:el.direcao})}));console.log('Enviando:',comandos);websocket.send(JSON.stringify({tipo:'ROTA',comandos}))};
renderizarRotasAnteriores();desenharTrajetoria([]);if(inputDistancia)inputDistancia.addEventListener('keypress',e=>e.key==='Enter'&&btnAddDistancia.click());if(inputRotacao)inputRotacao.addEventListener('keypress',e=>e.key==='Enter'&&btnAddRotacao.click())});
</script>
</body>
</html>
)rawliteral";

void notifyClients() {
  ws.textAll(String(ledState));
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = String((char*)data);
    
    Serial.println("Mensagem recebida: " + message);
    
    // Comando para ligar LED (botão Concluir)
    if (message == "LED_ON") {
      ledState = 1;
      notifyClients();
      Serial.println("LED ligado pelo botão Concluir");
    }
    // Comando para desligar LED (botão Limpar Rotas)
    else if (message == "LED_OFF") {
      ledState = 0;
      notifyClients();
      Serial.println("LED apagado pelo botão Limpar Rotas");
    }
    // Comando de toggle original
    else if (message == "toggle") {
      ledState = !ledState;
      notifyClients();
      Serial.println("LED toggle");
    }
    // Comandos de rota (JSON)
    else if (message.startsWith("{")) {
      Serial.println("Comando de rota recebido:");
      Serial.println(message);
      // Aqui você pode processar os comandos de movimento do robô
      // Exemplo de parsing e execução dos comandos
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket cliente #%u conectado de %s\n", client->id(), client->remoteIP().toString().c_str());
      // Envia estado atual do LED para o novo cliente
      client->text(String(ledState));
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket cliente #%u desconectado\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
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

void setup() {
  Serial.begin(115200);
  
  // Configura LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  // Inicia WiFi como Access Point
  Serial.println("Iniciando WiFi AP...");
  WiFi.softAP(ssid, password);
  
  Serial.println("");
  Serial.println("WiFi AP iniciado");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Inicia WebSocket
  initWebSocket();
  
  // Rota principal - HTML do Cegoinha
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  // Inicia servidor
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  ws.cleanupClients();
  digitalWrite(ledPin, ledState);
}
