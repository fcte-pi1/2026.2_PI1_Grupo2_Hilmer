#include "motores.h"

void motores_iniciar() {
    // TODO(#164): manter a ponte H desligada (STBY) até o robô estar pronto,
    // e configurar IN1/IN2 e o PWM (LEDC) dos dois canais.
}

void motores_definir_velocidade(float esquerdo, float direito) {
    // TODO(#164): converter a velocidade em sentido (IN1/IN2) e duty do PWM,
    // respeitando o duty máximo = 6,0 V / tensão da bateria (doc 4.3, seção 6.3).
    (void)esquerdo;
    (void)direito;
}

void motores_frear() {
    // TODO(#164): IN1 = IN2 = 1 nos dois canais.
}
