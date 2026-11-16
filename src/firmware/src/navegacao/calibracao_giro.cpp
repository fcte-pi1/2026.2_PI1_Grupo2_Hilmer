#include "calibracao_giro.h"

#include <cmath>

static const float PI_F = 3.14159265f;
static const float GRAUS = 180.0f / PI_F;

static bool valida(const AmostraGiro &a) {
    return a.frente > 0 && a.frente <= CALIB_DISTANCIA_MAXIMA_MM;
}

// Centro do mínimo pelo ajuste 1/d = a·cos θ + b·sin θ, com θ medido a partir de
// "referencia" para a conta ficar bem condicionada. Retorna false se não deu.
static bool ajustar_centro(const AmostraGiro *amostras, int n, float referencia, float &centro) {
    double scc = 0, sss = 0, scs = 0, syc = 0, sys = 0;
    int usados = 0;
    for (int i = 0; i < n; i++) {
        if (!valida(amostras[i])) {
            continue;
        }
        float delta = amostras[i].angulo - referencia;
        if (fabsf(delta) > CALIB_JANELA_GRAUS) {
            continue;
        }
        double c = cos(delta / GRAUS);
        double s = sin(delta / GRAUS);
        double y = 1.0 / amostras[i].frente;
        scc += c * c;
        sss += s * s;
        scs += c * s;
        syc += y * c;
        sys += y * s;
        usados++;
    }
    double det = scc * sss - scs * scs;
    if (usados < 8 || fabs(det) < 1e-9) {
        return false;
    }
    double a = (syc * sss - sys * scs) / det;
    double b = (sys * scc - syc * scs) / det;
    if (a <= 0) {
        return false;  // a parede ficaria atrás: não é um mínimo
    }
    centro = referencia + (float)(atan2(b, a) * GRAUS);
    return true;
}


// Para cada mínimo, o mesmo mínimo uma volta depois é o que fica mais perto de 360° de
// distância. A mediana das voltas separa as certas:
// com escala longe de 1, a parede vizinha uma volta depois pode cair na tolerância
// quando falta o par certo. Retorna quantas voltas entraram na média.
static int medir_voltas(const float *eventos, int n, float &graus, float &desvio) {
    float candidatas[CALIB_MAX_MINIMOS];
    int n_candidatas = 0;
    for (int i = 0; i < n && n_candidatas < CALIB_MAX_MINIMOS; i++) {
        float melhor = 0.0f;
        float erro_melhor = 1e9f;
        for (int j = i + 1; j < n; j++) {
            float volta = fabsf(eventos[j] - eventos[i]);
            float erro = fabsf(volta - 360.0f);
            if (erro < erro_melhor) {
                erro_melhor = erro;
                melhor = volta;
            }
        }
        if (erro_melhor <= CALIB_TOLERANCIA_VOLTA * 360.0f) {
            candidatas[n_candidatas++] = melhor;
        }
    }
    graus = 0.0f;
    desvio = 0.0f;
    if (n_candidatas == 0) {
        return 0;
    }
    float ordenadas[CALIB_MAX_MINIMOS];
    for (int i = 0; i < n_candidatas; i++) {
        ordenadas[i] = candidatas[i];
    }
    for (int i = 1; i < n_candidatas; i++) {
        float v = ordenadas[i];
        int j = i - 1;
        while (j >= 0 && ordenadas[j] > v) {
            ordenadas[j + 1] = ordenadas[j];
            j--;
        }
        ordenadas[j + 1] = v;
    }
    float mediana = ordenadas[n_candidatas / 2];
    double soma = 0, soma2 = 0;
    int usadas = 0;
    for (int i = 0; i < n_candidatas; i++) {
        if (fabsf(candidatas[i] - mediana) <= CALIB_DESVIO_MAXIMO_GRAUS) {
            soma += candidatas[i];
            soma2 += (double)candidatas[i] * candidatas[i];
            usadas++;
        }
    }
    graus = (float)(soma / usadas);
    double var = soma2 / usadas - (soma / usadas) * (soma / usadas);
    desvio = (float)sqrt(var > 0 ? var : 0);
    return usadas;
}

ResultadoCalibracaoGiro calibracao_giro_analisar(const AmostraGiro *amostras, int n) {
    ResultadoCalibracaoGiro r = {};

    // 1. Mínimos grosseiros: a amostra é a menor leitura válida numa janela de ±janela.
    //    Depois de achar um, pula a janela inteira para não contar o mesmo mínimo duas vezes.
    float ultimo = 0.0f;
    bool tem_ultimo = false;
    for (int i = 0; i < n && r.minimos < CALIB_MAX_MINIMOS; i++) {
        if (!valida(amostras[i])) {
            continue;
        }
        if (tem_ultimo && fabsf(amostras[i].angulo - ultimo) < 2.0f * CALIB_JANELA_GRAUS) {
            continue;
        }
        bool menor = true;
        bool janela_completa_antes = false;
        bool janela_completa_depois = false;
        for (int j = 0; j < n; j++) {
            float delta = amostras[j].angulo - amostras[i].angulo;
            if (fabsf(delta) > CALIB_JANELA_GRAUS) {
                continue;
            }
            if (fabsf(delta) > 0.8f * CALIB_JANELA_GRAUS) {
                (j < i ? janela_completa_antes : janela_completa_depois) = true;
            }
            if (valida(amostras[j]) && amostras[j].frente < amostras[i].frente) {
                menor = false;
                break;
            }
        }
        // Só conta mínimos com a janela inteira registrada (nem no começo nem no fim do giro).
        if (!menor || !janela_completa_antes || !janela_completa_depois) {
            continue;
        }
        float centro;
        if (ajustar_centro(amostras, n, amostras[i].angulo, centro)) {
            r.centros[r.minimos++] = centro;
            ultimo = amostras[i].angulo;
            tem_ultimo = true;
        }
    }

    // 2. Voltas: o mesmo mínimo uma volta depois.
    r.voltas = medir_voltas(r.centros, r.minimos, r.graus_por_volta, r.desvio_graus);
    return r;
}
