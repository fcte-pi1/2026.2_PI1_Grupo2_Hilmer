// Driver dos motores (ponte H TB6612FNG).
#pragma once

#include <cstdint>

// Tensão nominal dos motores N20 e tensão da bateria LiPo 2S cheia (doc 4.3, seções 6.1 e 6.3).
constexpr float MOTOR_TENSAO_NOMINAL = 6.0f;
constexpr float MOTOR_TENSAO_BATERIA_CHEIA = 8.4f;

// Pinos de um canal da ponte H.
struct PinosMotor {
    uint8_t pwm;
    uint8_t in1;
    uint8_t in2;
    // Troca o sentido de giro, para que velocidade positiva seja sempre para a frente.
    // Os motores ficam espelhados no chassi, então um dos dois costuma precisar disso.
    bool invertido;
};

// Configura os pinos e o PWM. A ponte H fica desligada (STBY em nível baixo)
// até motores_habilitar(true).
void motores_iniciar(const PinosMotor &esquerdo, const PinosMotor &direito, uint8_t pino_stby);

// Liga ou desliga a ponte H inteira pelo STBY. Desligada, os motores ficam livres
// (também serve de parada de emergência).
void motores_habilitar(bool habilitada);

// Velocidade de -1.0 (ré total) a 1.0 (frente total). Zero deixa o motor livre.
void motores_definir_velocidade(float esquerdo, float direito);

// Freio ativo nas duas rodas (IN1 = IN2 = 1).
void motores_frear();

// Teto do duty cycle (0.0 a 1.0), aplicado a todo comando de velocidade.
// O padrão é MOTOR_TENSAO_NOMINAL / MOTOR_TENSAO_BATERIA_CHEIA. Com a tensão lida
// pelo INA219, usar bateria_duty_maximo(MOTOR_TENSAO_NOMINAL, tensao) (doc 4.3, seção 6.3).
void motores_definir_duty_maximo(float duty);
