// Regras de proteção da bateria LiPo 2S (doc 4.3, seções 2.4 e 6.3).
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
#pragma once

constexpr float BATERIA_TENSAO_AVISO = 7.0f;
constexpr float BATERIA_TENSAO_BLOQUEIO = 6.6f;

enum EstadoBateria {
    BATERIA_NORMAL,
    BATERIA_BAIXA,
    BATERIA_BLOQUEADA,
};

float bateria_duty_maximo(float tensao_motor, float tensao_bateria);
EstadoBateria bateria_estado(float tensao_bateria);

// Estado de carga estimado pela tensão, de 0 a 100 %.
float bateria_carga(float tensao_bateria);
