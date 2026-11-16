#include "contagem_celulas.h"

#include <cmath>

int contagem_celulas(const LeituraTrecho &l, float tamanho_celula_mm) {
    float pelo_encoder = 0.0f;
    if (l.contagens_por_celula > 0) {
        pelo_encoder = (float)(l.contagem < 0 ? -l.contagem : l.contagem) / l.contagens_por_celula;
    }

    bool frente_perto = l.frente_inicio_mm <= CONTAGEM_ALCANCE_FRENTE_MM &&
                        l.frente_fim_mm <= CONTAGEM_ALCANCE_FRENTE_MM &&
                        l.frente_fim_mm < l.frente_inicio_mm;
    float pela_frente = frente_perto
                            ? ((float)l.frente_inicio_mm - (float)l.frente_fim_mm) / tamanho_celula_mm
                            : 0.0f;

    // Encoder parado mas o robô andou (a parede chegou perto): o encoder falhou.
    if (pelo_encoder < CONTAGEM_ENCODER_FALHOU && frente_perto && pela_frente >= 0.75f) {
        return (int)lroundf(pela_frente);
    }
    return (int)lroundf(pelo_encoder);
}
