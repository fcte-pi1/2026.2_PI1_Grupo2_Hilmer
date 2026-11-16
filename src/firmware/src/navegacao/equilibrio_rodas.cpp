#include "equilibrio_rodas.h"

float equilibrio_rodas(int32_t contagem_esquerda, int32_t contagem_direita, float ganho_por_contagem,
                       float correcao_maxima) {
    int32_t esquerda = contagem_esquerda < 0 ? -contagem_esquerda : contagem_esquerda;
    int32_t direita = contagem_direita < 0 ? -contagem_direita : contagem_direita;
    float correcao = ganho_por_contagem * (float)(esquerda - direita);
    if (correcao > correcao_maxima) {
        return correcao_maxima;
    }
    if (correcao < -correcao_maxima) {
        return -correcao_maxima;
    }
    return correcao;
}

void equilibrio_fatores(float correcao, float &fator_esquerda, float &fator_direita) {
    fator_esquerda = correcao < 0.0f ? 1.0f - correcao : 1.0f;
    fator_direita = correcao > 0.0f ? 1.0f + correcao : 1.0f;
}
