// Centralização do robô no corredor pelas paredes laterais.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// Com o robô no centro do corredor, cada sensor lateral lê um valor próprio (posição no
// chassi e offset de cada VL53L0X): ESQUERDA e DIREITA não leem igual. O desvio do
// centro é medido em relação a esses valores, e não comparando um lado com o outro.
#pragma once

#include <cstdint>

struct CalibracaoCentro {
    float centro_esquerda_mm;  // leitura da esquerda com o robô no centro
    float centro_direita_mm;   // leitura da direita com o robô no centro
    uint16_t limiar_parede_mm; // acima disso (ou sem leitura) o lado não tem parede
    float ganho_graus_por_mm;  // inclinação do rumo por mm fora do centro
    float inclinacao_max_graus;
};

// Quanto o robô está para a direita do centro, em mm (negativo: para a esquerda).
// Com as duas paredes, a média das duas medidas; com uma, só ela. Retorna false sem
// parede nenhuma (o desvio fica em 0).
bool centralizacao_desvio(uint16_t esquerda, uint16_t direita, const CalibracaoCentro &c,
                          float &desvio_mm);

// Inclinação do rumo, em graus, para levar o robô ao centro. Positivo: o robô está à
// direita do centro e o rumo inclina para a esquerda.
float centralizacao_inclinacao(uint16_t esquerda, uint16_t direita, const CalibracaoCentro &c);
