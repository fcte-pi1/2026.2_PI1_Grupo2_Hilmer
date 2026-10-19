// Flood Fill: decide para onde o robô vai a cada célula.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// A ideia: cada célula guarda quantos passos faltam até o objetivo, contando só
// com as paredes já descobertas. O robô sempre anda para a vizinha com o menor
// número. Quando uma parede nova aparece, os números são recalculados.
#pragma once

#include <cstdint>

#include "labirinto.h"

// Distância de uma célula que não tem caminho até o objetivo.
constexpr uint8_t DISTANCIA_INFINITA = 255;

// Move (x, y) uma célula na direção indicada.
void avancar(Direcao direcao, int &x, int &y);

class FloodFill {
public:
    // O robô sai do canto (0, 0) e o objetivo é o canto oposto.
    explicit FloodFill(const Labirinto &labirinto);

    bool objetivo(int x, int y) const;

    // Preenche a distância de cada célula até o objetivo, com as paredes atuais do mapa.
    void calcular();

    // Distância da célula no último calcular().
    uint8_t distancia(int x, int y) const;

    // Recalcula as distâncias e escolhe a próxima direção a partir de (x, y).
    // Em empate, prefere seguir na direção atual, para o robô fazer menos curvas.
    // Retorna false se o robô já está no objetivo ou se não existe caminho até ele.
    bool proximaDirecao(int x, int y, Direcao atual, Direcao &proxima);

private:
    const Labirinto &labirinto_;
    int objetivoX_;
    int objetivoY_;
    uint8_t distancias_[LABIRINTO_MAX_LARGURA][LABIRINTO_MAX_ALTURA];
};
