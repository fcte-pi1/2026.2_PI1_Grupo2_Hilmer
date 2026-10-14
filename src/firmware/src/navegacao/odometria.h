// Odometria: distância e giro do robô a partir dos encoders das rodas.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
#pragma once

#include <cstdint>

// contagens_por_volta = pulsos do encoder x redução x 4 (doc 4.3, seção 6.1).
float odometria_distancia_mm(int32_t contagens, float diametro_roda_mm, int32_t contagens_por_volta);

// Positivo é giro anti-horário (para a esquerda).
float odometria_angulo_graus(float distancia_esquerda_mm, float distancia_direita_mm,
                             float distancia_entre_rodas_mm);
