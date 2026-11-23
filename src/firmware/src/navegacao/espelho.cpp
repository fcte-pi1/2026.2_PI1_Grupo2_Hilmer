#include "espelho.h"

int espelho_x(int x, int largura, bool espelhado) {
    return espelhado ? largura - 1 - x : x;
}

Direcao espelho_direcao(Direcao direcao, bool espelhado) {
    if (!espelhado) {
        return direcao;
    }
    if (direcao == LESTE) {
        return OESTE;
    }
    if (direcao == OESTE) {
        return LESTE;
    }
    return direcao;
}

uint8_t espelho_paredes(uint8_t paredes, bool espelhado) {
    if (!espelhado) {
        return paredes;
    }
    uint8_t resultado = paredes & (NORTE | SUL);
    if (paredes & LESTE) {
        resultado |= OESTE;
    }
    if (paredes & OESTE) {
        resultado |= LESTE;
    }
    return resultado;
}

int indice_direcao(Direcao direcao) {
    switch (direcao) {
    case NORTE: return 0;
    case LESTE: return 1;
    case SUL: return 2;
    default: return 3;
    }
}

Direcao direcao_do_indice(int indice) {
    static const Direcao DIRECOES[] = {NORTE, LESTE, SUL, OESTE};
    return DIRECOES[((indice % 4) + 4) % 4];
}

Direcao lado_no_mapa(Direcao atual, int passos_direita, bool labirinto_a_direita) {
    int passos = labirinto_a_direita ? passos_direita : (4 - passos_direita) % 4;
    return direcao_do_indice(indice_direcao(atual) + passos);
}

int passos_ate(Direcao atual, Direcao proxima, bool labirinto_a_direita) {
    int passos = (indice_direcao(proxima) - indice_direcao(atual) + 4) % 4;
    // Mapa espelhado: a direita do mapa é a esquerda do robô.
    return labirinto_a_direita ? passos : (4 - passos) % 4;
}
