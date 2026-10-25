#include "imu.h"

#include <Arduino.h>

// Registradores do MPU6050 (Register Map, rev. 4.2).
constexpr uint8_t REG_DIVISOR_TAXA = 0x19;  // SMPLRT_DIV
constexpr uint8_t REG_CONFIG = 0x1A;        // filtro passa-baixa (DLPF)
constexpr uint8_t REG_CONFIG_GIRO = 0x1B;   // GYRO_CONFIG: faixa
constexpr uint8_t REG_TAXA_Z_MSB = 0x47;    // GYRO_ZOUT_H, seguido do LSB em 0x48
constexpr uint8_t REG_ENERGIA_1 = 0x6B;     // PWR_MGMT_1
constexpr uint8_t REG_WHO_AM_I = 0x75;

constexpr uint8_t COMANDO_RESET = 0x80;
constexpr uint32_t TEMPO_RESET_MS = 100;
// Sai do modo sleep e usa o oscilador do giroscópio X como relógio, mais estável que o interno.
constexpr uint8_t RELOGIO_GIRO_X = 0x01;

// O MPU6050 original responde 0x68. Módulos com chips compatíveis (MPU6500 e
// variações) respondem 0x70, 0x71 ou 0x72 e têm os mesmos registradores do giroscópio.
constexpr uint8_t IDS_ACEITOS[] = {0x68, 0x70, 0x71, 0x72};

// Faixa de ±1000 graus/s: cobre as curvas rápidas com folga e dá 0,03 graus/s por LSB.
constexpr uint8_t FAIXA_1000_GRAUS = 0x10;
constexpr float LSB_POR_GRAU = 32.8f;

// Filtro de 42 Hz (DLPF_CFG = 3): o giroscópio amostra a 1 kHz, e o divisor 4
// leva a taxa a 1000 / (1 + 4) = 200 Hz. A tarefa de sensores lê a 50 Hz.
constexpr uint8_t FILTRO_42HZ = 0x03;
constexpr uint8_t DIVISOR_200HZ = 4;
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

static bool id_aceito(uint8_t id) {
    for (uint8_t aceito : IDS_ACEITOS) {
        if (id == aceito) {
            return true;
        }
    }
    return false;
}

// Taxa bruta do eixo Z, em graus/s, sem tirar o offset.
static bool ler_taxa_bruta(float &graus_por_segundo) {
    uint8_t dados[2];
    // Os dois bytes são lidos na mesma transação, então são da mesma amostra.
    if (!ler_registros(REG_TAXA_Z_MSB, dados, 2)) {
        return false;
    }
    int16_t bruto = (int16_t)(((uint16_t)dados[0] << 8) | dados[1]);
    graus_por_segundo = bruto / LSB_POR_GRAU;
    return true;
}

bool imu_iniciar(TwoWire &wire, uint8_t endereco) {
    barramento = &wire;
    endereco_imu = endereco;
    offset_z = 0.0f;

    uint8_t id = 0;
    if (!ler_registros(REG_WHO_AM_I, &id, 1) || !id_aceito(id)) {
        return false;
    }

    // O reset volta tudo ao padrão e deixa o chip em modo sleep.
    escrever_registro(REG_ENERGIA_1, COMANDO_RESET);
    delay(TEMPO_RESET_MS);

    if (!escrever_registro(REG_ENERGIA_1, RELOGIO_GIRO_X) ||
        !escrever_registro(REG_CONFIG, FILTRO_42HZ) ||
        !escrever_registro(REG_DIVISOR_TAXA, DIVISOR_200HZ) ||
        !escrever_registro(REG_CONFIG_GIRO, FAIXA_1000_GRAUS)) {
        return false;
    }

    // Confere a configuração.
    uint8_t faixa = 0;
    uint8_t filtro = 0;
    if (!ler_registros(REG_CONFIG_GIRO, &faixa, 1) || !ler_registros(REG_CONFIG, &filtro, 1)) {
        return false;
    }
    if ((faixa & 0x18) != FAIXA_1000_GRAUS || (filtro & 0x07) != FILTRO_42HZ) {
        return false;
    }

    // Espera o relógio e o filtro assentarem antes da primeira leitura válida.
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
