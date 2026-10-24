// Driver dos motores (módulo de ponte H L298N).
#pragma once

#include <cstdint>

// Tensão nominal dos motores N20 e tensão da bateria LiPo 2S cheia (doc 4.3, seções 6.1 e 6.3).
constexpr float MOTOR_TENSAO_NOMINAL = 6.0f;
constexpr float MOTOR_TENSAO_BATERIA_CHEIA = 8.4f;

// Pinos de um canal da ponte H.
struct PinosMotor {
    uint8_t pwm;  // ENA ou ENB: velocidade
    uint8_t in1;  // IN1 ou IN3: sentido
    uint8_t in2;  // IN2 ou IN4: sentido
    // Troca o sentido de giro, para que velocidade positiva seja sempre para a frente.
    // Os motores ficam espelhados no chassi, então um dos dois costuma precisar disso.
    bool invertido;
};

// Configura os pinos e o PWM. A ponte H fica desligada (ENA e ENB em nível baixo)
// até motores_habilitar(true).
void motores_iniciar(const PinosMotor &esquerdo, const PinosMotor &direito);

// Liga ou desliga a ponte H. Desligada, ENA e ENB ficam em nível baixo, os motores
// ficam livres e os comandos de velocidade e freio são ignorados (parada de emergência).
void motores_habilitar(bool habilitada);

// Velocidade de -1.0 (ré total) a 1.0 (frente total). Zero deixa o motor livre.
void motores_definir_velocidade(float esquerdo, float direito);

// Freio ativo nas duas rodas (IN1 = IN2 = 1 com o ENA/ENB em nível alto).
void motores_frear();

// Teto do duty cycle (0.0 a 1.0), aplicado a todo comando de velocidade.
// O padrão é MOTOR_TENSAO_NOMINAL / MOTOR_TENSAO_BATERIA_CHEIA. Com a tensão lida
// pelo INA219, usar bateria_duty_maximo(MOTOR_TENSAO_NOMINAL, tensao) (doc 4.3, seção 6.3).
void motores_definir_duty_maximo(float duty);
