#include "centralizacao.h"

bool centralizacao_desvio(uint16_t esquerda, uint16_t direita, const CalibracaoCentro &c,
                          float &desvio_mm) {
    bool parede_esquerda = esquerda <= c.limiar_parede_mm;
    bool parede_direita = direita <= c.limiar_parede_mm;
    // Robô para a direita: a esquerda lê mais que no centro e a direita, menos.
    float desvio_esquerda = (float)esquerda - c.centro_esquerda_mm;
    float desvio_direita = c.centro_direita_mm - (float)direita;
    if (parede_esquerda && parede_direita) {
        desvio_mm = 0.5f * (desvio_esquerda + desvio_direita);
    } else if (parede_esquerda) {
        desvio_mm = desvio_esquerda;
    } else if (parede_direita) {
        desvio_mm = desvio_direita;
    } else {
        desvio_mm = 0.0f;
        return false;
    }
    return true;
}

float centralizacao_inclinacao(uint16_t esquerda, uint16_t direita, const CalibracaoCentro &c) {
    float desvio;
    centralizacao_desvio(esquerda, direita, c, desvio);
    float inclinacao = c.ganho_graus_por_mm * desvio;
    if (inclinacao > c.inclinacao_max_graus) {
        inclinacao = c.inclinacao_max_graus;
    } else if (inclinacao < -c.inclinacao_max_graus) {
        inclinacao = -c.inclinacao_max_graus;
    }
    return inclinacao;
}
