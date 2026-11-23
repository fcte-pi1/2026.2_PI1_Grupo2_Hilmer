#include "passagens.h"

#include "floodfill.h"

static Direcao oposta(Direcao d) {
    switch (d) {
    case NORTE: return SUL;
    case SUL: return NORTE;
    case LESTE: return OESTE;
    default: return LESTE;
    }
}

void Passagens::limpar() {
    for (int x = 0; x < LABIRINTO_MAX_LARGURA; x++) {
        for (int y = 0; y < LABIRINTO_MAX_ALTURA; y++) {
            abertas_[x][y] = 0;
        }
    }
}

void Passagens::marcar(int x, int y, Direcao direcao) {
    int nx = x, ny = y;
    avancar(direcao, nx, ny);
    if (x < 0 || y < 0 || x >= largura_ || y >= altura_ || nx < 0 || ny < 0 || nx >= largura_ ||
        ny >= altura_) {
        return;
    }
    abertas_[x][y] |= direcao;
    abertas_[nx][ny] |= oposta(direcao);
}

bool Passagens::aberta(int x, int y, Direcao direcao) const {
    if (x < 0 || y < 0 || x >= largura_ || y >= altura_) {
        return false;
    }
    return (abertas_[x][y] & direcao) != 0;
}

int Passagens::corrigir(int &x, int &y, Direcao direcao, const Labirinto &mapa) const {
    int avancou = 0;
    // No máximo o tamanho do labirinto, por segurança.
    for (int i = 0; i < largura_ + altura_; i++) {
        if (!aberta(x, y, direcao)) {
            break;
        }
        int nx = x, ny = y;
        avancar(direcao, nx, ny);
        if (!mapa.dentro(nx, ny)) {
            break;
        }
        x = nx;
        y = ny;
        avancou++;
    }
    return avancou;
}
