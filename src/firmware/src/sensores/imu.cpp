#include "imu.h"

#include <Arduino.h>

// Registradores do giroscópio do BMI088 (datasheet, seção 10).
constexpr uint8_t REG_CHIP_ID = 0x00;
constexpr uint8_t REG_TAXA_Z_LSB = 0x06;  // seguido do MSB em 0x07
constexpr uint8_t REG_FAIXA = 0x0F;
constexpr uint8_t REG_BANDA = 0x10;
constexpr uint8_t REG_RESET = 0x14;

constexpr uint8_t CHIP_ID = 0x0F;
constexpr uint8_t COMANDO_RESET = 0xB6;
constexpr uint32_t TEMPO_RESET_MS = 30;

// Faixa de ±1000 graus/s: cobre as curvas rápidas com folga e dá 0,03 graus/s por LSB.
constexpr uint8_t FAIXA_1000_GRAUS = 0x01;
constexpr float LSB_POR_GRAU = 32.768f;

// 200 Hz de taxa com filtro de 23 Hz. A tarefa de sensores lê a 50 Hz.
constexpr uint8_t BANDA_200HZ_23HZ = 0x04;
constexpr uint32_t PERIODO_AMOSTRA_MS = 5;

// Calibração: 200 amostras a 200 Hz (1 s). Se a leitura variar mais que isso,
// o robô se mexeu e o offset é descartado.
constexpr int AMOSTRAS_CALIBRACAO = 200;
constexpr float VARIACAO_MAXIMA_CALIBRACAO = 2.0f;  // graus/s

static TwoWire *barramento = nullptr;
static uint8_t endereco_imu = IMU_ENDERECO_PADRAO;
static float offset_z = 0.0f;

static bool escrever_registro(uint8_t registro, uint8_t valor) {
    barramento->beginTransmission(endereco_imu);
    barramento->write(registro);
    barramento->write(valor);
    return barramento->endTransmission() == 0;
}

static bool ler_registros(uint8_t registro, uint8_t *dados, uint8_t tamanho) {
    barramento->beginTransmission(endereco_imu);
    barramento->write(registro);
    if (barramento->endTransmission() != 0) {
        return false;
    }
    if (barramento->requestFrom(endereco_imu, tamanho) != tamanho) {
        return false;
    }
    for (uint8_t i = 0; i < tamanho; i++) {
        dados[i] = barramento->read();
    }
    return true;
}

// Taxa bruta do eixo Z, em graus/s, sem tirar o offset.
static bool ler_taxa_bruta(float &graus_por_segundo) {
    uint8_t dados[2];
    // Ler o LSB primeiro trava o MSB, então os dois bytes são da mesma amostra.
    if (!ler_registros(REG_TAXA_Z_LSB, dados, 2)) {
        return false;
    }
    int16_t bruto = (int16_t)(((uint16_t)dados[1] << 8) | dados[0]);
    graus_por_segundo = bruto / LSB_POR_GRAU;
    return true;
}

bool imu_iniciar(TwoWire &wire, uint8_t endereco) {
    barramento = &wire;
    endereco_imu = endereco;
    offset_z = 0.0f;

    // O reset volta tudo ao padrão; o I2C não responde durante os 30 ms seguintes.
    // O resultado dessa escrita é ignorado, e a presença é conferida pelo chip ID.
    escrever_registro(REG_RESET, COMANDO_RESET);
    delay(TEMPO_RESET_MS);

    uint8_t id = 0;
    if (!ler_registros(REG_CHIP_ID, &id, 1) || id != CHIP_ID) {
        return false;
    }

    if (!escrever_registro(REG_FAIXA, FAIXA_1000_GRAUS) ||
        !escrever_registro(REG_BANDA, BANDA_200HZ_23HZ)) {
        return false;
    }

    // Confere a configuração. O bit 7 da banda sempre é lido como 1.
    uint8_t faixa = 0;
    uint8_t banda = 0;
    if (!ler_registros(REG_FAIXA, &faixa, 1) || !ler_registros(REG_BANDA, &banda, 1)) {
        return false;
    }
    if (faixa != FAIXA_1000_GRAUS || (banda & 0x0F) != BANDA_200HZ_23HZ) {
        return false;
    }

    // Espera o filtro assentar antes da primeira leitura válida.
    delay(50);
    return true;
}

bool imu_calibrar() {
    if (barramento == nullptr) {
        return false;
    }

    float soma = 0.0f;
    float minimo = INFINITY;
    float maximo = -INFINITY;
    int validas = 0;

    for (int i = 0; i < AMOSTRAS_CALIBRACAO; i++) {
        float taxa;
        if (ler_taxa_bruta(taxa)) {
            soma += taxa;
            minimo = fminf(minimo, taxa);
            maximo = fmaxf(maximo, taxa);
            validas++;
        }
        delay(PERIODO_AMOSTRA_MS);
    }

    if (validas < AMOSTRAS_CALIBRACAO / 2 || maximo - minimo > VARIACAO_MAXIMA_CALIBRACAO) {
        return false;
    }
    offset_z = soma / validas;
    return true;
}

bool imu_ler_giro_z(float &graus_por_segundo) {
    if (barramento == nullptr) {
        return false;
    }
    float taxa;
    if (!ler_taxa_bruta(taxa)) {
        return false;
    }
    graus_por_segundo = taxa - offset_z;
    return true;
}
