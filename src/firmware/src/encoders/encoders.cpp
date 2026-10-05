#include "encoders.h"

void encoders_iniciar() {
    // TODO(#164): criar uma unidade PCNT por roda (driver/pulse_cnt.h do ESP-IDF),
    // em quadratura e com o filtro de glitch ligado (IO36/IO39 pegam ruído do rádio).
}

int32_t encoders_contagem_esquerda() {
    return 0;  // TODO(#164)
}

int32_t encoders_contagem_direita() {
    return 0;  // TODO(#164)
}

void encoders_zerar() {
    // TODO(#164)
}
