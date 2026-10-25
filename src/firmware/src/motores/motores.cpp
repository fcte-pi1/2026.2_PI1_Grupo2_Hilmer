#include "motores.h"

#include <Arduino.h>

// O L298N usa transistores bipolares, que chaveiam devagar: em frequências altas o
// motor perde torque e a ponte esquenta. 1 kHz é o valor usual para esse módulo.
constexpr uint32_t PWM_FREQUENCIA_HZ = 1000;
constexpr uint8_t PWM_RESOLUCAO_BITS = 10;
constexpr uint32_t PWM_DUTY_TOTAL = (1u << PWM_RESOLUCAO_BITS) - 1;

static PinosMotor motor_esquerdo;
static PinosMotor motor_direito;
static bool habilitada = false;

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

// ENA/ENB em nível baixo: o canal fica desligado e o motor gira livre.
static void soltar(const PinosMotor &motor) {
    digitalWrite(motor.in1, LOW);
    digitalWrite(motor.in2, LOW);
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
        soltar(motor);
        return;
    }

    float duty = fabsf(velocidade) * duty_maximo;
    ledcWrite(motor.pwm, (uint32_t)lroundf(duty * PWM_DUTY_TOTAL));
}

void motores_iniciar(const PinosMotor &esquerdo, const PinosMotor &direito) {
    habilitada = false;
    motor_esquerdo = esquerdo;
    motor_direito = direito;
    configurar_canal(motor_esquerdo);
    configurar_canal(motor_direito);
}

void motores_habilitar(bool ligar) {
    habilitada = ligar;
    if (!habilitada) {
        soltar(motor_esquerdo);
        soltar(motor_direito);
    }
}

void motores_definir_velocidade(float esquerdo, float direito) {
    if (!habilitada) {
        return;
    }
    aplicar_velocidade(motor_esquerdo, esquerdo);
    aplicar_velocidade(motor_direito, direito);
}

void motores_frear() {
    if (!habilitada) {
        return;
    }
    // No L298N, IN1 = IN2 = 1 com o ENA em nível alto curto-circuita os terminais
    // do motor pelos transistores de cima, o que freia a roda.
    digitalWrite(motor_esquerdo.in1, HIGH);
    digitalWrite(motor_esquerdo.in2, HIGH);
    digitalWrite(motor_direito.in1, HIGH);
    digitalWrite(motor_direito.in2, HIGH);
    ledcWrite(motor_esquerdo.pwm, PWM_DUTY_TOTAL);
    ledcWrite(motor_direito.pwm, PWM_DUTY_TOTAL);
}

void motores_definir_duty_maximo(float duty) {
    duty_maximo = constrain(duty, 0.0f, 1.0f);
}
