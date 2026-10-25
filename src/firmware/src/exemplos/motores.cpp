// Exemplo mínimo do driver dos motores (#233), para testar na placa.
//
// Gravar:  pio run -e exemplo_motores -t upload && pio device monitor
//
// Com o robô suspenso (rodas sem tocar a mesa), cada motor gira para a frente,
// freia, gira para trás e freia. Conferir:
//   - "frente" faz a roda girar no sentido que leva o robô para a frente
//     (se não, trocar o campo invertido em main.cpp e aqui);
//   - o freio para a roda na hora, e com a ponte desligada a roda gira livre à mão.

#include <Arduino.h>

#include "config/pinos.h"
#include "motores/motores.h"

static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, false};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, true};

constexpr float VELOCIDADE_TESTE = 0.5f;
constexpr uint32_t TEMPO_GIRO_MS = 2000;
constexpr uint32_t TEMPO_FREIO_MS = 1000;

static void girar(const char *descricao, float esquerdo, float direito) {
    Serial.println(descricao);
    motores_definir_velocidade(esquerdo, direito);
    delay(TEMPO_GIRO_MS);
    Serial.println("  freio");
    motores_frear();
    delay(TEMPO_FREIO_MS);
}

void setup() {
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);
    Serial.begin(115200);
    delay(2000);

    Serial.printf("Duty máximo padrão: %.2f\n", MOTOR_TENSAO_NOMINAL / MOTOR_TENSAO_BATERIA_CHEIA);
    motores_habilitar(true);
}

void loop() {
    girar("Esquerdo: frente", VELOCIDADE_TESTE, 0.0f);
    girar("Esquerdo: ré", -VELOCIDADE_TESTE, 0.0f);
    girar("Direito: frente", 0.0f, VELOCIDADE_TESTE);
    girar("Direito: ré", 0.0f, -VELOCIDADE_TESTE);
    girar("Os dois: frente (robô andaria para a frente)", VELOCIDADE_TESTE, VELOCIDADE_TESTE);

    Serial.println("Ponte desligada (ENA e ENB em nível baixo) por 5 s: as rodas devem girar livres à mão");
    motores_habilitar(false);
    delay(5000);
    motores_habilitar(true);
}
