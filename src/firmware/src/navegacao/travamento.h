// Detecta o robô travado andando para a frente (encostado numa parede ou preso numa
// quina): os encoders quase não avançam enquanto os motores estão ligados.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
#pragma once

#include <cstdint>

class DetectorTravamento {
public:
    // "janela_ms": tempo sem progresso para considerar travado. "progresso_minimo":
    // contagens que o robô precisa andar nessa janela. "carencia_ms": logo depois de sair
    // do repouso a roda ainda está acelerando, então não conta.
    DetectorTravamento(uint32_t janela_ms, int32_t progresso_minimo, uint32_t carencia_ms)
        : janela_ms_(janela_ms), progresso_minimo_(progresso_minimo), carencia_ms_(carencia_ms) {}

    // Começo do movimento (ou retomada depois da ré).
    void reiniciar(uint32_t agora_ms, int32_t contagem);

    // Chamar a cada ciclo com a contagem andada. Retorna true uma vez ao travar; depois
    // recomeça a contar a partir daí.
    bool atualizar(uint32_t agora_ms, int32_t contagem);

private:
    uint32_t janela_ms_;
    int32_t progresso_minimo_;
    uint32_t carencia_ms_;
    uint32_t inicio_ms_ = 0;
    uint32_t referencia_ms_ = 0;
    int32_t referencia_contagem_ = 0;
};
