// Exemplo mínimo do driver dos sensores de distância VL53L0X (#235), para testar na placa.
//
// Gravar:  pio run -e exemplo_distancia -t upload && pio device monitor
//
// Inicia os quatro sensores (troca de endereço pelos XSHUT) e mostra as distâncias.
// Conferir:
//   - o scan do I2C depois da inicialização mostra 0x30 a 0x33 (os quatro
//     sensores), além do INA219 (0x40) e do BMI088 (0x18 e 0x68);
//   - com uma parede a distâncias conhecidas (por exemplo 50, 100 e 180 mm),
//     anotar a média e o desvio de 50 leituras de cada sensor ("m" no monitor);
//   - o tempo de distancia_ler() com os quatro sensores, que define o período
//     da tarefa sensores.
// Apertar o botão EN da placa (reset sem desligar a alimentação) deve iniciar
// os quatro sensores de novo sem erro.

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "sensores/distancia.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_LEITURA_MS = 20;  // período atual da tarefa sensores
constexpr uint32_t PERIODO_IMPRESSAO_MS = 250;
constexpr int AMOSTRAS_MEDIA = 50;

static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE_DIR, Pinos::XSHUT_FRENTE_ESQ,
                                               Pinos::XSHUT_ESQUERDA};

static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA,
                                DISTANCIA_INVALIDA};

static void escanear_i2c() {
    Serial.print("Dispositivos no I2C:");
    for (uint8_t endereco = 1; endereco < 127; endereco++) {
        Wire.beginTransmission(endereco);
        if (Wire.endTransmission() == 0) {
            Serial.printf(" 0x%02X", endereco);
        }
    }
    Serial.println();
}

// Média e desvio padrão de AMOSTRAS_MEDIA medidas novas de cada sensor.
static void medir_estatisticas() {
    const char *nomes[4] = {"frente_dir", "frente_esq", "esquerda", "direita"};
    float soma[4] = {};
    float soma_quadrados[4] = {};
    int validas[4] = {};

    for (int i = 0; i < AMOSTRAS_MEDIA; i++) {
        delay(PERIODO_LEITURA_MS + 5);  // garante uma medida nova de cada sensor
        Distancias d = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA,
                        DISTANCIA_INVALIDA};
        distancia_ler(d);
        uint16_t valores[4] = {d.frente_dir, d.frente_esq, d.esquerda, d.direita};
        for (int s = 0; s < 4; s++) {
            if (valores[s] != DISTANCIA_INVALIDA) {
                soma[s] += valores[s];
                soma_quadrados[s] += (float)valores[s] * valores[s];
                validas[s]++;
            }
        }
    }

    for (int s = 0; s < 4; s++) {
        if (validas[s] == 0) {
            Serial.printf("%-10s: sem leituras válidas\n", nomes[s]);
            continue;
        }
        float media = soma[s] / validas[s];
        float variancia = soma_quadrados[s] / validas[s] - media * media;
        Serial.printf("%-10s: média %.1f mm, desvio %.1f mm (%d leituras)\n", nomes[s], media,
                      sqrtf(fmaxf(variancia, 0.0f)), validas[s]);
    }
}

void setup() {
    // Mantém a ponte H desligada.
    pinMode(Pinos::MOTOR_STBY, OUTPUT);
    digitalWrite(Pinos::MOTOR_STBY, LOW);

    Serial.begin(115200);
    delay(2000);

    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    uint32_t inicio = millis();
    bool ok = distancia_iniciar(Wire, PINOS_DISTANCIA);
    Serial.printf("Inicialização %s em %lu ms\n", ok ? "ok" : "com erro",
                  (unsigned long)(millis() - inicio));
    escanear_i2c();
    Serial.println("Envie \"m\" para medir média e desvio de cada sensor.");
}

void loop() {
    static uint32_t ultima_impressao = 0;
    static uint32_t tempo_maximo_us = 0;

    if (Serial.available() > 0 && Serial.read() == 'm') {
        medir_estatisticas();
    }

    uint32_t inicio_us = micros();
    distancia_ler(distancias);
    uint32_t tempo_us = micros() - inicio_us;
    tempo_maximo_us = max(tempo_maximo_us, tempo_us);

    if (millis() - ultima_impressao >= PERIODO_IMPRESSAO_MS) {
        ultima_impressao = millis();
        Serial.printf("frente_dir: %5u  frente_esq: %5u  esquerda: %5u  direita: %5u mm"
                      "   leitura: %4lu us (máx. %4lu us)\n",
                      distancias.frente_dir, distancias.frente_esq, distancias.esquerda,
                      distancias.direita, (unsigned long)tempo_us,
                      (unsigned long)tempo_maximo_us);
    }

    delay(PERIODO_LEITURA_MS);
}
