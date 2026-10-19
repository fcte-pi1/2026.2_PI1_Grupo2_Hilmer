// Sensores de distância VL53L0X (U7 a U10) no barramento I2C (doc 4.3, seção 4.2).
#pragma once

#include <Wire.h>

#include <cstdint>

// Pinos XSHUT. O sensor da direita (U10) não tem XSHUT e fica sempre ligado.
struct PinosDistancia {
    uint8_t xshut_frente_dir;  // U7
    uint8_t xshut_frente_esq;  // U8
    uint8_t xshut_esquerda;    // U9
};

// Distâncias até a parede, em mm.
struct Distancias {
    uint16_t frente_dir;
    uint16_t frente_esq;
    uint16_t esquerda;
    uint16_t direita;
};

// Sem parede ao alcance, ou sensor que não iniciou.
constexpr uint16_t DISTANCIA_INVALIDA = UINT16_MAX;

// Endereços atribuídos na inicialização (todos saem de fábrica em 0x29).
constexpr uint8_t DISTANCIA_ENDERECO_DIREITA = 0x30;    // U10
constexpr uint8_t DISTANCIA_ENDERECO_FRENTE_DIR = 0x31;  // U7
constexpr uint8_t DISTANCIA_ENDERECO_FRENTE_ESQ = 0x32;  // U8
constexpr uint8_t DISTANCIA_ENDERECO_ESQUERDA = 0x33;    // U9

// O barramento I2C já deve estar iniciado. Troca o endereço de cada sensor pelos
// XSHUT e deixa os quatro medindo em modo contínuo.
// Retorna false se algum sensor não iniciar; os outros continuam funcionando.
bool distancia_iniciar(TwoWire &wire, const PinosDistancia &pinos);

// Não bloqueia: atualiza só os campos dos sensores que têm medida nova, e os
// outros ficam como estão. Cada sensor faz uma medida nova a cada ~20 ms.
void distancia_ler(Distancias &distancias);
