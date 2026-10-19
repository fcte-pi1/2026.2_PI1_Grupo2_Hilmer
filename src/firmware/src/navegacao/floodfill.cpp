#include "floodfill.h"

static const Direcao DIRECOES[] = {NORTE, LESTE, SUL, OESTE};

void avancar(Direcao direcao, int &x, int &y) {
    switch (direcao) {
        case NORTE: y++; break;
        case LESTE: x++; break;
        case SUL: y--; break;
        case OESTE: x--; break;
    }
}

FloodFill::FloodFill(const Labirinto &labirinto)
    : labirinto_(labirinto),
      objetivoX_(labirinto.largura() - 1),
      objetivoY_(labirinto.altura() - 1) {
    calcular();
}

bool FloodFill::objetivo(int x, int y) const {
    return x == objetivoX_ && y == objetivoY_;
}

void FloodFill::calcular() {
    for (int x = 0; x < LABIRINTO_MAX_LARGURA; x++) {
        for (int y = 0; y < LABIRINTO_MAX_ALTURA; y++) {
            distancias_[x][y] = DISTANCIA_INFINITA;
        }
    }

    // Busca em largura a partir do objetivo: primeiro as células a 1 passo,
    // depois as a 2 passos, e assim por diante. A fila cabe o labirinto inteiro.
    int filaX[LABIRINTO_MAX_LARGURA * LABIRINTO_MAX_ALTURA];
    int filaY[LABIRINTO_MAX_LARGURA * LABIRINTO_MAX_ALTURA];
    int inicio = 0;
    int fim = 0;

    distancias_[objetivoX_][objetivoY_] = 0;
    filaX[fim] = objetivoX_;
    filaY[fim] = objetivoY_;
    fim++;

    while (inicio < fim) {
        int x = filaX[inicio];
        int y = filaY[inicio];
        inicio++;

        for (Direcao direcao : DIRECOES) {
            if (labirinto_.temParede(x, y, direcao)) {
                continue;
            }
            int vizinhoX = x;
            int vizinhoY = y;
            avancar(direcao, vizinhoX, vizinhoY);
            if (distancias_[vizinhoX][vizinhoY] != DISTANCIA_INFINITA) {
                continue;  // já foi visitada, por um caminho igual ou mais curto
            }
            distancias_[vizinhoX][vizinhoY] = distancias_[x][y] + 1;
            filaX[fim] = vizinhoX;
            filaY[fim] = vizinhoY;
            fim++;
        }
    }
}

uint8_t FloodFill::distancia(int x, int y) const {
    if (!labirinto_.dentro(x, y)) {
        return DISTANCIA_INFINITA;
    }
    return distancias_[x][y];
}

bool FloodFill::proximaDirecao(int x, int y, Direcao atual, Direcao &proxima) {
    calcular();
    if (objetivo(x, y) || distancia(x, y) == DISTANCIA_INFINITA) {
        return false;
    }

    // Começa pela direção atual; as outras só ganham se forem mais curtas.
    uint8_t melhor = DISTANCIA_INFINITA;
    Direcao candidatas[] = {atual, NORTE, LESTE, SUL, OESTE};
    for (Direcao direcao : candidatas) {
        if (labirinto_.temParede(x, y, direcao)) {
            continue;
        }
        int vizinhoX = x;
        int vizinhoY = y;
        avancar(direcao, vizinhoX, vizinhoY);
        if (distancia(vizinhoX, vizinhoY) < melhor) {
            melhor = distancia(vizinhoX, vizinhoY);
            proxima = direcao;
        }
    }
    return true;
}
