#include "sensores.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

// Modo rápido do I2C: todos os dispositivos do barramento suportam (doc 4.3, seção 4.1).
constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;

void sensores_iniciar() {
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);

    // O robô precisa estar parado durante a calibração do giroscópio (cerca de 1 s).
    if (!imu_iniciar(Wire)) {
        Serial.println("Erro: BMI088 não respondeu");
    } else if (!imu_calibrar()) {
        Serial.println("Aviso: robô se mexeu na calibração do giroscópio, offset não medido");
    }

    if (!energia_iniciar(Wire)) {
        Serial.println("Erro: INA219 não respondeu");
    }

    // TODO: trocar o endereço de cada VL53L0X pelos XSHUT (#235).
}

void sensores_ler(LeituraSensores &leitura) {
    // Se uma leitura falhar, o campo fica com o último valor lido.
    static LeituraSensores ultima = {};

    imu_ler_giro_z(ultima.giro_z);

    energia_ler(ultima.tensao_bateria, ultima.corrente_bateria);

    leitura = ultima;
}
