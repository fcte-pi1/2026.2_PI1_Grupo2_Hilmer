#include "motores.h"

#include <Arduino.h>

// 20 kHz fica acima da faixa audível, e o TB6612FNG aceita até 100 kHz.
// Com o APB de 80 MHz, 20 kHz permite até 11 bits; 10 bits dão 1024 níveis.
constexpr uint32_t PWM_FREQUENCIA_HZ = 20000;
constexpr uint8_t PWM_RESOLUCAO_BITS = 10;
constexpr uint32_t PWM_DUTY_TOTAL = (1u << PWM_RESOLUCAO_BITS) - 1;

static PinosMotor motor_esquerdo;
static PinosMotor motor_direito;
static uint8_t stby;

// Escrito por uma tarefa e lido por outra; float de 32 bits é atômico no ESP32.
static volatile float duty_maximo = MOTOR_TENSAO_NOMINAL / MOTOR_TENSAO_BATERIA_CHEIA;

static void configurar_canal(const PinosMotor &motor) {
    pinMode(motor.in1, OUTPUT);
    pinMode(motor.in2, OUTPUT);
    digitalWrite(motor.in1, LOW);
    digitalWrite(motor.in2, LOW);
    ledcAttach(motor.pwm, PWM_FREQUENCIA_HZ, PWM_RESOLUCAO_BITS);
    ledcWrite(motor.pwm, 0);
}

static void aplicar_velocidade(const PinosMotor &motor, float velocidade) {
    if (isnan(velocidade)) {
        velocidade = 0.0f;
    }
    if (motor.invertido) {
        velocidade = -velocidade;
    }
    velocidade = constrain(velocidade, -1.0f, 1.0f);

    if (velocidade > 0.0f) {
        digitalWrite(motor.in1, HIGH);
        digitalWrite(motor.in2, LOW);
    } else if (velocidade < 0.0f) {
        digitalWrite(motor.in1, LOW);
        digitalWrite(motor.in2, HIGH);
    } else {
        // IN1 = IN2 = 0: motor livre.
        digitalWrite(motor.in1, LOW);
        digitalWrite(motor.in2, LOW);
    }

    float duty = fabsf(velocidade) * duty_maximo;
    ledcWrite(motor.pwm, (uint32_t)lroundf(duty * PWM_DUTY_TOTAL));
}

void motores_iniciar(const PinosMotor &esquerdo, const PinosMotor &direito, uint8_t pino_stby) {
    // STBY primeiro: o pull-down R1 segura a ponte desligada no boot, e o pino
    // passa a ser saída em nível baixo antes de mexer nos outros.
    stby = pino_stby;
    pinMode(stby, OUTPUT);
    digitalWrite(stby, LOW);

    motor_esquerdo = esquerdo;
    motor_direito = direito;
    configurar_canal(motor_esquerdo);
    configurar_canal(motor_direito);
}

void motores_habilitar(bool habilitada) {
    digitalWrite(stby, habilitada ? HIGH : LOW);
}

void motores_definir_velocidade(float esquerdo, float direito) {
    aplicar_velocidade(motor_esquerdo, esquerdo);
    aplicar_velocidade(motor_direito, direito);
}

void motores_frear() {
    // Com IN1 = IN2 = 1 o TB6612FNG curto-circuita os terminais, seja qual for o PWM.
    digitalWrite(motor_esquerdo.in1, HIGH);
    digitalWrite(motor_esquerdo.in2, HIGH);
    digitalWrite(motor_direito.in1, HIGH);
    digitalWrite(motor_direito.in2, HIGH);
    ledcWrite(motor_esquerdo.pwm, 0);
    ledcWrite(motor_direito.pwm, 0);
}

void motores_definir_duty_maximo(float duty) {
    duty_maximo = constrain(duty, 0.0f, 1.0f);
}
