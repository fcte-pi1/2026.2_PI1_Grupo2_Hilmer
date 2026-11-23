// Confere se os XSHUT da frente e da esquerda desligam mesmo os sensores.
//
// Gravar:  pio run -e exemplo_xshut -t upload && pio device monitor
//
// Com os dois XSHUT em nível baixo, só o sensor da direita (sem XSHUT) deveria
// responder no I2C. O teste muda o endereço de quem está no 0x29 para 0x33 e
// confere se o 0x29 ficou livre. Se continuar respondendo, há outro sensor no
// 0x29 que o XSHUT não desligou (fio do XSHUT solto). Depois liga um XSHUT de
// cada vez e confere se aparece um sensor novo no 0x29.

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 100000;
constexpr uint8_t ENDERECO_FABRICA = 0x29;
constexpr uint8_t ENDERECO_TESTE = 0x33;
constexpr uint8_t REGISTRADOR_ENDERECO = 0x8A;  // I2C_SLAVE_DEVICE_ADDRESS

static bool responde(uint8_t endereco) {
    Wire.beginTransmission(endereco);
    return Wire.endTransmission() == 0;
}

static void escanear(const char *quando) {
    Serial.printf("%-40s I2C:", quando);
    for (uint8_t endereco = 1; endereco < 127; endereco++) {
        if (responde(endereco)) {
            Serial.printf(" 0x%02X", endereco);
        }
    }
    Serial.println();
}

static void mudar_endereco(uint8_t de, uint8_t para) {
    Wire.beginTransmission(de);
    Wire.write(REGISTRADOR_ENDERECO);
    Wire.write(para & 0x7F);
    Wire.endTransmission();
    delay(5);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Teste dos XSHUT ===");

    pinMode(Pinos::XSHUT_FRENTE, OUTPUT);
    digitalWrite(Pinos::XSHUT_FRENTE, LOW);
    pinMode(Pinos::XSHUT_ESQUERDA, OUTPUT);
    digitalWrite(Pinos::XSHUT_ESQUERDA, LOW);
    delay(20);
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);

    escanear("1. XSHUT frente e esquerda em 0:");
    // Junta no 0x33 quem estiver no 0x29 ou no 0x30 (o da direita, de um boot anterior).
    if (responde(0x30)) {
        mudar_endereco(0x30, ENDERECO_TESTE);
    }
    if (responde(ENDERECO_FABRICA)) {
        mudar_endereco(ENDERECO_FABRICA, ENDERECO_TESTE);
    }
    escanear("2. depois de mover o 0x29 para 0x33:");
    bool sobrou = responde(ENDERECO_FABRICA);
    Serial.println(sobrou ? "   -> o 0x29 continua ocupado: algum XSHUT não desliga o sensor dele."
                          : "   -> 0x29 livre: com os XSHUT em 0 só a direita fica ligada (ok).");

    pinMode(Pinos::XSHUT_FRENTE, INPUT);  // liga a frente (pull-up do módulo)
    delay(20);
    bool frente = responde(ENDERECO_FABRICA);
    escanear("3. ligando só o XSHUT da frente:");
    Serial.println(frente ? "   -> frente apareceu no 0x29 (ok)."
                          : "   -> frente NÃO apareceu: XSHUT, VIN, GND, SDA ou SCL da frente.");
    if (frente) {
        mudar_endereco(ENDERECO_FABRICA, 0x34);
    }

    pinMode(Pinos::XSHUT_ESQUERDA, INPUT);
    delay(20);
    bool esquerda = responde(ENDERECO_FABRICA);
    escanear("4. ligando também o XSHUT da esquerda:");
    Serial.println(esquerda ? "   -> esquerda apareceu no 0x29 (ok)."
                            : "   -> esquerda NÃO apareceu: XSHUT, VIN, GND, SDA ou SCL da esquerda.");
    Serial.println("Fim. Desligue a alimentação para os sensores voltarem ao 0x29.");
}

void loop() {}
