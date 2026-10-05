// Leitura dos encoders das rodas pelo contador de pulsos (PCNT) do ESP32.
#pragma once

#include <cstdint>

void encoders_iniciar();

// Contagem acumulada desde o início (quadratura, 4 contagens por pulso).
int32_t encoders_contagem_esquerda();
int32_t encoders_contagem_direita();

void encoders_zerar();
