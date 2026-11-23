// Descobre sozinho para que lado o labirinto continua a partir do start.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// O start fica num canto, com o robô virado para a única saída. As duas laterais da
// primeira coluna têm parede: de um lado é a borda externa do labirinto, que nunca tem
// abertura; do outro, paredes internas. Então a primeira abertura lateral da primeira
// coluna mostra o lado em que o labirinto continua. Até lá o robô só anda reto (é o
// único caminho dessa coluna) e as paredes laterais lidas ficam guardadas; quando o
// lado é descoberto, elas vão para o mapa do lado certo.
//
// Contra leitura falsa de um sensor: em cada célula entram várias amostras das duas
// laterais, e o lado só vale se a abertura aparecer em quase todas (tolera uma errada)
// e o outro lado ficar fechado em quase todas.
#pragma once

#include "labirinto.h"

enum LadoLabirinto : uint8_t {
    LADO_DESCONHECIDO,
    LADO_DIREITA,   // o labirinto continua à direita do robô no start
    LADO_ESQUERDA,  // à esquerda (o mapa do explorar fica espelhado)
};

class DeteccaoLado {
public:
    explicit DeteccaoLado(int altura);

    void reiniciar();
    // Lado já conhecido (configurado no código): pula a detecção.
    void definir(LadoLabirinto lado) { lado_ = lado; }
    LadoLabirinto lado() const { return lado_; }

    // Uma célula da primeira coluna, na linha y, com "n" amostras das laterais vistas
    // pelo robô virado para o norte (true = aberto). Guarda as paredes da célula e, se
    // um lado aparecer aberto em pelo menos n-1 amostras e o outro em no máximo 1
    // (com n >= 3), define o lado.
    LadoLabirinto celula(int y, const bool *direita_aberta, const bool *esquerda_aberta, int n);

    // Sem abertura confirmada e com parede na frente, o robô precisa escolher: usa o lado
    // que apareceu aberto em mais amostras na coluna e, sem nenhuma, "padrao". As paredes
    // guardadas do lado escolhido são descartadas (alguma abertura foi lida como parede).
    LadoLabirinto escolher(LadoLabirinto padrao);

    // Parede lateral guardada na célula (0, y): lado_robo 1 = direita, 3 = esquerda.
    bool parede(int y, int lado_robo) const;

    // Põe no mapa (coordenadas do explorar) as paredes laterais guardadas, com o lado já
    // definido. As células são (0, y), com o robô virado para o norte.
    void aplicar(Labirinto &mapa) const;

private:
    int altura_;
    LadoLabirinto lado_ = LADO_DESCONHECIDO;
    int linhas_ = 0;  // células da coluna já lidas
    bool parede_direita_[LABIRINTO_MAX_ALTURA] = {};
    bool parede_esquerda_[LABIRINTO_MAX_ALTURA] = {};
    int abertas_direita_ = 0;  // amostras abertas na coluna toda, para escolher()
    int abertas_esquerda_ = 0;
};
