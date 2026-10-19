// Giroscópio da IMU BMI088 (U5) por I2C (doc 4.3, seção 4.3).
// O acelerômetro do BMI088 (0x18) não é usado.
#pragma once

#include <Wire.h>

#include <cstdint>

// SDO2 no GND (doc 4.3, seção 4.3).
constexpr uint8_t IMU_ENDERECO_PADRAO = 0x68;

// O barramento I2C já deve estar iniciado. Reinicia e configura o giroscópio.
// Retorna false se o BMI088 não responder ou o chip ID não conferir.
bool imu_iniciar(TwoWire &wire, uint8_t endereco = IMU_ENDERECO_PADRAO);

// Mede o offset do eixo Z com o robô parado (leva cerca de 1 s).
// Retorna false se o robô se mexeu durante a medição; nesse caso o offset anterior é mantido.
bool imu_calibrar();

// Velocidade angular no eixo Z, em graus/s, já sem o offset.
// Positivo é giro anti-horário visto de cima (para a esquerda), com o módulo
// montado com o componente para cima. Retorna false se a leitura falhar.
bool imu_ler_giro_z(float &graus_por_segundo);
