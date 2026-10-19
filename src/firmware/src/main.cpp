#include <Arduino.h>

#include "armazenamento/armazenamento.h"
#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "sensores/sensores.h"
#include "tarefas/tarefas.h"

// O sentido de cada motor deve ser conferido na placa: velocidade positiva = robô para a frente.
static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, false};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, true};

// O sinal de cada encoder deve ser conferido na placa: robô para a frente = contagem positiva.
static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, true};

static const PinosSD PINOS_SD = {Pinos::SD_SCK, Pinos::SD_MISO, Pinos::SD_MOSI, Pinos::SD_CS};

void setup() {
    // Motores primeiro, para a ponte H ficar desligada o quanto antes.
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO, Pinos::MOTOR_STBY);

    Serial.begin(115200);

    if (!encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO)) {
        Serial.println("Erro: falha ao configurar o PCNT dos encoders");
    }
    sensores_iniciar();
    if (!armazenamento_iniciar(PINOS_SD)) {
        Serial.println("Aviso: cartão SD não encontrado");
    }

    tarefas_criar();

    // Só libera a ponte H com tudo iniciado.
    motores_habilitar(true);
    Serial.println("Micromouse iniciado");
}

void loop() {
    // Todo o trabalho acontece nas tarefas do FreeRTOS.
    vTaskDelay(portMAX_DELAY);
}
