// Equilíbrio das rodas: as duas devem andar o mesmo tanto (em módulo) desde o começo do
// movimento. Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// Com o mesmo duty, o motor esquerdo gira 6% a 8% mais rápido que o direito (medido com os
// encoders, robô suspenso). No giro no lugar, isso faz o robô andar enquanto gira: o centro
// de giro escorrega e a ponta pega na parede. Comparando as contagens das duas rodas, a
// roda atrasada recebe um pouco mais de velocidade até alcançar a outra.
#pragma once

#include <cstdint>

// Correção c: c > 0 quando a esquerda andou mais que a direita, c < 0 ao contrário.
// "contagem_*" são as contagens desde o começo do movimento (o sinal não importa).
float equilibrio_rodas(int32_t contagem_esquerda, int32_t contagem_direita, float ganho_por_contagem,
                       float correcao_maxima);

// Fatores para multiplicar o módulo da velocidade de cada roda. Só a roda atrasada é
// acelerada (1 + |c|); a adiantada fica com 1. Assim nenhuma roda cai abaixo da
// velocidade pedida: no fim da curva ela já está no mínimo para o N20 vencer o atrito.
void equilibrio_fatores(float correcao, float &fator_esquerda, float &fator_direita);
