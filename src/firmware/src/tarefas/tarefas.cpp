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

// Mostra no monitor serial, a cada INTERVALO_TEMPOS_MS, o período real e o tempo de
// execução de cada tarefa (teste 3.12 do doc 7.3). Desligar na competição.
constexpr bool MOSTRAR_TEMPOS = true;
constexpr uint32_t INTERVALO_TEMPOS_MS = 5000;

// Período medido entre o início de dois ciclos seguidos e duração de cada ciclo.
// As tarefas escrevem e a comunicação lê e zera; como cada campo tem 32 bits, uma
// leitura no meio de um ciclo só desloca uma medida, o que basta para conferência.
struct MedidaTempo {
    uint32_t inicio_anterior_us;
    uint32_t periodo_min_us;
    uint32_t periodo_max_us;
    uint32_t execucao_max_us;
    uint32_t ciclos;
};

static MedidaTempo tempo_sensores;
static MedidaTempo tempo_controle;
static MedidaTempo tempo_comunicacao;

static void medida_registrar(MedidaTempo &medida, uint32_t inicio_us) {
    if (medida.inicio_anterior_us != 0) {
        uint32_t periodo = inicio_us - medida.inicio_anterior_us;
        if (medida.ciclos == 0 || periodo < medida.periodo_min_us) {
            medida.periodo_min_us = periodo;
        }
        medida.periodo_max_us = max(medida.periodo_max_us, periodo);
        medida.ciclos++;
    }
    medida.inicio_anterior_us = inicio_us;
}

static void medida_fim(MedidaTempo &medida, uint32_t inicio_us) {
    medida.execucao_max_us = max(medida.execucao_max_us, (uint32_t)micros() - inicio_us);
}

static void medida_imprimir(const char *nome, MedidaTempo &medida, uint32_t periodo_ms) {
    uint32_t esperados = INTERVALO_TEMPOS_MS / periodo_ms;
    Serial.printf("%-12s %4lu ciclos (esperado %4lu)  período %6.2f a %6.2f ms"
                  "  execução máx. %6.2f ms\n",
                  nome, (unsigned long)medida.ciclos, (unsigned long)esperados,
                  medida.periodo_min_us / 1000.0f, medida.periodo_max_us / 1000.0f,
                  medida.execucao_max_us / 1000.0f);
    medida.periodo_min_us = 0;
    medida.periodo_max_us = 0;
    medida.execucao_max_us = 0;
    medida.ciclos = 0;
}

static void mostrar_tempos() {
    static uint32_t ultima = 0;
    if (!MOSTRAR_TEMPOS || millis() - ultima < INTERVALO_TEMPOS_MS) {
        return;
    }
    ultima = millis();
    medida_imprimir("sensores", tempo_sensores, PERIODO_SENSORES_MS);
    medida_imprimir("controle", tempo_controle, PERIODO_CONTROLE_MS);
    medida_imprimir("comunicacao", tempo_comunicacao, PERIODO_COMUNICACAO_MS);
}

// Grava uma linha CSV com as leituras dos sensores no cartão (teste 3.15 do doc 7.3).
// O número da linha cresce a cada gravação, então uma linha faltando aparece como um salto.
static void gravar_leitura(const LeituraSensores &leitura) {
    static uint32_t linha = 0;
    char texto[128];
    snprintf(texto, sizeof(texto), "%lu,%lu,%u,%u,%u,%.2f,%.3f,%.4f", (unsigned long)linha,
             (unsigned long)millis(), leitura.distancia_frente, leitura.distancia_esquerda,
             leitura.distancia_direita, leitura.giro_z, leitura.tensao_bateria,
             leitura.corrente_bateria);
    if (armazenamento_gravar_linha(texto)) {
        linha++;
    }
}

// Fila de tamanho 1 com a leitura mais recente dos sensores.
// A tarefa de sensores sobrescreve; as outras só espiam (peek), sem remover.
static QueueHandle_t fila_leitura;

static void tarefa_sensores(void *) {
    TickType_t ultimo = xTaskGetTickCount();
    LeituraSensores leitura;
    for (;;) {
        uint32_t inicio_us = micros();
        medida_registrar(tempo_sensores, inicio_us);
        sensores_ler(leitura);
        xQueueOverwrite(fila_leitura, &leitura);
        medida_fim(tempo_sensores, inicio_us);
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_SENSORES_MS));
    }
}

static void tarefa_controle(void *) {
    TickType_t ultimo = xTaskGetTickCount();
    LeituraSensores leitura;
    for (;;) {
        uint32_t inicio_us = micros();
        medida_registrar(tempo_controle, inicio_us);
        if (xQueuePeek(fila_leitura, &leitura, 0) == pdTRUE) {
            // TODO: atualizar o mapa, rodar o Flood Fill e o PID, e mandar a
            // velocidade para os motores com motores_definir_velocidade().
        }
        medida_fim(tempo_controle, inicio_us);
        vTaskDelayUntil(&ultimo, pdMS_TO_TICKS(PERIODO_CONTROLE_MS));
    }
}

static void tarefa_comunicacao(void *) {
    TickType_t ultimo = xTaskGetTickCount();
    LeituraSensores leitura;
    Telemetria telemetria(ROBOT_ID);
    char texto[TELEMETRIA_TAMANHO_MAX];
    // Cabeçalho do CSV. Aparece de novo a cada inicialização, marcando onde ela começou.
    armazenamento_gravar_linha("linha,t_ms,frente_mm,esquerda_mm,direita_mm,giro_z_gps,tensao_v,"
                               "corrente_a");
    for (;;) {
        uint32_t inicio_us = micros();
        medida_registrar(tempo_comunicacao, inicio_us);
        comunicacao_atualizar();
        if (xQueuePeek(fila_leitura, &leitura, 0) == pdTRUE) {
            gravar_leitura(leitura);

            // TODO: preencher posição, direção, velocidade, paredes e estado com os
            // dados do controle.
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
        medida_fim(tempo_comunicacao, inicio_us);
        mostrar_tempos();
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
