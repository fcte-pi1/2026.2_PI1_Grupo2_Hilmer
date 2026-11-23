// Calibração da escala do giroscópio pelas paredes.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// O robô gira devagar no lugar, várias voltas, registrando o ângulo do giroscópio e a
// distância do sensor da frente. Cada vez que ele fica perpendicular a uma parede plana,
// a distância passa por um mínimo. Entre duas passagens pela mesma parede o giro real é
// de exatamente 360°: comparando com o ângulo do giroscópio nesse trecho, sai a escala.
//
// O centro de cada mínimo vem de um ajuste da geometria, não da menor leitura (que tem
// alguns mm de ruído): para uma parede a uma distância D, a leitura com o robô virado
// θ graus vale d = D / cos(θ - θ0). Então 1/d = a·cos θ + b·sin θ, uma regressão linear
// em (a, b), e θ0 = atan2(b, a).
#pragma once

#include <cstdint>

struct AmostraGiro {
    float angulo;     // graus, ângulo integrado do giroscópio (sem escala)
    uint16_t frente;  // mm; acima de CALIB_DISTANCIA_MAXIMA_MM a amostra é ignorada
};

// Leituras mais longe que isso não entram (VL53L0X erra mais, ou é outra parede).
constexpr uint16_t CALIB_DISTANCIA_MAXIMA_MM = 600;
// Meia largura da janela do ajuste de cada mínimo. Até 45° a leitura é da mesma parede.
constexpr float CALIB_JANELA_GRAUS = 35.0f;
// Uma volta de giroscópio é aceita se ficar a até 15% de 360°.
constexpr float CALIB_TOLERANCIA_VOLTA = 0.15f;
// Voltas a mais que isso da mediana são descartadas (par de paredes errado).
constexpr float CALIB_DESVIO_MAXIMO_GRAUS = 3.0f;

constexpr int CALIB_MAX_MINIMOS = 32;


struct ResultadoCalibracaoGiro {
    int minimos;                      // mínimos encontrados (passagens por paredes)
    float centros[CALIB_MAX_MINIMOS]; // ângulo do giroscópio de cada mínimo, em ordem
    int voltas;                       // pares de mínimos da mesma parede, uma volta depois
    float graus_por_volta;            // média do ângulo do giroscópio numa volta real; 0 se não deu
    float desvio_graus;               // desvio padrão das voltas medidas
};

// Analisa as amostras de um giro sempre no mesmo sentido (ângulo crescendo ou caindo).
ResultadoCalibracaoGiro calibracao_giro_analisar(const AmostraGiro *amostras, int n);
