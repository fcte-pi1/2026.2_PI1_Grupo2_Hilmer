#include "tarefas.h"

#include <Arduino.h>

#include "armazenamento/armazenamento.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "sensores/sensores.h"

// Núcleo 1: tempo real (sensores e controle). Núcleo 0: Wi-Fi e cartão SD.
// Assim a comunicação não atrapalha a temporização do controle (doc 4.3, seção 3.1).
constexpr BaseType_t NUCLEO_CONTROLE = 1;
constexpr BaseType_t NUCLEO_COMUNICACAO = 0;

// Períodos de cada tarefa, em ms.
constexpr uint32_t PERIODO_SENSORES_MS = 20;      // 50 Hz
constexpr uint32_t PERIODO_CONTROLE_MS = 10;      // 100 Hz
constexpr uint32_t PERIODO_COMUNICACAO_MS = 100;  // 10 Hz, taxa da telemetria (doc 4.4)

constexpr uint32_t PILHA_TAREFA = 4096;  // bytes

// Fila de tamanho 1 com a leitura mais recente dos sensores.
// A tarefa de sensores sobrescreve; as outras só espiam (peek), sem remover.
static QueueHandle_t fila_leitura;

static void tarefa_sensores(void *) {
    TickType_t ultimo = xTaskGetTickCount();
    LeituraSensores leitura;
    for (;;) {
        sensores_ler(leitura);
        xQueueOverwrite(fila_leitura, &leitura);
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_SENSORES_MS));
    }
}

static void tarefa_controle(void *) {
    TickType_t ultimo = xTaskGetTickCount();
    LeituraSensores leitura;
    for (;;) {
        if (xQueuePeek(fila_leitura, &leitura, 0) == pdTRUE) {
            // TODO: atualizar o mapa, rodar o Flood Fill e o PID, e mandar a
            // velocidade para os motores com motores_definir_velocidade().
        }
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_CONTROLE_MS));
    }
}

static void tarefa_comunicacao(void *) {
    TickType_t ultimo = xTaskGetTickCount();
    LeituraSensores leitura;
    for (;;) {
        if (xQueuePeek(fila_leitura, &leitura, 0) == pdTRUE) {
            // TODO: montar o pacote de telemetria, enviar por WebSocket (#187)
            // e gravar no cartão com armazenamento_gravar_linha().
        }
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_COMUNICACAO_MS));
    }
}

void tarefas_criar() {
    fila_leitura = xQueueCreate(1, sizeof(LeituraSensores));

    // Prioridade maior = roda primeiro quando duas tarefas estão prontas.
    xTaskCreatePinnedToCore(tarefa_sensores, "sensores", PILHA_TAREFA, nullptr, 3,
                            nullptr, NUCLEO_CONTROLE);
    xTaskCreatePinnedToCore(tarefa_controle, "controle", PILHA_TAREFA, nullptr, 2,
                            nullptr, NUCLEO_CONTROLE);
    xTaskCreatePinnedToCore(tarefa_comunicacao, "comunicacao", PILHA_TAREFA, nullptr, 1,
                            nullptr, NUCLEO_COMUNICACAO);
}
