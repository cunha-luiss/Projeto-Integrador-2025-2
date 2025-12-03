// Cegoinha - Sistema de Controle do Carrinho
document.addEventListener("DOMContentLoaded", () => {
	let rotas = [];
	let rotaAtual = { elementos: [] };

	// Seletores
	const $ = (sel) => document.querySelector(sel);
	const $$ = (sel) => document.querySelectorAll(sel);

	const btnAddDistancia = $(".distancia-add .btn-add");
	const btnAddRotacao = $(".rotacao-add .btn-add");
	const inputDistancia = $("#input-distancia");
	const inputRotacao = $("#input-graus");
	const selectDirecao = $("#select-direcao");
	const btnConcluir = $("#btn-concluir-rota");
	const btnLimparRota = $("#btn-limpar-rota");
	const btnLimparRotas = $(".btn-rotas-limpar");
	const elementosContainer = $(".elementos-adicionados");
	const rotasAnterioresContainer = $(".rotas-grid");
	const svgTrajetoria = $(".map-route-large");
	const mapaInfo = {
		distancia: $$(".trajetoria-info")[0],
		tempo: $$(".trajetoria-info")[1],
	};

	// Controles da porta e carrinho
	const btnAbrirPorta = $("#btn-abrir-porta");
	const btnFecharPorta = $("#btn-fechar-porta");
	const statusPortaInfo = $("#status-porta-info");
	const btnPararCarrinho = $("#btn-parar-carrinho");
	const btnPararContainer = $("#btn-parar-container");
	const statusRotaInfo = $("#status-rota-info");

	// Estado da rota
	let rotaEmExecucao = false;

	// Função para mostrar o botão de parar
	const mostrarBotaoParar = () => {
		if (btnPararContainer) {
			btnPararContainer.classList.remove("hidden");
			btnPararContainer.classList.add("visible");
			rotaEmExecucao = true;
		}
	};

	// Função para ocultar o botão de parar
	const ocultarBotaoParar = () => {
		if (btnPararContainer) {
			btnPararContainer.classList.remove("visible");
			btnPararContainer.classList.add("hidden");
			rotaEmExecucao = false;
		}
	};

	// Função para atualizar status da rota
	const atualizarStatusRota = (mensagem) => {
		if (statusRotaInfo) {
			statusRotaInfo.textContent = mensagem;
		}
	};

	// Calcular trajetória com auto-escala
	const calcularTrajetoria = (
		elementos,
		viewBoxWidth,
		viewBoxHeight,
		margem
	) => {
		let pontos = [{ x: 0, y: 0 }];
		let x = 0,
			y = 0,
			angulo = 0;

		elementos.forEach((el) => {
			if (el.tipo === "distancia") {
				const distancia = parseFloat(el.valor);
				const radianos = ((angulo - 90) * Math.PI) / 180;
				x += distancia * Math.cos(radianos);
				y += distancia * Math.sin(radianos);
				pontos.push({ x, y });
			} else if (el.tipo === "rotacao") {
				angulo += parseFloat(el.valor) * (el.direcao === "direita" ? 1 : -1);
				angulo = ((angulo % 360) + 360) % 360;
			}
		});

		const minX = Math.min(...pontos.map((p) => p.x));
		const maxX = Math.max(...pontos.map((p) => p.x));
		const minY = Math.min(...pontos.map((p) => p.y));
		const maxY = Math.max(...pontos.map((p) => p.y));
		const largura = maxX - minX;
		const altura = maxY - minY;

		const escala =
			largura < 1 && altura < 1
				? 10
				: Math.min(
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
			svgTrajetoria.innerHTML = "";
			if (mapaInfo.distancia)
				mapaInfo.distancia.textContent = "Distância do percurso: 0m";
			if (mapaInfo.tempo)
				mapaInfo.tempo.textContent = "Tempo estimado do percurso: 0s";

			const caminhao = $(".trajetoria .truck-icon-large");
			const trofeu = $(".trajetoria .award-icon-large");
			if (caminhao) {
				caminhao.style.left = "15%";
				caminhao.style.bottom = "10%";
			}
			if (trofeu) {
				trofeu.style.left = "85%";
				trofeu.style.bottom = "10%";
			}
			return;
		}

		const viewBoxWidth = 400,
			viewBoxHeight = 350;
		const { pontos, escala, offsetX, offsetY } = calcularTrajetoria(
			elementos,
			viewBoxWidth,
			viewBoxHeight,
			30
		);

		let x = 0,
			y = 0,
			angulo = 0;
		const pontoInicialX = offsetX,
			pontoInicialY = offsetY;
		let path = `M ${pontoInicialX} ${pontoInicialY}`;
		let pontoFinal = { x: pontoInicialX, y: pontoInicialY };

		elementos.forEach((el) => {
			if (el.tipo === "distancia") {
				const radianos = ((angulo - 90) * Math.PI) / 180;
				x += parseFloat(el.valor) * Math.cos(radianos);
				y += parseFloat(el.valor) * Math.sin(radianos);
				const novoX = x * escala + offsetX;
				const novoY = y * escala + offsetY;
				path += ` L ${novoX} ${novoY}`;
				pontoFinal = { x: novoX, y: novoY };
			} else if (el.tipo === "rotacao") {
				angulo += parseFloat(el.valor) * (el.direcao === "direita" ? 1 : -1);
				angulo = ((angulo % 360) + 360) % 360;
			}
		});

		svgTrajetoria.innerHTML = `<path d="${path}" stroke="#313C41" stroke-width="3" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`;

		const atualizarIcone = (icone, ponto) => {
			if (icone) {
				icone.style.left = `${(ponto.x / viewBoxWidth) * 100}%`;
				icone.style.bottom = `${
					((viewBoxHeight - ponto.y) / viewBoxHeight) * 100
				}%`;
				icone.style.transform = "translate(-50%, 50%)";
			}
		};

		atualizarIcone($(".trajetoria .truck-icon-large"), {
			x: pontoInicialX,
			y: pontoInicialY,
		});
		atualizarIcone($(".trajetoria .award-icon-large"), pontoFinal);

		const stats = calcularEstatisticasRota(elementos);
		if (mapaInfo.distancia)
			mapaInfo.distancia.textContent = `Distância do percurso: ${stats.distanciaMetros}m`;
		if (mapaInfo.tempo)
			mapaInfo.tempo.textContent = `Tempo estimado do percurso: ${Math.round(
				stats.distanciaTotal / 50
			)}s`;
	};

	// Gerar SVG para rotas anteriores
	const gerarSVGTrajetoria = (elementos) => {
		const viewBoxWidth = 316,
			viewBoxHeight = 211;
		if (elementos.length === 0) {
			return {
				path: '<path d="" stroke="#313C41" stroke-width="2" fill="none"/>',
				pontoInicial: { x: 158, y: 180 },
				pontoFinal: { x: 158, y: 180 },
				viewBox: { width: viewBoxWidth, height: viewBoxHeight },
			};
		}

		const { escala, offsetX, offsetY } = calcularTrajetoria(
			elementos,
			viewBoxWidth,
			viewBoxHeight,
			30
		);
		let x = 0,
			y = 0,
			angulo = 0;
		const pontoInicialX = offsetX,
			pontoInicialY = offsetY;
		let path = `M ${pontoInicialX} ${pontoInicialY}`;
		let pontoFinal = { x: pontoInicialX, y: pontoInicialY };

		elementos.forEach((el) => {
			if (el.tipo === "distancia") {
				const radianos = ((angulo - 90) * Math.PI) / 180;
				x += parseFloat(el.valor) * Math.cos(radianos);
				y += parseFloat(el.valor) * Math.sin(radianos);
				const novoX = x * escala + offsetX;
				const novoY = y * escala + offsetY;
				path += ` L ${novoX} ${novoY}`;
				pontoFinal = { x: novoX, y: novoY };
			} else if (el.tipo === "rotacao") {
				angulo += parseFloat(el.valor) * (el.direcao === "direita" ? 1 : -1);
				angulo = ((angulo % 360) + 360) % 360;
			}
		});

		return {
			path: `<path d="${path}" stroke="#313C41" stroke-width="2" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`,
			pontoInicial: { x: pontoInicialX, y: pontoInicialY },
			pontoFinal,
			viewBox: { width: viewBoxWidth, height: viewBoxHeight },
		};
	};

	// Calcular estatísticas da rota
	const calcularEstatisticasRota = (elementos) => {
		let distanciaTotal = 0,
			tempoEstimado = 0;
		elementos.forEach((el) => {
			if (el.tipo === "distancia") {
				const dist = parseFloat(el.valor);
				distanciaTotal += dist;
				tempoEstimado += dist / 50;
			} else if (el.tipo === "rotacao") {
				tempoEstimado += parseFloat(el.valor) / 90;
			}
		});

		return {
			distanciaTotal: Math.round(distanciaTotal),
			distanciaMetros: Math.round(distanciaTotal / 10) / 10,
			tempoMinutos: Math.round(tempoEstimado / 60),
			velocidadeMedia: 1.8,
			consumo: Math.round(distanciaTotal * 0.5) / 10,
		};
	};

	// Renderizar rotas anteriores
	const renderizarRotasAnteriores = () => {
		if (!rotasAnterioresContainer) return;

		if (rotas.length === 0) {
			rotasAnterioresContainer.innerHTML =
				'<div style="padding: 40px; text-align: center; color: #666; font-size: 20px; grid-column: 1 / -1;">Nenhuma rota registrada ainda</div>';
			atualizarEstatisticasGlobais();
			return;
		}

		rotasAnterioresContainer.innerHTML = rotas
			.slice(-5)
			.reverse()
			.map((rota, index) => {
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
                        <p class="rota-vmedia">Vmédia: ${
													stats.velocidadeMedia
												}km/h</p>
                        <p class="rota-consumo">Consumo de ${
													stats.consumo
												} Wh de bateria</p>
                        <div class="mapa-container">
                            <div class="mapa-placeholder">
                                <svg class="map-route" width="100%" height="100%" viewBox="0 0 316 211" preserveAspectRatio="xMidYMid meet" fill="none">${
																	svg.path
																}</svg>
                                <div class="truck-icon" style="left:${calcPos(
																	svg.pontoInicial.x,
																	svg.viewBox.width
																)}%; bottom:${calcBottom(
					svg.pontoInicial.y,
					svg.viewBox.height
				)}%; transform:translate(-50%,50%)">🚚</div>
                                <div class="award-icon" style="left:${calcPos(
																	svg.pontoFinal.x,
																	svg.viewBox.width
																)}%; bottom:${calcBottom(
					svg.pontoFinal.y,
					svg.viewBox.height
				)}%; transform:translate(-50%,50%)">🏆</div>
                            </div>
                        </div>
                    </div>
                </div>
            `;
			})
			.join("");

		atualizarEstatisticasGlobais();
	};

	// Atualizar estatísticas globais
	const atualizarEstatisticasGlobais = () => {
		const capacidadeBateria = 10000;
		let consumoTotal = 0,
			distanciaTotal = 0;

		rotas.forEach((rota) => {
			const stats = calcularEstatisticasRota(rota.elementos);
			consumoTotal += stats.consumo;
			distanciaTotal += stats.distanciaMetros;
		});

		const porcentagemGasta = Math.min(
			(consumoTotal / capacidadeBateria) * 100,
			100
		);
		const soc = Math.max(100 - porcentagemGasta, 0);

		const atualizar = (id, valor) => {
			const el = $("#" + id);
			if (el) el.textContent = Math.round(valor);
		};

		atualizar("rotas-concluidas", rotas.length);
		atualizar("bateria-gasta", consumoTotal);
		atualizar("porcentagem-gasta", porcentagemGasta);
		atualizar("distancia-total", distanciaTotal);
		atualizar("bateria-soc", soc);
	};

	// Adicionar elemento
	const adicionarElemento = (tipo, valor, direcao = null) => {
		const elemento = { tipo, valor, id: Date.now() };
		if (tipo === "rotacao") elemento.direcao = direcao;
		rotaAtual.elementos.push(elemento);
		renderizarElementos();
		desenharTrajetoria(rotaAtual.elementos);
	};

	// Event listeners - Adicionar
	if (btnAddDistancia) {
		btnAddDistancia.addEventListener("click", () => {
			const valor = inputDistancia.value.trim();
			if (valor && parseFloat(valor) > 0) {
				adicionarElemento("distancia", valor);
				inputDistancia.value = "";
			} else {
				alert("Insira um valor válido para a distância");
			}
		});
	}

	if (btnAddRotacao) {
		btnAddRotacao.addEventListener("click", () => {
			const valor = inputRotacao.value.trim();
			if (valor && parseFloat(valor) > 0) {
				adicionarElemento("rotacao", valor, selectDirecao.value);
				inputRotacao.value = "";
			} else {
				alert("Insira um valor válido para a rotação");
			}
		});
	}

	// Renderizar elementos
	const renderizarElementos = () => {
		elementosContainer.innerHTML = rotaAtual.elementos
			.map((el, i) => {
				const content =
					el.tipo === "distancia"
						? `<p class="elemento-label">Distância</p><p class="elemento-value">${el.valor}</p>`
						: `<p class="elemento-label">Girar ${
								el.valor
						  }°</p><p class="elemento-value">Direção: ${
								el.direcao === "direita" ? "Direita ➡️" : "Esquerda ⬅️"
						  }</p>`;

				return `<div class="elemento-item ${el.tipo}" data-id="${el.id}">
                <div class="elemento-content">${content}</div>
                <button class="btn-remover" data-id="${el.id}">-</button>
                <span class="elemento-numero">${i + 1}</span>
            </div>`;
			})
			.join("");

		$$(".btn-remover").forEach((btn) => {
			btn.addEventListener("click", () => {
				rotaAtual.elementos = rotaAtual.elementos.filter(
					(el) => el.id !== parseInt(btn.dataset.id)
				);
				renderizarElementos();
				desenharTrajetoria(rotaAtual.elementos);
			});
		});
	};

	// Botões de ação
	if (btnConcluir) {
		btnConcluir.addEventListener("click", () => {
			if (rotaAtual.elementos.length === 0) {
				return alert("Adicione pelo menos um elemento à rota");
			}

			const rota = {
				id: Date.now(),
				elementos: [...rotaAtual.elementos],
				dataHora: new Date().toISOString(),
			};

			enviarRota(rota);
			rotaAtual.elementos = [];
			renderizarElementos();
			desenharTrajetoria([]);

			// Mostra o botão de parar quando a rota é iniciada
			mostrarBotaoParar();
			atualizarStatusRota("🚚 Rota em execução...");
		});
	}

	if (btnLimparRota) {
		btnLimparRota.addEventListener("click", () => {
			if (confirm("Deseja limpar todos os elementos da rota atual?")) {
				rotaAtual.elementos = [];
				renderizarElementos();
				desenharTrajetoria([]);
			}
		});
	}

	if (btnLimparRotas) {
		btnLimparRotas.addEventListener("click", () => {
			if (
				confirm(
					"Deseja limpar todas as rotas anteriores? Isso afetará todos os usuários conectados."
				)
			) {
				if (wsConnected) {
					websocket.send(
						JSON.stringify({
							channel: "LIMPAR_ROTAS",
							deviceId: deviceId,
						})
					);
				}
			}
		});
	}

	// ===== CONTROLES DA PORTA =====
	if (btnAbrirPorta) {
		btnAbrirPorta.addEventListener("click", () => {
			console.log("Enviando comando ABRIR");
			if (wsConnected) {
				websocket.send(
					JSON.stringify({
						channel: "ABRIR",
						deviceId: deviceId,
					})
				);
				console.log(
					JSON.stringify({
						channel: "ABRIR",
						deviceId: deviceId,
					})
				);
			} else {
				alert("WebSocket não conectado!");
			}
		});
	}

	if (btnFecharPorta) {
		btnFecharPorta.addEventListener("click", () => {
			console.log("Enviando comando FECHAR");
			if (wsConnected) {
				websocket.send(
					JSON.stringify({
						channel: "FECHAR",
						deviceId: deviceId,
					})
				);
				console.log(
					JSON.stringify({
						channel: "FECHAR",
						deviceId: deviceId,
					})
				);
			} else {
				alert("WebSocket não conectado!");
			}
		});
	}

	// ===== CONTROLE PARAR CARRINHO =====
	if (btnPararCarrinho) {
		btnPararCarrinho.addEventListener("click", () => {
			if (
				confirm("⚠️ Tem certeza que deseja PARAR o carrinho imediatamente?")
			) {
				console.log("🛑 Enviando comando PARAR_CARRINHO");

				if (wsConnected) {
					websocket.send(
						JSON.stringify({
							channel: "PARAR_CARRINHO",
							deviceId: deviceId,
						})
					);

					atualizarStatusRota("⏳ Parando carrinho...");
				} else {
					alert("❌ WebSocket não conectado! Não é possível enviar comando.");
				}
			}
		});
	}

	// Inicialização
	renderizarRotasAnteriores();
	desenharTrajetoria([]);

	// Enter para adicionar
	if (inputDistancia) {
		inputDistancia.addEventListener("keypress", (e) => {
			if (e.key === "Enter") btnAddDistancia.click();
		});
	}

	if (inputRotacao) {
		inputRotacao.addEventListener("keypress", (e) => {
			if (e.key === "Enter") btnAddRotacao.click();
		});
	}

	// ===== WEBSOCKET =====
	function getDeviceId() {
		let deviceId = localStorage.getItem("cegoinha_device_id");
		if (!deviceId) {
			deviceId =
				"device_" + Date.now() + "_" + Math.random().toString(36).substr(2, 9);
			localStorage.setItem("cegoinha_device_id", deviceId);
			console.log("🆔 Novo dispositivo criado:", deviceId);
		} else {
			console.log("🆔 Dispositivo reconhecido:", deviceId);
		}
		return deviceId;
	}

	var gateway = `ws://${window.location.hostname}/ws`;
	var websocket;
	var wsConnected = false;
	var reconnectInterval = null;
	var deviceId = getDeviceId();
	var sessionId = "session_" + Date.now();

	window.addEventListener("load", onLoad);

	function initWebSocket() {
		console.log("🔄 Tentando conectar ao WebSocket...");
		console.log("🆔 Device ID:", deviceId);
		console.log("🔑 Session ID:", sessionId);

		websocket = new WebSocket(gateway);
		websocket.onopen = onOpen;
		websocket.onclose = onClose;
		websocket.onerror = onError;
		websocket.onmessage = onMessage;
	}

	function onOpen(event) {
		console.log("✅ WebSocket conectado!");
		wsConnected = true;

		const identificacao = {
			channel: "DEVICE_ID",
			deviceId: deviceId,
			sessionId: sessionId,
			timestamp: Date.now(),
		};

		websocket.send(JSON.stringify(identificacao));
		console.log("📤 Identificação enviada para ESP32");

		if (reconnectInterval) {
			clearInterval(reconnectInterval);
			reconnectInterval = null;
		}

		document.title = "Cegoinha 🕊️ - Conectado";
	}

	function onClose(event) {
		console.log("❌ WebSocket desconectado");
		wsConnected = false;
		document.title = "Cegoinha 🕊️ - Desconectado";

		console.log("⏳ Reconectando em 2 segundos...");
		setTimeout(initWebSocket, 2000);
	}

	function onError(event) {
		console.error("❌ Erro no WebSocket:", event);
		wsConnected = false;
	}

	function onMessage(event) {
		console.log("📨 Mensagem recebida:", event.data);

		try {
			const data = JSON.parse(event.data);

			if (data.channel === "SYNC_ROTAS") {
				console.log("🔄 Sincronizando rotas do servidor...");
				rotas = data.rotas || [];
				renderizarRotasAnteriores();
				console.log(`✅ ${rotas.length} rotas sincronizadas`);
			} else if (data.channel === "NOVA_ROTA") {
				console.log("📥 Nova rota recebida de outro usuário");
				rotas.push(data.rota);
				renderizarRotasAnteriores();
				atualizarEstatisticasGlobais();

				// Mostra o botão quando uma nova rota é iniciada
				mostrarBotaoParar();
			} else if (data.channel === "ROTAS_LIMPAS") {
				console.log("🗑️ Rotas limpas por outro usuário");
				rotas = [];
				renderizarRotasAnteriores();
			} else if (data.channel === "UPDATE") {
			    switch (data.value) {
                case "DIREITA":
                    atualizarStatusRota("↪️ Virando à direita...");
                    break;
                case "ESQUERDA":
                    atualizarStatusRota("↩️ Virando à esquerda...");
                    break;
                case "FRENTE":
                    atualizarStatusRota("⬆️ Seguindo em frente...");
                    break;
                default:
                    break;
                }
			} else if (
				data.status === "ok" &&
				data.message === "Dispositivo identificado"
			) {
				console.log(`✅ Dispositivo identificado: ${data.deviceId}`);
			} else if (data.channel === "STATUS_PORTA") {
				console.log("🚪 Status da porta atualizado:", data.status);
				if (statusPortaInfo) {
					statusPortaInfo.textContent = `A porta está: ${data.status}`;
				}
			} else if (data.channel === "CARRINHO_PARADO") {
				console.log("✅ Carrinho parado com sucesso");

				// Oculta o botão quando o carrinho para
				ocultarBotaoParar();

				alert("🛑 Carrinho parado com sucesso!");
			} else if (data.channel === "ROTA_COMPLETA") {
				console.log("✅ Rota completa");

				// Oculta o botão quando a rota é concluída
				ocultarBotaoParar();

				alert("✅ Rota concluída com sucesso!");
			} else if (data.channel === "BATTERY_STATUS") {
				// Atualiza informações da bateria
				console.log("🔋 Bateria:", data.soc + "%", data.voltage + "V");

				const socElement = $("#bateria-soc");
				if (socElement) {
					socElement.textContent = Math.round(data.soc);

					// Muda cor baseado no nível
					const socCard = socElement.closest(".status-card");
					if (socCard) {
						const valueDiv = socCard.querySelector(".status-value");
						if (valueDiv) {
							if (data.soc <= 10) {
								valueDiv.style.backgroundColor = "#FF3D00";
								valueDiv.style.color = "#fff";
							} else if (data.soc <= 20) {
								valueDiv.style.backgroundColor = "#FF6B00";
								valueDiv.style.color = "#fff";
							} else {
								valueDiv.style.backgroundColor = "#C8C8C8";
								valueDiv.style.color = "#000";
							}
						}
					}
				}

				// Atualiza tensão se houver elemento
				const voltageElement = $("#bateria-voltage");
				if (voltageElement) {
					voltageElement.textContent = data.voltage.toFixed(2);
				}
			} else if (data.channel === "BATTERY_CRITICAL") {
				console.error("⚠️ BATERIA CRÍTICA!");
				alert(
					"⚠️ BATERIA CRÍTICA! Carrinho parado automaticamente. Recarregue a bateria."
				);

				// Oculta o botão de parar
				ocultarBotaoParar();
			} else if (data.channel === "VELOCIDADE") {
				const velocidadeElements = document.querySelectorAll(
					".status-percurso-card .status-value"
				);
				if (velocidadeElements.length > 0) {
					// Primeiro card é a velocidade
					velocidadeElements[0].textContent = `${data.value} cm/s`;
				}
			}
		} catch (e) {
			console.log("Mensagem texto:", event.data);
		}
	}

	const enviarRota = (rota) => {
		if (!wsConnected) {
			alert("⚠️ WebSocket não está conectado. Tentando reconectar...");
			initWebSocket();
			return;
		}

		const comandos = rota.elementos.map((el) => ({
			tipo: el.tipo === "distancia" ? "MOVE" : "ROTATE",
			...(el.tipo === "distancia"
				? { valor: parseInt(el.valor) }
				: { angulo: parseInt(el.valor), direcao: el.direcao }),
		}));

		console.log("📤 Enviando rota para ESP32:", comandos);
		websocket.send(
			JSON.stringify({
				channel: "ENVIAR_ROTAS",
				value: comandos,
				deviceId: deviceId,
				rotaId: rota.id,
			})
		);
	};

	function onLoad(event) {
		initWebSocket();
	}
});
