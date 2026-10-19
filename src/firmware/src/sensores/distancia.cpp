#include "distancia.h"

#include <Arduino.h>
#include <VL53L0X.h>

constexpr uint8_t ENDERECO_FABRICA = 0x29;

// O sensor leva até 1,2 ms para ligar depois do XSHUT; 10 ms dá folga.
constexpr uint32_t TEMPO_BOOT_MS = 10;
constexpr uint16_t TIMEOUT_MS = 500;

// Tempo de cada medida. 20 ms é o mínimo da biblioteca e acompanha a tarefa de
// sensores (20 ms). Com a parede a até 30 cm, a precisão continua milimétrica.
constexpr uint32_t ORCAMENTO_MEDIDA_US = 20000;

// Sem alvo, o VL53L0X devolve 8190 ou 8191.
constexpr uint16_t LEITURA_SEM_ALVO = 8190;

struct Sensor {
    VL53L0X vl53;
    bool ativo = false;
};

static Sensor sensor_frente_dir;  // U7
static Sensor sensor_frente_esq;  // U8
static Sensor sensor_esquerda;    // U9
static Sensor sensor_direita;     // U10

static bool responde(TwoWire &wire, uint8_t endereco) {
    wire.beginTransmission(endereco);
    return wire.endTransmission() == 0;
}

// Com o sensor em ENDERECO_FABRICA: muda o endereço, inicia e começa a medir.
static bool iniciar_sensor(Sensor &sensor, TwoWire &wire, uint8_t endereco) {
    sensor.vl53.setBus(&wire);
    sensor.vl53.setTimeout(TIMEOUT_MS);
    // O endereço muda antes do init, para que um sensor que falhe no init não
    // fique em 0x29 e colida com o próximo a ser ligado.
    sensor.vl53.setAddress(endereco);
    if (sensor.vl53.last_status != 0 || !sensor.vl53.init()) {
        return false;
    }
    if (!sensor.vl53.setMeasurementTimingBudget(ORCAMENTO_MEDIDA_US)) {
        return false;
    }
    sensor.vl53.startContinuous();
    sensor.ativo = sensor.vl53.last_status == 0;
    return sensor.ativo;
}

// O XSHUT tem pull-up para 2,8 V no módulo. Para ligar o sensor, o pino vira
// entrada e o pull-up assume, sem o ESP32 forçar 3,3 V na linha.
static void desligar(uint8_t xshut) {
    pinMode(xshut, OUTPUT);
    digitalWrite(xshut, LOW);
}

static void ligar(uint8_t xshut) {
    pinMode(xshut, INPUT);
    delay(TEMPO_BOOT_MS);
}

static bool ligar_e_iniciar(Sensor &sensor, TwoWire &wire, uint8_t xshut, uint8_t endereco) {
    ligar(xshut);
    if (iniciar_sensor(sensor, wire, endereco)) {
        return true;
    }
    // Mantém o sensor com defeito desligado, para não ocupar o 0x29.
    desligar(xshut);
    return false;
}

bool distancia_iniciar(TwoWire &wire, const PinosDistancia &pinos) {
    // Desliga U7, U8 e U9, que voltam ao endereço de fábrica.
    desligar(pinos.xshut_frente_dir);
    desligar(pinos.xshut_frente_esq);
    desligar(pinos.xshut_esquerda);
    delay(TEMPO_BOOT_MS);

    // O U10 não tem XSHUT. Se o ESP32 reiniciou sem desligar a alimentação, ele
    // continua no endereço da inicialização anterior, medindo em modo contínuo.
    // Nesse caso, para a medida e devolve o sensor ao endereço de fábrica.
    if (!responde(wire, ENDERECO_FABRICA) && responde(wire, DISTANCIA_ENDERECO_DIREITA)) {
        wire.beginTransmission(DISTANCIA_ENDERECO_DIREITA);
        wire.write(VL53L0X::SYSRANGE_START);
        wire.write(0x01);  // em modo contínuo, para a medida
        wire.endTransmission();
        delay(ORCAMENTO_MEDIDA_US / 1000 * 2);

        wire.beginTransmission(DISTANCIA_ENDERECO_DIREITA);
        wire.write(VL53L0X::SYSTEM_INTERRUPT_CLEAR);
        wire.write(0x01);
        wire.endTransmission();

        wire.beginTransmission(DISTANCIA_ENDERECO_DIREITA);
        wire.write(VL53L0X::I2C_SLAVE_DEVICE_ADDRESS);
        wire.write(ENDERECO_FABRICA);
        wire.endTransmission();
    }

    bool ok = iniciar_sensor(sensor_direita, wire, DISTANCIA_ENDERECO_DIREITA);
    ok &= ligar_e_iniciar(sensor_frente_dir, wire, pinos.xshut_frente_dir,
                          DISTANCIA_ENDERECO_FRENTE_DIR);
    ok &= ligar_e_iniciar(sensor_frente_esq, wire, pinos.xshut_frente_esq,
                          DISTANCIA_ENDERECO_FRENTE_ESQ);
    ok &= ligar_e_iniciar(sensor_esquerda, wire, pinos.xshut_esquerda,
                          DISTANCIA_ENDERECO_ESQUERDA);
    return ok;
}

// Mesma leitura de readRangeContinuousMillimeters() da biblioteca, mas sem
// esperar: se a medida ainda não terminou, retorna false na hora.
static bool ler_sensor(Sensor &sensor, uint16_t &distancia) {
    if (!sensor.ativo) {
        distancia = DISTANCIA_INVALIDA;
        return false;
    }

    uint8_t status = sensor.vl53.readReg(VL53L0X::RESULT_INTERRUPT_STATUS);
    if (sensor.vl53.last_status != 0 || (status & 0x07) == 0) {
        return false;
    }

    uint16_t mm = sensor.vl53.readReg16Bit(VL53L0X::RESULT_RANGE_STATUS + 10);
    bool leitura_ok = sensor.vl53.last_status == 0;
    sensor.vl53.writeReg(VL53L0X::SYSTEM_INTERRUPT_CLEAR, 0x01);
    if (!leitura_ok) {
        return false;
    }

    distancia = mm >= LEITURA_SEM_ALVO ? DISTANCIA_INVALIDA : mm;
    return true;
}

void distancia_ler(Distancias &distancias) {
    ler_sensor(sensor_frente_dir, distancias.frente_dir);
    ler_sensor(sensor_frente_esq, distancias.frente_esq);
    ler_sensor(sensor_esquerda, distancias.esquerda);
    ler_sensor(sensor_direita, distancias.direita);
}
