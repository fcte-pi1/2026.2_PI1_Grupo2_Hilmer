// Procura mau contato no sensor de distância da direita (U10, sem XSHUT).
//
// Gravar:  pio run -e exemplo_fio_direita -t upload && pio device monitor
//
// Inicia os três sensores e, a cada 50 ms, confere se o da direita continua no
// endereço 0x30 e entregando medidas. Mexa nos fios do sensor um de cada vez
// (VIN, GND, SDA, SCL): quando ele cair, a falha aparece na hora com o tipo:
//   - "voltou ao 0x29": o sensor reiniciou, ou seja, perdeu alimentação (VIN ou GND);
//   - "sumiu do I2C": não responde em endereço nenhum (SDA, SCL, VIN ou GND solto);
//   - "responde mas não mede": o I2C chega, mas as medidas pararam.
// Depois de cada queda o teste inicia os sensores de novo e continua.
// O LED fica aceso com o sensor ok e apaga quando ele cai.

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "sensores/distancia.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_MS = 50;
constexpr uint32_t PERIODO_RESUMO_MS = 1000;
constexpr uint32_t IDADE_MAXIMA_MS = 200;
constexpr uint8_t ENDERECO_FABRICA = 0x29;

static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

enum EstadoDireita { DIREITA_OK, VOLTOU_FABRICA, SUMIU, SEM_MEDIDA };
static const char *NOMES[] = {
    "ok", "voltou ao 0x29 (reiniciou: perdeu alimentação, VIN ou GND)",
    "sumiu do I2C (fio solto: SDA, SCL, VIN ou GND)", "responde mas não mede"};

static EstadoDireita anterior = DIREITA_OK;
static uint32_t quedas = 0;
static uint32_t inicio_ok = 0;
static uint32_t ultimo_resumo = 0;

static bool responde(uint8_t endereco) {
    Wire.beginTransmission(endereco);
    return Wire.endTransmission() == 0;
}

static EstadoDireita estado_direita() {
    bool no_endereco = responde(DISTANCIA_ENDERECO_DIREITA);
    if (!no_endereco) {
        // A frente e a esquerda ficam em 0x31 e 0x32; no 0x29 só pode estar a direita.
        return responde(ENDERECO_FABRICA) ? VOLTOU_FABRICA : SUMIU;
    }
    return (distancia_falhas(IDADE_MAXIMA_MS) & DISTANCIA_FALHA_DIREITA) ? SEM_MEDIDA : DIREITA_OK;
}

static void iniciar_sensores() {
    bool ok = distancia_iniciar(Wire, PINOS_DISTANCIA);
    uint32_t inicio = millis();
    while (millis() - inicio < 2 * IDADE_MAXIMA_MS) {
        distancia_ler(distancias);
        delay(5);
    }
    Serial.printf("[%7.2f s] sensores iniciados: %s\n", millis() / 1000.0f,
                  ok ? "os três ok" : "algum falhou");
}

void setup() {
    pinMode(Pinos::LED_STATUS, OUTPUT);
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Teste do fio do sensor da direita ===");
    Serial.println("Mexa nos fios do sensor da direita um de cada vez.");
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    iniciar_sensores();
    anterior = estado_direita();
    Serial.printf("[%7.2f s] direita: %s\n", millis() / 1000.0f, NOMES[anterior]);
    inicio_ok = millis();
}

void loop() {
    static uint32_t ultimo = 0;
    if (millis() - ultimo < PERIODO_MS) {
        return;
    }
    ultimo = millis();

    distancia_ler(distancias);
    EstadoDireita agora = estado_direita();
    digitalWrite(Pinos::LED_STATUS, agora == DIREITA_OK);

    if (agora != anterior) {
        if (agora != DIREITA_OK) {
            quedas++;
            Serial.printf("[%7.2f s] >>> QUEDA %lu: direita %s (ficou ok por %.1f s)\n",
                          millis() / 1000.0f, (unsigned long)quedas, NOMES[agora],
                          (millis() - inicio_ok) / 1000.0f);
        } else {
            Serial.printf("[%7.2f s] direita voltou: ok\n", millis() / 1000.0f);
            inicio_ok = millis();
        }
        anterior = agora;
    }

    if (agora == VOLTOU_FABRICA || agora == SEM_MEDIDA) {
        // Com o sensor de volta no barramento, inicia de novo para seguir testando.
        iniciar_sensores();
        agora = estado_direita();
        if (agora == DIREITA_OK) {
            Serial.printf("[%7.2f s] direita voltou: ok\n", millis() / 1000.0f);
            inicio_ok = millis();
        }
        anterior = agora;
    }

    if (millis() - ultimo_resumo >= PERIODO_RESUMO_MS) {
        ultimo_resumo = millis();
        Serial.printf("direita: %-6s %5u mm | frente %5u | esquerda %5u | quedas: %lu\n",
                      agora == DIREITA_OK ? "ok" : "FALHA", distancias.direita, distancias.frente,
                      distancias.esquerda, (unsigned long)quedas);
    }
}
