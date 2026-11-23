// Sensores de distância VL53L0X (U7, U9 e U10) no barramento I2C (doc 4.3, seção 4.2).
#pragma once

#include <Wire.h>

#include <cstdint>

// Pinos XSHUT. O sensor da direita (U10) não tem XSHUT e fica sempre ligado.
struct PinosDistancia {
    uint8_t xshut_frente;    // U7
    uint8_t xshut_esquerda;  // U9
};

// Distâncias até a parede, em mm.
struct Distancias {
    uint16_t frente;
    uint16_t esquerda;
    uint16_t direita;
};

// Sem parede ao alcance, ou sensor que não iniciou.
constexpr uint16_t DISTANCIA_INVALIDA = UINT16_MAX;

// Endereços atribuídos na inicialização (todos saem de fábrica em 0x29).
constexpr uint8_t DISTANCIA_ENDERECO_DIREITA = 0x30;  // U10
constexpr uint8_t DISTANCIA_ENDERECO_FRENTE = 0x31;   // U7
constexpr uint8_t DISTANCIA_ENDERECO_ESQUERDA = 0x32;  // U9

// O barramento I2C já deve estar iniciado. Troca o endereço de cada sensor pelos
// XSHUT e deixa os três medindo em modo contínuo.
// Retorna false se algum sensor não iniciar; os outros continuam funcionando.
bool distancia_iniciar(TwoWire &wire, const PinosDistancia &pinos);

// Não bloqueia: atualiza só os campos dos sensores que têm medida nova, e os
// outros ficam como estão. Cada sensor faz uma medida nova a cada ~20 ms.
void distancia_ler(Distancias &distancias);

// true se os três sensores iniciaram e todos entregaram medida nova nos últimos
// idade_maxima_ms. Um sensor que cai do I2C no meio da corrida para de atualizar,
// e o campo dele em Distancias fica com o último valor, que já não vale.
bool distancia_respondendo(uint32_t idade_maxima_ms);

// Quais sensores não iniciaram ou estão sem medida nova há mais de idade_maxima_ms.
constexpr uint8_t DISTANCIA_FALHA_FRENTE = 1;
constexpr uint8_t DISTANCIA_FALHA_ESQUERDA = 2;
constexpr uint8_t DISTANCIA_FALHA_DIREITA = 4;
uint8_t distancia_falhas(uint32_t idade_maxima_ms);

// Nova tentativa só para os sensores em "falhas" (bits acima), sem mexer nos que estão
// medindo. Retorna os que continuam com falha.
uint8_t distancia_reiniciar(TwoWire &wire, const PinosDistancia &pinos, uint8_t falhas);
