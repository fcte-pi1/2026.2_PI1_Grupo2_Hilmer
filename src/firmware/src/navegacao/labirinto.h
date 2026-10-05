// Mapa de paredes do labirinto.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
#pragma once

#include <cstdint>

// Maior labirinto da competição: 12x4 células.
constexpr int LABIRINTO_MAX_LARGURA = 12;
constexpr int LABIRINTO_MAX_ALTURA = 4;

// Cada direção é um bit, assim uma célula guarda as quatro paredes em um byte.
enum Direcao : uint8_t {
    NORTE = 1 << 0,
    LESTE = 1 << 1,
    SUL = 1 << 2,
    OESTE = 1 << 3,
};

class Labirinto {
public:
    // Começa só com as paredes da borda externa.
    Labirinto(int largura, int altura);

    int largura() const { return largura_; }
    int altura() const { return altura_; }

    bool dentro(int x, int y) const;
    bool temParede(int x, int y, Direcao direcao) const;

    // Marca a parede na célula e também na vizinha do outro lado.
    void colocarParede(int x, int y, Direcao direcao);

private:
    int largura_;
    int altura_;
    uint8_t paredes_[LABIRINTO_MAX_LARGURA][LABIRINTO_MAX_ALTURA] = {};
};
