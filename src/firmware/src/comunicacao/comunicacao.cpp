#include "comunicacao.h"

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <WiFi.h>

#if __has_include("config/credenciais.h")
#include "config/credenciais.h"
#else
#error "Falta src/config/credenciais.h: copie credenciais.exemplo.h e preencha (README do firmware)"
#endif

// Espera entre as tentativas de reconectar o WebSocket.
constexpr uint32_t RECONEXAO_MS = 2000;

// Se o servidor parar de responder ao ping, a conexão é dada como caída.
constexpr uint32_t PING_INTERVALO_MS = 2000;
constexpr uint32_t PING_TIMEOUT_MS = 1000;
constexpr uint8_t PING_FALHAS = 2;

static WebSocketsClient websocket;
static bool websocket_iniciado = false;

static void ao_receber_evento(WStype_t tipo, uint8_t *, size_t) {
    switch (tipo) {
        case WStype_CONNECTED:
            Serial.println("WebSocket conectado");
            break;
        case WStype_DISCONNECTED:
            Serial.println("WebSocket desconectado");
            break;
        default:
            // Mensagens do servidor são ignoradas (canal só de ida, RNF13).
            break;
    }
}

void comunicacao_iniciar() {
    WiFi.mode(WIFI_STA);
    // O ESP32 volta a conectar sozinho no Wi-Fi depois de uma queda.
    WiFi.setAutoReconnect(true);
    WiFi.begin(Credenciais::WIFI_SSID, Credenciais::WIFI_SENHA);

    websocket.onEvent(ao_receber_evento);
    websocket.setReconnectInterval(RECONEXAO_MS);
    websocket.enableHeartbeat(PING_INTERVALO_MS, PING_TIMEOUT_MS, PING_FALHAS);
}

void comunicacao_atualizar() {
    // Sem Wi-Fi não adianta tentar o WebSocket: cada tentativa travaria a tarefa
    // até o tempo limite da conexão TCP.
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // O begin só pode ser chamado uma vez; depois a biblioteca reconecta sozinha.
    if (!websocket_iniciado) {
        websocket.begin(Credenciais::BACKEND_IP, Credenciais::BACKEND_PORTA,
                        Credenciais::BACKEND_CAMINHO);
        websocket_iniciado = true;
    }
    websocket.loop();
}

bool comunicacao_conectada() {
    return WiFi.status() == WL_CONNECTED && websocket.isConnected();
}

bool comunicacao_enviar(const char *texto, size_t tamanho) {
    if (!comunicacao_conectada()) {
        return false;
    }
    return websocket.sendTXT(texto, tamanho);
}
