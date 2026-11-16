// Lê direto o nível dos pinos dos encoders, sem o PCNT, para achar fio solto.
//
// Gravar:  pio run -e exemplo_pinos_encoder -t upload && pio device monitor
//
// Gire a roda à mão. A cada 500 ms mostra, para cada pino, o nível atual e quantas
// trocas de nível aconteceram. Um canal bom troca dezenas de vezes por volta:
//   - sempre 0 e sem trocas: sem alimentação no encoder (VCC/GND) ou fio em curto;
//   - sempre 1 e sem trocas: fio do canal solto (GPIO34-39 não têm pull-up, então
//     solto também pode ficar oscilando sozinho, com trocas mesmo com a roda parada);
//   - um canal troca e o outro não: só o fio daquele canal.

#include <Arduino.h>

#include "config/pinos.h"

static const uint8_t PINOS[] = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, Pinos::ENC_DIR_A,
                                Pinos::ENC_DIR_B};
static const char *NOMES[] = {"esq C1 (34)", "esq C2 (35)", "dir C1 (36)", "dir C2 (39)"};
static volatile uint32_t trocas[4];

static void IRAM_ATTR troca0() { trocas[0]++; }
static void IRAM_ATTR troca1() { trocas[1]++; }
static void IRAM_ATTR troca2() { trocas[2]++; }
static void IRAM_ATTR troca3() { trocas[3]++; }

void setup() {
    // Ponte H desligada: as rodas giram livres à mão.
    pinMode(Pinos::MOTOR_ESQ_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_ESQ_PWM, LOW);
    pinMode(Pinos::MOTOR_DIR_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_DIR_PWM, LOW);
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Nível dos pinos dos encoders ===");
    void (*funcoes[])() = {troca0, troca1, troca2, troca3};
    for (int i = 0; i < 4; i++) {
        pinMode(PINOS[i], INPUT);
        attachInterrupt(PINOS[i], funcoes[i], CHANGE);
    }
}

void loop() {
    for (int i = 0; i < 4; i++) {
        Serial.printf("%s: %d (%5lu trocas)   ", NOMES[i], digitalRead(PINOS[i]),
                      (unsigned long)trocas[i]);
    }
    Serial.println();
    delay(500);
}
