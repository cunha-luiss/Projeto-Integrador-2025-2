// Service Worker para Cegoinha PWA
// Versão: 1.0.0

const CACHE_NAME = 'cegoinha-cache-v1';
const ASSETS_TO_CACHE = [
    '/',
    '/index.html',
    '/style.css',
    '/app_new.js',
    '/manifest.json'
];

// Instalação do Service Worker
self.addEventListener('install', event => {
    console.log('[Service Worker] Instalando...');
    
    event.waitUntil(
        caches.open(CACHE_NAME)
            .then(cache => {
                console.log('[Service Worker] Cacheando assets');
                return cache.addAll(ASSETS_TO_CACHE);
            })
            .then(() => {
                console.log('[Service Worker] Instalado com sucesso');
                return self.skipWaiting();
            })
            .catch(error => {
                console.error('[Service Worker] Erro ao cachear:', error);
            })
    );
});

// Ativação do Service Worker
self.addEventListener('activate', event => {
    console.log('[Service Worker] Ativando...');
    
    event.waitUntil(
        caches.keys()
            .then(cacheNames => {
                return Promise.all(
                    cacheNames.map(cacheName => {
                        if (cacheName !== CACHE_NAME) {
                            console.log('[Service Worker] Removendo cache antigo:', cacheName);
                            return caches.delete(cacheName);
                        }
                    })
                );
            })
            .then(() => {
                console.log('[Service Worker] Ativado com sucesso');
                return self.clients.claim();
            })
    );
});

// Estratégia de cache: Network First, fallback para Cache
// Para WebSocket: sempre network (não cachear)
self.addEventListener('fetch', event => {
    const { request } = event;
    const url = new URL(request.url);
    
    // Não cachear WebSocket
    if (url.protocol === 'ws:' || url.protocol === 'wss:') {
        return;
    }
    
    // Estratégia: Network First com timeout
    event.respondWith(
        Promise.race([
            fetch(request)
                .then(response => {
                    // Clonar a resposta antes de cachear
                    const responseClone = response.clone();
                    
                    // Cachear apenas respostas válidas
                    if (response && response.status === 200 && response.type === 'basic') {
                        caches.open(CACHE_NAME)
                            .then(cache => {
                                cache.put(request, responseClone);
                            });
                    }
                    
                    return response;
                })
                .catch(() => {
                    // Se network falhar, buscar no cache
                    return caches.match(request)
                        .then(cachedResponse => {
                            if (cachedResponse) {
                                console.log('[Service Worker] Servindo do cache:', request.url);
                                return cachedResponse;
                            }
                            
                            // Se não estiver no cache, retornar página offline
                            if (request.mode === 'navigate') {
                                return caches.match('/index.html');
                            }
                        });
                }),
            // Timeout de 5 segundos
            new Promise((resolve, reject) => {
                setTimeout(() => reject(new Error('Timeout')), 5000);
            })
        ])
        .catch(() => {
            // Fallback para cache em caso de timeout ou erro
            return caches.match(request)
                .then(cachedResponse => {
                    if (cachedResponse) {
                        return cachedResponse;
                    }
                    
                    if (request.mode === 'navigate') {
                        return caches.match('/index.html');
                    }
                });
        })
    );
});

// Sincronização em background (futuro)
self.addEventListener('sync', event => {
    console.log('[Service Worker] Background sync:', event.tag);
    
    if (event.tag === 'sync-rotas') {
        event.waitUntil(
            // Implementar lógica de sincronização aqui
            Promise.resolve()
        );
    }
});

// Notificações push (futuro)
self.addEventListener('push', event => {
    console.log('[Service Worker] Push recebido:', event);
    
    const options = {
        body: event.data ? event.data.text() : 'Nova notificação do Cegoinha',
        icon: '/icon-192.png',
        badge: '/icon-192.png',
        vibrate: [200, 100, 200]
    };
    
    event.waitUntil(
        self.registration.showNotification('Cegoinha 🕊️', options)
    );
});
