#include <Arduino.h>

#include "armazenamento/armazenamento.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "sensores/sensores.h"
#include "tarefas/tarefas.h"

void setup() {
    // Motores primeiro, para a ponte H ficar desligada o quanto antes.
    motores_iniciar();

    Serial.begin(115200);

    encoders_iniciar();
    sensores_iniciar();
    if (!armazenamento_iniciar()) {
        Serial.println("Aviso: cartão SD não encontrado");
    }

    tarefas_criar();
    Serial.println("Micromouse iniciado");
}

void loop() {
    // Todo o trabalho acontece nas tarefas do FreeRTOS.
    vTaskDelay(portMAX_DELAY);
}
