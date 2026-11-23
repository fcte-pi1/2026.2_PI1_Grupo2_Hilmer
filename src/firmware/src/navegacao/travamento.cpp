#include "travamento.h"

void DetectorTravamento::reiniciar(uint32_t agora_ms, int32_t contagem) {
    inicio_ms_ = agora_ms;
    referencia_ms_ = agora_ms;
    referencia_contagem_ = contagem;
}

bool DetectorTravamento::atualizar(uint32_t agora_ms, int32_t contagem) {
    if (agora_ms - inicio_ms_ < carencia_ms_) {
        referencia_ms_ = agora_ms;
        referencia_contagem_ = contagem;
        return false;
    }
    if (contagem - referencia_contagem_ >= progresso_minimo_) {
        // Andou o suficiente: a janela recomeça daqui.
        referencia_ms_ = agora_ms;
        referencia_contagem_ = contagem;
        return false;
    }
    if (agora_ms - referencia_ms_ >= janela_ms_) {
        referencia_ms_ = agora_ms;
        referencia_contagem_ = contagem;
        return true;
    }
    return false;
}
