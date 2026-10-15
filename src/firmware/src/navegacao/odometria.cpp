#include "odometria.h"

static const float PI_F = 3.14159265f;

float odometria_distancia_mm(int32_t contagens, float diametro_roda_mm, int32_t contagens_por_volta) {
    if (contagens_por_volta <= 0) {
        return 0.0f;
    }
    float voltas = (float)contagens / contagens_por_volta;
    return voltas * PI_F * diametro_roda_mm;
}

float odometria_angulo_graus(float distancia_esquerda_mm, float distancia_direita_mm,
                             float distancia_entre_rodas_mm) {
    if (distancia_entre_rodas_mm <= 0.0f) {
        return 0.0f;
    }
    float radianos = (distancia_direita_mm - distancia_esquerda_mm) / distancia_entre_rodas_mm;
    return radianos * 180.0f / PI_F;
}
