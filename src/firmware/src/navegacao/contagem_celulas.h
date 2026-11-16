// Quantas células o robô andou num trecho reto. Lógica pura: roda nos testes do PC.
//
// Os encoders medem a distância de cada célula e o robô para a cada uma, então um trecho
// tem 0 ou 1 célula. A parede da frente só confere: de longe o VL53L0X erra muito (o cone
// de ~25° pega quinas de paredes laterais). Numa corrida, a frente leu 383 mm no começo e
// 53 mm no fim de uma célula só (a parede estava a ~234 mm): contando pela frente, o robô
// achou que andou 2 células, ficou uma célula adiantado no mapa e passou a bater.
#pragma once

#include <cstdint>

constexpr uint16_t CONTAGEM_SEM_LEITURA = UINT16_MAX;

struct LeituraTrecho {
    int32_t contagem;             // contagem andada no trecho (encoders)
    int32_t contagens_por_celula;
    uint16_t frente_inicio_mm;    // parado, antes de andar (CONTAGEM_SEM_LEITURA se não houver)
    uint16_t frente_fim_mm;       // parado, depois de andar
};

// Pela frente vale só com a parede perto nos dois momentos (até esse alcance).
constexpr uint16_t CONTAGEM_ALCANCE_FRENTE_MM = 400;
// Abaixo dessa fração de célula pelos encoders, o encoder pode ter falhado: aí a frente,
// se estiver perto, decide.
constexpr float CONTAGEM_ENCODER_FALHOU = 0.25f;

// Células andadas no trecho. Pelos encoders, arredondando; se os encoders quase não
// contaram mas a frente (perto) mostra uma célula ou mais, vale a frente (encoder solto).
int contagem_celulas(const LeituraTrecho &l, float tamanho_celula_mm);
