#include "deteccao_lado.h"

#include "espelho.h"

DeteccaoLado::DeteccaoLado(int altura) : altura_(altura) {}

void DeteccaoLado::reiniciar() {
    lado_ = LADO_DESCONHECIDO;
    linhas_ = 0;
    abertas_direita_ = 0;
    abertas_esquerda_ = 0;
    for (int y = 0; y < LABIRINTO_MAX_ALTURA; y++) {
        parede_direita_[y] = false;
        parede_esquerda_[y] = false;
    }
}

LadoLabirinto DeteccaoLado::celula(int y, const bool *direita_aberta, const bool *esquerda_aberta,
                                   int n) {
    if (lado_ != LADO_DESCONHECIDO || y < 0 || y >= altura_ || n <= 0) {
        return lado_;
    }
    int direita = 0, esquerda = 0;
    for (int i = 0; i < n; i++) {
        direita += direita_aberta[i] ? 1 : 0;
        esquerda += esquerda_aberta[i] ? 1 : 0;
    }
    abertas_direita_ += direita;
    abertas_esquerda_ += esquerda;
    // A parede vale pela maioria das amostras.
    parede_direita_[y] = direita * 2 <= n;
    parede_esquerda_[y] = esquerda * 2 <= n;
    if (y + 1 > linhas_) {
        linhas_ = y + 1;
    }
    // Uma amostra errada de cada lado é tolerada: com a regra de "todas abertas e
    // nenhuma do outro lado", uma leitura falsa na borda fazia o robô passar pela
    // abertura sem reconhecer (simulado: 5 de 600 starts errados com 10% de erro).
    int folga = n >= 3 ? 1 : 0;
    if (direita >= n - folga && esquerda <= folga) {
        lado_ = LADO_DIREITA;
    } else if (esquerda >= n - folga && direita <= folga) {
        lado_ = LADO_ESQUERDA;
    }
    return lado_;
}

LadoLabirinto DeteccaoLado::escolher(LadoLabirinto padrao) {
    if (lado_ != LADO_DESCONHECIDO) {
        return lado_;
    }
    if (abertas_direita_ > abertas_esquerda_) {
        lado_ = LADO_DIREITA;
    } else if (abertas_esquerda_ > abertas_direita_) {
        lado_ = LADO_ESQUERDA;
    } else {
        lado_ = padrao;
    }
    // Se chegou aqui, alguma abertura do lado do labirinto foi lida como parede. As
    // paredes guardadas desse lado não são confiáveis: ficam fora do mapa, e o Flood Fill
    // as trata como abertas. Quando o robô voltar a uma dessas células, ele lê as paredes
    // de novo e marca as que existem. As do outro lado (a borda) continuam.
    for (int y = 0; y < linhas_; y++) {
        if (lado_ == LADO_DIREITA) {
            parede_direita_[y] = false;
        } else if (lado_ == LADO_ESQUERDA) {
            parede_esquerda_[y] = false;
        }
    }
    return lado_;
}

bool DeteccaoLado::parede(int y, int lado_robo) const {
    if (y < 0 || y >= LABIRINTO_MAX_ALTURA) {
        return true;
    }
    return lado_robo == 1 ? parede_direita_[y] : parede_esquerda_[y];
}

void DeteccaoLado::aplicar(Labirinto &mapa) const {
    if (lado_ == LADO_DESCONHECIDO) {
        return;
    }
    bool a_direita = lado_ == LADO_DIREITA;
    for (int y = 0; y < linhas_; y++) {
        if (parede_direita_[y]) {
            mapa.colocarParede(0, y, lado_no_mapa(NORTE, 1, a_direita));
        }
        if (parede_esquerda_[y]) {
            mapa.colocarParede(0, y, lado_no_mapa(NORTE, 3, a_direita));
        }
    }
}
