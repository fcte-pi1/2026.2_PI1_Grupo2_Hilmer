#include "energia.h"

// Registradores do INA219 (datasheet, seção 8.6). Todos têm 16 bits, MSB primeiro.
constexpr uint8_t REG_CONFIG = 0x00;
constexpr uint8_t REG_TENSAO_SHUNT = 0x01;
constexpr uint8_t REG_TENSAO_BARRAMENTO = 0x02;

// Configuração:
//   BRNG = 0      faixa de 16 V no barramento (a bateria vai até 8,4 V)
//   PG = 11       ganho /8, até 320 mV no shunt = 3,2 A com 0,1 ohm
//   BADC = 1011   12 bits com média de 8 amostras (4,26 ms)
//   SADC = 1011   12 bits com média de 8 amostras (4,26 ms)
//   MODE = 111    shunt e barramento em modo contínuo
// Uma leitura nova a cada ~8,5 ms, mais rápido que a tarefa de sensores (20 ms).
constexpr uint16_t CONFIG = (0b0 << 13) | (0b11 << 11) | (0b1011 << 7) | (0b1011 << 3) | 0b111;

constexpr float LSB_TENSAO_SHUNT = 10e-6f;      // 10 uV
constexpr float LSB_TENSAO_BARRAMENTO = 4e-3f;  // 4 mV

static TwoWire *barramento = nullptr;
static uint8_t endereco_ina = ENERGIA_ENDERECO_PADRAO;

static bool escrever_registro(uint8_t registro, uint16_t valor) {
    barramento->beginTransmission(endereco_ina);
    barramento->write(registro);
    barramento->write((uint8_t)(valor >> 8));
    barramento->write((uint8_t)(valor & 0xFF));
    return barramento->endTransmission() == 0;
}

static bool ler_registro(uint8_t registro, uint16_t &valor) {
    barramento->beginTransmission(endereco_ina);
    barramento->write(registro);
    if (barramento->endTransmission() != 0) {
        return false;
    }
    if (barramento->requestFrom(endereco_ina, (uint8_t)2) != 2) {
        return false;
    }
    uint8_t msb = barramento->read();
    uint8_t lsb = barramento->read();
    valor = ((uint16_t)msb << 8) | lsb;
    return true;
}

bool energia_iniciar(TwoWire &wire, uint8_t endereco) {
    barramento = &wire;
    endereco_ina = endereco;

    if (!escrever_registro(REG_CONFIG, CONFIG)) {
        return false;
    }
    uint16_t lido = 0;
    return ler_registro(REG_CONFIG, lido) && lido == CONFIG;
}

bool energia_ler(float &tensao, float &corrente) {
    if (barramento == nullptr) {
        return false;
    }

    uint16_t bruto_shunt = 0;
    uint16_t bruto_barramento = 0;
    if (!ler_registro(REG_TENSAO_SHUNT, bruto_shunt) ||
        !ler_registro(REG_TENSAO_BARRAMENTO, bruto_barramento)) {
        return false;
    }

    // Tensão no shunt: complemento de dois, já com o sinal.
    float tensao_shunt = (int16_t)bruto_shunt * LSB_TENSAO_SHUNT;
    // Tensão do barramento: bits 15 a 3 (os bits 1 e 0 são flags).
    float tensao_barramento = (bruto_barramento >> 3) * LSB_TENSAO_BARRAMENTO;

    // Em high-side, o barramento é medido depois do shunt (VIN-). A tensão da
    // bateria é a do barramento mais a queda no shunt (até 0,3 V em 3 A).
    tensao = tensao_barramento + tensao_shunt;
    // Corrente pela lei de Ohm, sem usar o registrador de calibração do INA219.
    corrente = tensao_shunt / ENERGIA_SHUNT_OHMS;
    return true;
}
