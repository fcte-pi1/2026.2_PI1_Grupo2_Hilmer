#include "tarefas.h"

#include <Arduino.h>

#include "armazenamento/armazenamento.h"
#include "comunicacao/comunicacao.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "navegacao/telemetria.h"
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
// A comunicação monta o JSON e usa a pilha do Wi-Fi, então ganha mais espaço.
constexpr uint32_t PILHA_COMUNICACAO = 8192;  // bytes

constexpr const char *ROBOT_ID = "mm-01";

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
    Telemetria telemetria(ROBOT_ID);
    char texto[TELEMETRIA_TAMANHO_MAX];
    for (;;) {
        comunicacao_atualizar();
        if (xQueuePeek(fila_leitura, &leitura, 0) == pdTRUE) {
            // TODO: preencher posição, direção, velocidade, paredes e estado com os
            // dados do controle, e gravar no cartão com armazenamento_gravar_linha().
            DadosTelemetria dados = {};
            dados.direcao = NORTE;
            dados.estado = ROBO_PARADO;
            dados.bateria_pct = bateria_carga(leitura.tensao_bateria);

            int tamanho = telemetria.pacote(millis(), dados, texto, sizeof(texto));
            if (tamanho > 0) {
                // Sem conexão, o pacote é descartado e o backend vê a lacuna no seq.
                comunicacao_enviar(texto, tamanho);
            }
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
    xTaskCreatePinnedToCore(tarefa_comunicacao, "comunicacao", PILHA_COMUNICACAO, nullptr, 1,
                            nullptr, NUCLEO_COMUNICACAO);
}
