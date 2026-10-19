// Leitura dos encoders das rodas pelo contador de pulsos (PCNT) do ESP32.
#pragma once

#include <cstdint>

// Canais do encoder de uma roda.
struct PinosEncoder {
    uint8_t a;
    uint8_t b;
    // Troca o sinal da contagem, para que andar para a frente conte para cima.
    // As rodas ficam espelhadas no chassi, então um dos dois costuma precisar disso.
    bool invertido;
};

// Cria uma unidade PCNT por roda. Retorna false se o PCNT não puder ser configurado.
bool encoders_iniciar(const PinosEncoder &esquerdo, const PinosEncoder &direito);

// Contagem acumulada desde o início (quadratura, 4 contagens por pulso).
int32_t encoders_contagem_esquerda();
int32_t encoders_contagem_direita();

void encoders_zerar();
