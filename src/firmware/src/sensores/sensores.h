// Sensores do barramento I2C: 4x VL53L0X, BMI088 e INA219.
#pragma once

#include <cstdint>

struct LeituraSensores {
    // Distância até a parede, em mm (VL53L0X U7 a U10).
    uint16_t distancia_frente_dir;
    uint16_t distancia_frente_esq;
    uint16_t distancia_esquerda;
    uint16_t distancia_direita;

    // Velocidade angular no eixo Z, em graus/s (giroscópio do BMI088).
    float giro_z;

    // Bateria (INA219).
    float tensao_bateria;
    float corrente_bateria;
};

// Liga o I2C e troca o endereço dos VL53L0X pelos pinos XSHUT (doc 4.3, seção 4.2).
void sensores_iniciar();

void sensores_ler(LeituraSensores &leitura);
