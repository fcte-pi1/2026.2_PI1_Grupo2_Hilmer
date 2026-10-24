#include "sensores.h"

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

// Modo rápido do I2C: todos os dispositivos do barramento suportam (doc 4.3, seção 4.1).
constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;

static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

// Começa como inválida até cada sensor entregar a primeira medida.
static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

void sensores_iniciar() {
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);

    if (!distancia_iniciar(Wire, PINOS_DISTANCIA)) {
        Serial.println("Erro: algum VL53L0X não iniciou");
    }

    // O robô precisa estar parado durante a calibração do giroscópio (cerca de 1 s).
    if (!imu_iniciar(Wire)) {
        Serial.println("Erro: MPU6050 não respondeu");
    } else if (!imu_calibrar()) {
        Serial.println("Aviso: robô se mexeu na calibração do giroscópio, offset não medido");
    }

    if (!energia_iniciar(Wire)) {
        Serial.println("Erro: INA219 não respondeu");
    }
}

void sensores_ler(LeituraSensores &leitura) {
    // Se uma leitura falhar, o campo fica com o último valor lido.
    static LeituraSensores ultima = {};

    distancia_ler(distancias);
    ultima.distancia_frente = distancias.frente;
    ultima.distancia_esquerda = distancias.esquerda;
    ultima.distancia_direita = distancias.direita;

    imu_ler_giro_z(ultima.giro_z);

    energia_ler(ultima.tensao_bateria, ultima.corrente_bateria);

    leitura = ultima;
}
