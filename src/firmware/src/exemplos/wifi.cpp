// Teste da conexão Wi-Fi do ESP32, sem o WebSocket nem o resto do firmware.
//
// Gravar:  pio run -e exemplo_wifi -t upload && pio device monitor
//
// Usa o nome e a senha de src/config/credenciais.h (copiar de credenciais.exemplo.h).
// Conferir:
//   - a rede configurada aparece no scan. O ESP32 só enxerga redes de 2,4 GHz;
//   - a conexão termina em poucos segundos e mostra o IP recebido;
//   - o RSSI (sinal) fica acima de uns -70 dBm onde o robô vai correr;
//   - desligar o roteador (ou se afastar) e religar: a conexão volta sozinha.

#include <Arduino.h>
#include <WiFi.h>

#include "config/credenciais.h"
#include "config/pinos.h"

constexpr uint32_t TIMEOUT_CONEXAO_MS = 15000;
constexpr uint32_t PERIODO_IMPRESSAO_MS = 2000;

static const char *nome_status(wl_status_t status) {
    switch (status) {
        case WL_CONNECTED: return "conectado";
        case WL_NO_SSID_AVAIL: return "rede não encontrada";
        case WL_CONNECT_FAILED: return "falha na conexão (senha errada?)";
        case WL_CONNECTION_LOST: return "conexão perdida";
        case WL_DISCONNECTED: return "desconectado";
        case WL_IDLE_STATUS: return "aguardando";
        default: return "outro";
    }
}

static void escanear_redes() {
    Serial.println("Procurando redes...");
    int quantidade = WiFi.scanNetworks();
    bool encontrou = false;
    for (int i = 0; i < quantidade; i++) {
        bool configurada = WiFi.SSID(i) == Credenciais::WIFI_SSID;
        encontrou = encontrou || configurada;
        Serial.printf("  %-32s %4d dBm  canal %2d%s\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                      WiFi.channel(i), configurada ? "   <- configurada" : "");
    }
    if (!encontrou) {
        Serial.printf("A rede \"%s\" não apareceu no scan.\n", Credenciais::WIFI_SSID);
    }
    WiFi.scanDelete();
}

void setup() {
    // Mantém a ponte H desligada, caso o teste seja feito na placa do robô.
    pinMode(Pinos::MOTOR_ESQ_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_ESQ_PWM, LOW);
    pinMode(Pinos::MOTOR_DIR_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_DIR_PWM, LOW);

    Serial.begin(115200);
    delay(2000);

    WiFi.mode(WIFI_STA);
    escanear_redes();
    // Antes do scan o driver ainda não iniciou e o MAC sai zerado.
    Serial.printf("MAC do ESP32: %s\n", WiFi.macAddress().c_str());

    WiFi.setAutoReconnect(true);
    Serial.printf("Conectando em \"%s\"...\n", Credenciais::WIFI_SSID);
    WiFi.begin(Credenciais::WIFI_SSID, Credenciais::WIFI_SENHA);

    uint32_t inicio = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - inicio < TIMEOUT_CONEXAO_MS) {
        delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("Conectado em %lu ms. IP: %s  gateway: %s  RSSI: %d dBm\n",
                      (unsigned long)(millis() - inicio), WiFi.localIP().toString().c_str(),
                      WiFi.gatewayIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.printf("Não conectou em %lu s: %s\n", (unsigned long)(TIMEOUT_CONEXAO_MS / 1000),
                      nome_status(WiFi.status()));
    }
}

void loop() {
    static wl_status_t status_anterior = WL_IDLE_STATUS;
    static uint32_t ultima_impressao = 0;

    wl_status_t status = WiFi.status();
    if (status != status_anterior) {
        Serial.printf("Status: %s\n", nome_status(status));
        status_anterior = status;
    }

    if (status == WL_CONNECTED && millis() - ultima_impressao >= PERIODO_IMPRESSAO_MS) {
        ultima_impressao = millis();
        Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    }

    delay(100);
}
