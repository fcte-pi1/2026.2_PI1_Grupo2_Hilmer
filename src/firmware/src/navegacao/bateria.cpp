#include "bateria.h"

float bateria_duty_maximo(float tensao_motor, float tensao_bateria) {
    if (tensao_bateria <= 0.0f) {
        return 0.0f;
    }
    float duty = tensao_motor / tensao_bateria;
    return duty > 1.0f ? 1.0f : duty;
}

EstadoBateria bateria_estado(float tensao_bateria) {
    if (tensao_bateria < BATERIA_TENSAO_BLOQUEIO) {
        return BATERIA_BLOQUEADA;
    }
    if (tensao_bateria < BATERIA_TENSAO_AVISO) {
        return BATERIA_BAIXA;
    }
    return BATERIA_NORMAL;
}

// Curva típica da LiPo em repouso: tensão por célula e carga (%).
// O 0 % é o corte de 3,3 V por célula, onde a operação é bloqueada.
static const float TABELA_TENSAO[] = {3.30f, 3.73f, 3.80f, 3.87f, 4.02f, 4.20f};
static const float TABELA_CARGA[] = {0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f};
static const int TABELA_PONTOS = 6;
static const int CELULAS = 2;

float bateria_carga(float tensao_bateria) {
    float tensao_celula = tensao_bateria / CELULAS;

    if (tensao_celula <= TABELA_TENSAO[0]) {
        return 0.0f;
    }
    if (tensao_celula >= TABELA_TENSAO[TABELA_PONTOS - 1]) {
        return 100.0f;
    }

    int i = 1;
    while (tensao_celula > TABELA_TENSAO[i]) {
        i++;
    }
    float fracao = (tensao_celula - TABELA_TENSAO[i - 1]) / (TABELA_TENSAO[i] - TABELA_TENSAO[i - 1]);
    return TABELA_CARGA[i - 1] + fracao * (TABELA_CARGA[i] - TABELA_CARGA[i - 1]);
}
