// Cegoinha - Tela Principal JavaScript
// Este arquivo contém a lógica para interação com a interface

document.addEventListener('DOMContentLoaded', function() {
    // Elementos da rota
    let rotas = [];
    let rotaAtual = {
        elementos: []
    };

    // Botões de adicionar
    const btnAddDistancia = document.querySelector('.distancia-add .btn-add');
    const btnAddRotacao = document.querySelector('.rotacao-add .btn-add');
    const inputDistancia = document.getElementById('input-distancia');
    const inputRotacao = document.getElementById('input-graus');
    const selectDirecao = document.getElementById('select-direcao');

    // Botões de ação
    const btnConcluir = document.querySelector('.btn-concluir');
    const btnLimparRota = document.querySelector('.enviar-rota .btn-limpar');
    const btnLimparRotas = document.querySelector('.btn-rotas-limpar');

    // Container de elementos
    const elementosContainer = document.querySelector('.elementos-adicionados');
    const rotasAnterioresContainer = document.querySelector('.rotas-grid');
    const svgTrajetoria = document.querySelector('.map-route-large');
    const mapaInfo = {
        distancia: document.querySelectorAll('.trajetoria-info')[0],
        tempo: document.querySelectorAll('.trajetoria-info')[1]
    };

    // Função para desenhar o caminho no mapa
    function desenharTrajetoria(elementos) {
        if (!svgTrajetoria) return;
        
        // Se não houver elementos, limpar o mapa
        if (elementos.length === 0) {
            svgTrajetoria.innerHTML = '';
            if (mapaInfo.distancia) mapaInfo.distancia.textContent = 'Distância do percurso: 0m';
            if (mapaInfo.tempo) mapaInfo.tempo.textContent = 'Tempo estimado do percurso: 0s';
            
            // Resetar posições dos ícones para posição padrão
            const caminhao = document.querySelector('.trajetoria .truck-icon-large');
            const trofeu = document.querySelector('.trajetoria .award-icon-large');
            
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
        
        const viewBoxWidth = 400;
        const viewBoxHeight = 350;
        const margem = 30; // Margem de segurança
        
        // Primeiro, calcular todos os pontos para encontrar os limites
        let pontos = [];
        let x = 0;
        let y = 0;
        let angulo = 0;
        
        pontos.push({x, y});
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const distancia = parseFloat(el.valor);
                const radianos = (angulo - 90) * Math.PI / 180;
                x += distancia * Math.cos(radianos);
                y += distancia * Math.sin(radianos);
                pontos.push({x, y});
            } else if (el.tipo === 'rotacao') {
                const graus = parseFloat(el.valor);
                if (el.direcao === 'direita') {
                    angulo += graus;
                } else if (el.direcao === 'esquerda') {
                    angulo -= graus;
                }
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        // Encontrar limites da trajetória
        const minX = Math.min(...pontos.map(p => p.x));
        const maxX = Math.max(...pontos.map(p => p.x));
        const minY = Math.min(...pontos.map(p => p.y));
        const maxY = Math.max(...pontos.map(p => p.y));
        
        const larguraTrajetoria = maxX - minX;
        const alturaTrajetoria = maxY - minY;
        
        // Calcular escala para caber no viewBox com margem
        let escala;
        if (larguraTrajetoria < 1 && alturaTrajetoria < 1) {
            escala = 10; // Escala padrão para trajetórias muito pequenas
        } else {
            const escalaX = larguraTrajetoria > 0 ? (viewBoxWidth - 2 * margem) / larguraTrajetoria : 10;
            const escalaY = alturaTrajetoria > 0 ? (viewBoxHeight - 2 * margem) / alturaTrajetoria : 10;
            escala = Math.min(escalaX, escalaY); // Adaptar ao espaço disponível
        }
        
        // Calcular offset para centralizar
        const offsetX = viewBoxWidth / 2 - ((minX + maxX) / 2) * escala;
        const offsetY = viewBoxHeight / 2 - ((minY + maxY) / 2) * escala;
        
        // Agora desenhar o caminho com a escala e offset corretos
        x = 0;
        y = 0;
        angulo = 0;
        
        const pontoInicialX = x * escala + offsetX;
        const pontoInicialY = y * escala + offsetY;
        
        let path = `M ${pontoInicialX} ${pontoInicialY}`;
        let pontoInicial = {x: pontoInicialX, y: pontoInicialY};
        let pontoFinal = {x: pontoInicialX, y: pontoInicialY};
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const distancia = parseFloat(el.valor);
                const radianos = (angulo - 90) * Math.PI / 180;
                x += distancia * Math.cos(radianos);
                y += distancia * Math.sin(radianos);
                
                const novoX = x * escala + offsetX;
                const novoY = y * escala + offsetY;
                
                path += ` L ${novoX} ${novoY}`;
                pontoFinal = {x: novoX, y: novoY};
                
            } else if (el.tipo === 'rotacao') {
                const graus = parseFloat(el.valor);
                if (el.direcao === 'direita') {
                    angulo += graus;
                } else if (el.direcao === 'esquerda') {
                    angulo -= graus;
                }
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        // Atualizar o SVG com o caminho
        svgTrajetoria.innerHTML = `
            <path d="${path}" stroke="#313C41" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>
        `;
        
        // Atualizar ícones do caminhão e troféu
        const caminhao = document.querySelector('.trajetoria .truck-icon-large');
        const trofeu = document.querySelector('.trajetoria .award-icon-large');
        
        if (caminhao) {
            caminhao.style.left = `${(pontoInicial.x / viewBoxWidth) * 100}%`;
            caminhao.style.bottom = `${((viewBoxHeight - pontoInicial.y) / viewBoxHeight) * 100}%`;
            caminhao.style.transform = 'translate(-50%, 50%)';
        }
        
        if (trofeu) {
            trofeu.style.left = `${(pontoFinal.x / viewBoxWidth) * 100}%`;
            trofeu.style.bottom = `${((viewBoxHeight - pontoFinal.y) / viewBoxHeight) * 100}%`;
            trofeu.style.transform = 'translate(-50%, 50%)';
        }
        
        // Calcular e atualizar informações
        const stats = calcularEstatisticasRota(elementos);
        if (mapaInfo.distancia) {
            mapaInfo.distancia.textContent = `Distância do percurso: ${stats.distanciaMetros}m`;
        }
        if (mapaInfo.tempo) {
            const tempoSeg = Math.round(stats.distanciaTotal / 50); // segundos
            mapaInfo.tempo.textContent = `Tempo estimado do percurso: ${tempoSeg}s`;
        }
    }

    // Função para gerar SVG de uma trajetória (para usar nas rotas anteriores)
    function gerarSVGTrajetoria(elementos) {
        if (elementos.length === 0) {
            return {
                path: '<path d="" stroke="#313C41" stroke-width="2" fill="none"/>',
                pontoInicial: {x: 158, y: 180},
                pontoFinal: {x: 158, y: 180},
                viewBox: {width: 316, height: 211}
            };
        }
        
        const viewBoxWidth = 316;
        const viewBoxHeight = 211;
        const margem = 30; // Margem de segurança aumentada
        
        // Primeiro, calcular todos os pontos para encontrar os limites
        let pontos = [];
        let x = 0;
        let y = 0;
        let angulo = 0;
        
        pontos.push({x, y});
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const distancia = parseFloat(el.valor);
                const radianos = (angulo - 90) * Math.PI / 180;
                x += distancia * Math.cos(radianos);
                y += distancia * Math.sin(radianos);
                pontos.push({x, y});
            } else if (el.tipo === 'rotacao') {
                const graus = parseFloat(el.valor);
                if (el.direcao === 'direita') {
                    angulo += graus;
                } else if (el.direcao === 'esquerda') {
                    angulo -= graus;
                }
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        // Encontrar limites da trajetória
        const minX = Math.min(...pontos.map(p => p.x));
        const maxX = Math.max(...pontos.map(p => p.x));
        const minY = Math.min(...pontos.map(p => p.y));
        const maxY = Math.max(...pontos.map(p => p.y));
        
        const larguraTrajetoria = maxX - minX;
        const alturaTrajetoria = maxY - minY;
        
        // Calcular escala para caber no viewBox com margem
        // Se a trajetória for muito pequena ou for um único ponto, usar tamanho padrão
        let escala;
        if (larguraTrajetoria < 1 && alturaTrajetoria < 1) {
            escala = 10; // Escala padrão para trajetórias muito pequenas
        } else {
            const escalaX = larguraTrajetoria > 0 ? (viewBoxWidth - 2 * margem) / larguraTrajetoria : 10;
            const escalaY = alturaTrajetoria > 0 ? (viewBoxHeight - 2 * margem) / alturaTrajetoria : 10;
            escala = Math.min(escalaX, escalaY); // Remover limite de 1 para permitir aumento
        }
        
        // Calcular offset para centralizar
        const offsetX = viewBoxWidth / 2 - ((minX + maxX) / 2) * escala;
        const offsetY = viewBoxHeight / 2 - ((minY + maxY) / 2) * escala;
        
        // Agora desenhar o caminho com a escala e offset corretos
        x = 0;
        y = 0;
        angulo = 0;
        
        const pontoInicialX = x * escala + offsetX;
        const pontoInicialY = y * escala + offsetY;
        
        let path = `M ${pontoInicialX} ${pontoInicialY}`;
        let pontoInicial = {x: pontoInicialX, y: pontoInicialY};
        let pontoFinal = {x: pontoInicialX, y: pontoInicialY};
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const distancia = parseFloat(el.valor);
                const radianos = (angulo - 90) * Math.PI / 180;
                x += distancia * Math.cos(radianos);
                y += distancia * Math.sin(radianos);
                
                const novoX = x * escala + offsetX;
                const novoY = y * escala + offsetY;
                
                path += ` L ${novoX} ${novoY}`;
                pontoFinal = {x: novoX, y: novoY};
                
            } else if (el.tipo === 'rotacao') {
                const graus = parseFloat(el.valor);
                if (el.direcao === 'direita') {
                    angulo += graus;
                } else if (el.direcao === 'esquerda') {
                    angulo -= graus;
                }
                angulo = ((angulo % 360) + 360) % 360;
            }
        });
        
        return {
            path: `<path d="${path}" stroke="#313C41" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`,
            pontoInicial,
            pontoFinal,
            viewBox: {width: viewBoxWidth, height: viewBoxHeight}
        };
    }
    function calcularEstatisticasRota(elementos) {
        let distanciaTotal = 0;
        let tempoEstimado = 0;
        const velocidadeMedia = 1.8; // km/h
        
        elementos.forEach(el => {
            if (el.tipo === 'distancia') {
                const distanciaCm = parseFloat(el.valor);
                distanciaTotal += distanciaCm;
                // Tempo em segundos: distância (cm) / velocidade (cm/s)
                // 1.8 km/h = 50 cm/s
                tempoEstimado += distanciaCm / 50;
            } else if (el.tipo === 'rotacao') {
                // Tempo para girar: aproximadamente 1 segundo por 90 graus
                tempoEstimado += parseFloat(el.valor) / 90;
            }
        });
        
        return {
            distanciaTotal: Math.round(distanciaTotal), // em cm
            distanciaMetros: Math.round(distanciaTotal / 100 * 10) / 10, // em metros
            tempoMinutos: Math.round(tempoEstimado / 60),
            velocidadeMedia: velocidadeMedia,
            consumo: Math.round(distanciaTotal / 100 * 50 * 10) / 10 // estimativa: 50 Wh por metro
        };
    }

    // Função para renderizar rotas anteriores
    function renderizarRotasAnteriores() {
        if (!rotasAnterioresContainer) return;
        
        rotasAnterioresContainer.innerHTML = '';
        
        // Mostrar apenas as 5 rotas mais recentes
        const rotasRecentes = rotas.slice(-5).reverse();
        
        rotasRecentes.forEach((rota, index) => {
            const stats = calcularEstatisticasRota(rota.elementos);
            const numeroRota = rotas.length - index;
            const svg = gerarSVGTrajetoria(rota.elementos);
            
            // Calcular posições dos ícones (percentuais)
            // Converter coordenadas SVG para posição CSS (inverter Y para bottom)
            const caminhaoLeft = (svg.pontoInicial.x / svg.viewBox.width) * 100;
            const caminhaoBottom = ((svg.viewBox.height - svg.pontoInicial.y) / svg.viewBox.height) * 100;
            const trofeuLeft = (svg.pontoFinal.x / svg.viewBox.width) * 100;
            const trofeuBottom = ((svg.viewBox.height - svg.pontoFinal.y) / svg.viewBox.height) * 100;
            
            const cardHtml = `
                <div class="card-rota" data-rota-id="${rota.id}">
                    <div class="rota-header">
                        <p class="rota-name">Rota ${numeroRota}</p>
                        <p class="rota-distance">${stats.distanciaMetros}m</p>
                    </div>
                    <div class="rota-body">
                        <p class="rota-time">${stats.tempoMinutos} min</p>
                        <p class="rota-vmedia">Vmédia: ${stats.velocidadeMedia}km/h</p>
                        <p class="rota-consumo">Consumo de ${stats.consumo} Wh de bateria</p>
                        
                        <div class="mapa-container">
                            <div class="mapa-placeholder">
                                <svg class="map-route" width="100%" height="100%" viewBox="0 0 316 211" preserveAspectRatio="xMidYMid meet" fill="none">
                                    ${svg.path}
                                </svg>
                                <div class="truck-icon" style="left: ${caminhaoLeft}%; bottom: ${caminhaoBottom}%; transform: translate(-50%, 50%);">🚚</div>
                                <div class="award-icon" style="left: ${trofeuLeft}%; bottom: ${trofeuBottom}%; transform: translate(-50%, 50%);">🏆</div>
                            </div>
                        </div>
                    </div>
                </div>
            `;
            
            rotasAnterioresContainer.innerHTML += cardHtml;
        });
        
        // Se não houver rotas, mostrar mensagem
        if (rotas.length === 0) {
            rotasAnterioresContainer.innerHTML = `
                <div style="padding: 40px; text-align: center; color: #666; font-size: 20px; grid-column: 1 / -1;">
                    Nenhuma rota registrada ainda
                </div>
            `;
        }
        
        // Atualizar estatísticas globais
        atualizarEstatisticasGlobais();
    }

    // Função para atualizar estatísticas globais (consumo total e SOC)
    function atualizarEstatisticasGlobais() {
        const capacidadeBateria = 10000; // Capacidade total da bateria em Wh (ajuste conforme necessário)
        let consumoTotal = 0;
        let distanciaTotal = 0;
        
        rotas.forEach(rota => {
            const stats = calcularEstatisticasRota(rota.elementos);
            consumoTotal += stats.consumo;
            distanciaTotal += stats.distanciaMetros;
        });
        
        // Calcular SOC (State of Charge) - percentual restante
        const porcentagemGasta = Math.min((consumoTotal / capacidadeBateria) * 100, 100);
        const soc = Math.max(100 - porcentagemGasta, 0);
        
        // Atualizar elementos do DOM
        const rotasConcluidas = document.getElementById('rotas-concluidas');
        const bateriaGasta = document.getElementById('bateria-gasta');
        const porcentagemGastaEl = document.getElementById('porcentagem-gasta');
        const distanciaTotalEl = document.getElementById('distancia-total');
        const bateriaSOC = document.getElementById('bateria-soc');
        
        if (rotasConcluidas) rotasConcluidas.textContent = rotas.length;
        if (bateriaGasta) bateriaGasta.textContent = Math.round(consumoTotal);
        if (porcentagemGastaEl) porcentagemGastaEl.textContent = Math.round(porcentagemGasta);
        if (distanciaTotalEl) distanciaTotalEl.textContent = Math.round(distanciaTotal);
        if (bateriaSOC) bateriaSOC.textContent = Math.round(soc);
    }

    // Função para adicionar distância
    if (btnAddDistancia) {
        btnAddDistancia.addEventListener('click', function() {
            const valor = inputDistancia.value.trim();
            if (valor && parseFloat(valor) > 0) {
                adicionarElemento('distancia', valor);
                inputDistancia.value = '';
            } else {
                alert('Por favor, insira um valor válido para a distância');
            }
        });
    }

    // Função para adicionar rotação
    if (btnAddRotacao) {
        btnAddRotacao.addEventListener('click', function() {
            const valor = inputRotacao.value.trim();
            const direcao = selectDirecao.value;
            if (valor && parseFloat(valor) > 0) {
                adicionarElemento('rotacao', valor, direcao);
                inputRotacao.value = '';
            } else {
                alert('Por favor, insira um valor válido para a rotação');
            }
        });
    }

    // Função para adicionar elemento à lista
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
        console.log('Elemento adicionado:', elemento);
    }

    // Função para renderizar elementos
    function renderizarElementos() {
        elementosContainer.innerHTML = '';
        
        rotaAtual.elementos.forEach((elemento, index) => {
            const div = document.createElement('div');
            div.className = `elemento-item ${elemento.tipo}`;
            div.dataset.id = elemento.id;

            if (elemento.tipo === 'distancia') {
                div.innerHTML = `
                    <div class="elemento-content">
                        <p class="elemento-label">Distância</p>
                        <p class="elemento-value">${elemento.valor} cm</p>
                    </div>
                    <button class="btn-remover" data-id="${elemento.id}">-</button>
                    <span class="elemento-numero">${index + 1}</span>
                `;
            } else if (elemento.tipo === 'rotacao') {
                const direcaoTexto = elemento.direcao === 'direita' ? 'Direita ➡️' : 'Esquerda ⬅️';
                div.innerHTML = `
                    <div class="elemento-content">
                        <p class="elemento-label">Girar ${elemento.valor}°</p>
                        <p class="elemento-value">Direção: ${direcaoTexto}</p>
                    </div>
                    <button class="btn-remover" data-id="${elemento.id}">-</button>
                    <span class="elemento-numero">${index + 1}</span>
                `;
            }

            elementosContainer.appendChild(div);
        });

        // Adicionar event listeners aos botões de remover
        document.querySelectorAll('.btn-remover').forEach(btn => {
            btn.addEventListener('click', function() {
                const id = parseInt(this.dataset.id);
                removerElemento(id);
            });
        });
    }

    // Função para remover elemento
    function removerElemento(id) {
        rotaAtual.elementos = rotaAtual.elementos.filter(el => el.id !== id);
        renderizarElementos();
        desenharTrajetoria(rotaAtual.elementos);
        console.log('Elemento removido:', id);
    }

    // Botão Concluir - Enviar rota
    if (btnConcluir) {
        btnConcluir.addEventListener('click', function() {
            if (rotaAtual.elementos.length === 0) {
                alert('Adicione pelo menos um elemento à rota');
                return;
            }

            // Salvar rota
            const rota = {
                id: Date.now(),
                elementos: [...rotaAtual.elementos],
                dataHora: new Date().toISOString()
            };

            rotas.push(rota);
            console.log('Rota concluída:', rota);
            
            // Renderizar rotas anteriores
            renderizarRotasAnteriores();
            
            // Aqui você pode enviar a rota via WebSocket ou API
            enviarRota(rota);

            // Limpar elementos
            rotaAtual.elementos = [];
            renderizarElementos();
            desenharTrajetoria(rotaAtual.elementos);
            
            alert('Rota enviada com sucesso!');
        });
    }

    // Botão Limpar - Limpar rota atual
    if (btnLimparRota) {
        btnLimparRota.addEventListener('click', function() {
            if (confirm('Deseja limpar todos os elementos da rota atual?')) {
                rotaAtual.elementos = [];
                renderizarElementos();
                desenharTrajetoria(rotaAtual.elementos);
                console.log('Rota limpa');
            }
        });
    }

    // Botão Limpar - Limpar rotas anteriores
    if (btnLimparRotas) {
        btnLimparRotas.addEventListener('click', function() {
            if (confirm('Deseja limpar todas as rotas anteriores?')) {
                rotas = [];
                renderizarRotasAnteriores();
                console.log('Rotas anteriores limpas');
                alert('Rotas anteriores limpas com sucesso!');
            }
        });
    }

    // Inicializar estatísticas globais
    atualizarEstatisticasGlobais();

    // Função para enviar rota (placeholder)
    function enviarRota(rota) {
        // Aqui você implementaria a lógica para enviar a rota
        // Exemplo: via WebSocket para o Arduino/ESP32
        console.log('Enviando rota:', rota);
        
        // Exemplo de estrutura de dados para enviar:
        const comandos = rota.elementos.map(el => {
            if (el.tipo === 'distancia') {
                return {
                    tipo: 'MOVE',
                    valor: parseInt(el.valor),
                    unidade: 'cm'
                };
            } else if (el.tipo === 'rotacao') {
                return {
                    tipo: 'ROTATE',
                    angulo: parseInt(el.valor),
                    direcao: el.direcao // 'direita' ou 'esquerda'
                };
            }
        });

        console.log('Comandos a serem enviados:', comandos);
        
        // Se você estiver usando WebSocket:
        // ws.send(JSON.stringify({ comandos: comandos }));
    }

    // Simulação de atualização de status (valores dinâmicos)
    function atualizarStatus() {
        // Aqui você pode implementar a lógica para atualizar os status
        // baseado em dados recebidos do carrinho
        
        // Exemplo de atualização periódica:
        setInterval(() => {
            // Atualizar consumo de motores
            // Atualizar bateria
            // Atualizar velocidade
            // etc.
            
            // Você pode receber esses dados via WebSocket do Arduino/ESP32
        }, 1000);
    }

    // Inicializar
    console.log('Sistema Cegoinha inicializado');
    
    // Renderizar rotas anteriores ao carregar
    renderizarRotasAnteriores();
    
    // Inicializar mapa vazio
    desenharTrajetoria([]);
    
    // Comentado por enquanto, descomentar quando implementar WebSocket
    // atualizarStatus();

    // Event listeners para inputs (permitir Enter para adicionar)
    if (inputDistancia) {
        inputDistancia.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                btnAddDistancia.click();
            }
        });
    }

    if (inputRotacao) {
        inputRotacao.addEventListener('keypress', function(e) {
            if (e.key === 'Enter') {
                btnAddRotacao.click();
            }
        });
    }

    // Exemplo de integração com WebSocket (comentado)
    /*
    const ws = new WebSocket('ws://localhost:81'); // Ajuste para o IP do ESP32
    
    ws.onopen = function() {
        console.log('WebSocket conectado');
    };
    
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        console.log('Dados recebidos:', data);
        
        // Atualizar interface com dados recebidos
        if (data.motor1) {
            // Atualizar consumo motor 1
        }
        if (data.motor2) {
            // Atualizar consumo motor 2
        }
        if (data.bateria) {
            // Atualizar bateria
        }
        if (data.velocidade) {
            // Atualizar velocidade
        }
    };
    
    ws.onerror = function(error) {
        console.error('Erro WebSocket:', error);
    };
    
    ws.onclose = function() {
        console.log('WebSocket desconectado');
    };
    */
});
