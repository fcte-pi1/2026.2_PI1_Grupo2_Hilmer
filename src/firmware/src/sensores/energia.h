// Monitor de energia INA219 (U4), em série com o positivo da bateria (doc 4.3, seção 2.4).
#pragma once

#include <Wire.h>

#include <cstdint>

constexpr uint8_t ENERGIA_ENDERECO_PADRAO = 0x40;
constexpr float ENERGIA_SHUNT_OHMS = 0.1f;

// O barramento I2C já deve estar iniciado. Retorna false se o INA219 não responder.
bool energia_iniciar(TwoWire &wire, uint8_t endereco = ENERGIA_ENDERECO_PADRAO);

// Tensão da bateria em V e corrente em A (positiva quando a bateria descarrega).
// Retorna false se a leitura falhar; nesse caso os valores não são alterados.
bool energia_ler(float &tensao, float &corrente);
