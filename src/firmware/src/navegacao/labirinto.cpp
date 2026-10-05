#include "labirinto.h"

// Coordenadas: x cresce para o leste e y cresce para o norte.
// A célula (0, 0) é o canto sudoeste.

Labirinto::Labirinto(int largura, int altura) {
    // Limita ao tamanho máximo para não escrever fora do vetor.
    largura_ = largura > LABIRINTO_MAX_LARGURA ? LABIRINTO_MAX_LARGURA : largura;
    altura_ = altura > LABIRINTO_MAX_ALTURA ? LABIRINTO_MAX_ALTURA : altura;

    for (int x = 0; x < largura_; x++) {
        colocarParede(x, 0, SUL);
        colocarParede(x, altura_ - 1, NORTE);
    }
    for (int y = 0; y < altura_; y++) {
        colocarParede(0, y, OESTE);
        colocarParede(largura_ - 1, y, LESTE);
    }
}

bool Labirinto::dentro(int x, int y) const {
    return x >= 0 && x < largura_ && y >= 0 && y < altura_;
}

bool Labirinto::temParede(int x, int y, Direcao direcao) const {
    // Fora do mapa é tratado como parede, para o robô nunca sair dele.
    if (!dentro(x, y)) {
        return true;
    }
    return (paredes_[x][y] & direcao) != 0;
}

void Labirinto::colocarParede(int x, int y, Direcao direcao) {
    if (!dentro(x, y)) {
        return;
    }
    paredes_[x][y] |= direcao;

    // A mesma parede vista pela célula vizinha.
    int vizinhoX = x;
    int vizinhoY = y;
    Direcao oposta = NORTE;
    switch (direcao) {
        case NORTE: vizinhoY++; oposta = SUL; break;
        case LESTE: vizinhoX++; oposta = OESTE; break;
        case SUL: vizinhoY--; oposta = NORTE; break;
        case OESTE: vizinhoX--; oposta = LESTE; break;
    }
    if (dentro(vizinhoX, vizinhoY)) {
        paredes_[vizinhoX][vizinhoY] |= oposta;
    }
}
