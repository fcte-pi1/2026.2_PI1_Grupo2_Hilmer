// Passagens já percorridas pelo robô, para corrigir a posição quando ele anda mais do que
// contou. Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// Paredes não mudam de lugar: se o robô para com parede logo na frente numa passagem por
// onde ele já passou, ele não está onde acha que está, e sim mais à frente. Foi o que
// aconteceu depois de uma meia-volta num beco: o robô andou quase duas células, contou
// uma, viu a parede da célula seguinte e marcou no lugar errado, fechando o caminho
// ("sem caminho até o objetivo").
#pragma once

#include "labirinto.h"

class Passagens {
public:
    Passagens(int largura, int altura) : largura_(largura), altura_(altura) {}

    void limpar();

    // O robô passou de (x, y) para a vizinha na direção "direcao".
    void marcar(int x, int y, Direcao direcao);
    bool aberta(int x, int y, Direcao direcao) const;

    // Robô parado em (x, y), virado para "direcao", vendo parede logo na frente. Enquanto
    // a passagem à frente for conhecida como aberta, ele está mais adiante: avança x e y.
    // Para na primeira célula sem passagem conhecida à frente. Retorna quantas células
    // avançou (0 = posição coerente).
    int corrigir(int &x, int &y, Direcao direcao, const Labirinto &mapa) const;

private:
    int largura_;
    int altura_;
    uint8_t abertas_[LABIRINTO_MAX_LARGURA][LABIRINTO_MAX_ALTURA] = {};
};
