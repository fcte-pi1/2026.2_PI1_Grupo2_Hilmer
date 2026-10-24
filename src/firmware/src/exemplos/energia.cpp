// Exemplo mínimo do driver do monitor de energia INA219 (#237), para testar na placa.
//
// Gravar:  pio run -e exemplo_energia -t upload && pio device monitor
//
// Mostra a tensão da bateria e a corrente a cada 500 ms (média de 10 leituras).
// Conferir:
//   - o scan do I2C mostra o INA219 em 0x40;
//   - a tensão bate com o multímetro nos terminais da bateria;
//   - a corrente bate com o multímetro em série com a bateria, em repouso e com
//     carga (por exemplo, motores girando no exemplo_motores);
//   - a corrente é positiva com a bateria descarregando (se não, VIN+ e VIN-
//     estão trocados na placa).

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "sensores/energia.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr int LEITURAS_POR_MEDIA = 10;

// Mostra os endereços que respondem no barramento I2C.
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

void setup() {
    // Mantém a ponte H desligada.
    pinMode(Pinos::MOTOR_ESQ_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_ESQ_PWM, LOW);
    pinMode(Pinos::MOTOR_DIR_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_DIR_PWM, LOW);

    Serial.begin(115200);
    delay(2000);

    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    escanear_i2c();
    // Os NACKs do scan deixam o driver I2C do ESP-IDF 5 travado; reinicia o barramento.
    Wire.end();
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    while (!energia_iniciar(Wire)) {
        Serial.printf("INA219 não respondeu no endereço 0x%02X, tentando de novo\n",
                      ENERGIA_ENDERECO_PADRAO);
        delay(1000);
    }
    Serial.println("INA219 iniciado");
}

void loop() {
    float soma_tensao = 0.0f;
    float soma_corrente = 0.0f;
    int validas = 0;

    for (int i = 0; i < LEITURAS_POR_MEDIA; i++) {
        float tensao, corrente;
        if (energia_ler(tensao, corrente)) {
            soma_tensao += tensao;
            soma_corrente += corrente;
            validas++;
        }
        delay(50);
    }

    if (validas == 0) {
        Serial.println("Falha na leitura do INA219");
        return;
    }
    Serial.printf("tensão: %.3f V   corrente: %.1f mA\n", soma_tensao / validas,
                  1000.0f * soma_corrente / validas);
}
