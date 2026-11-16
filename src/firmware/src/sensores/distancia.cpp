#include "distancia.h"

#include <Arduino.h>
#include <VL53L0X.h>

constexpr uint8_t ENDERECO_FABRICA = 0x29;
// Endereço sem uso para tirar do 0x29 um U10 que não iniciou.
constexpr uint8_t ENDERECO_ESTACIONADO = 0x33;
constexpr uint8_t ID_MODELO_VL53L0X = 0xEE;
// Onde o U10 pode ter ficado depois de um boot anterior (bom ou com falha).
constexpr uint8_t ENDERECOS_POSSIVEIS_DIREITA[] = {
    DISTANCIA_ENDERECO_DIREITA, DISTANCIA_ENDERECO_FRENTE, DISTANCIA_ENDERECO_ESQUERDA,
    ENDERECO_ESTACIONADO};

// O sensor leva até 1,2 ms para ligar depois do XSHUT; 10 ms dá folga.
constexpr uint32_t TEMPO_BOOT_MS = 10;
constexpr uint32_t TEMPO_MAX_LIGAR_MS = 100;
constexpr uint16_t TIMEOUT_MS = 500;

// Tempo de cada medida. 20 ms é o mínimo da biblioteca e acompanha a tarefa de
// sensores (20 ms). Com a parede a até 30 cm, a precisão continua milimétrica.
constexpr uint32_t ORCAMENTO_MEDIDA_US = 20000;

// Sem alvo, o VL53L0X devolve 8190 ou 8191.
constexpr uint16_t LEITURA_SEM_ALVO = 8190;

struct Sensor {
    VL53L0X vl53;
    bool ativo = false;
    uint32_t ultima_medida_ms = 0;
};

static Sensor sensor_frente;    // U7
static Sensor sensor_esquerda;  // U9
static Sensor sensor_direita;   // U10

static bool responde(TwoWire &wire, uint8_t endereco) {
    wire.beginTransmission(endereco);
    return wire.endTransmission() == 0;
}

static void escrever(TwoWire &wire, uint8_t endereco, uint8_t registrador, uint8_t valor) {
    wire.beginTransmission(endereco);
    wire.write(registrador);
    wire.write(valor);
    wire.endTransmission();
}

// Com o sensor em ENDERECO_FABRICA: muda o endereço, inicia e começa a medir.
static void falhou(uint8_t endereco, const char *etapa) {
    Serial.printf("VL53L0X 0x%02X: falhou em %s\n", endereco, etapa);
}

static bool iniciar_sensor(Sensor &sensor, TwoWire &wire, uint8_t endereco) {
    sensor.ativo = false;  // numa nova tentativa, só volta a valer se iniciar de novo
    // Objeto novo a cada tentativa: o da biblioteca guarda o endereço da tentativa
    // anterior (0x30, por exemplo), e o setAddress abaixo escreveria nele em vez de
    // escrever no 0x29, onde o sensor está. Toda nova tentativa falhava por isso.
    sensor.vl53 = VL53L0X();
    sensor.vl53.setBus(&wire);
    sensor.vl53.setTimeout(TIMEOUT_MS);
    // O endereço muda antes do init, para que um sensor que falhe no init não
    // fique em 0x29 e colida com o próximo a ser ligado.
    sensor.vl53.setAddress(endereco);
    if (sensor.vl53.last_status != 0) {
        falhou(endereco, "trocar o endereço");
        return false;
    }
    if (!sensor.vl53.init()) {
        falhou(endereco, "init");
        return false;
    }
    // O init() da biblioteca não confere todas as transações. Com o I2C sem resposta,
    // ele termina com valores zerados, e o setMeasurementTimingBudget divide por zero.
    if (sensor.vl53.last_status != 0) {
        falhou(endereco, "init (I2C sem resposta)");
        return false;
    }
    if (!sensor.vl53.setMeasurementTimingBudget(ORCAMENTO_MEDIDA_US)) {
        falhou(endereco, "tempo de medida");
        return false;
    }
    sensor.vl53.startContinuous();
    sensor.ativo = sensor.vl53.last_status == 0;
    if (!sensor.ativo) {
        falhou(endereco, "começar a medir");
    }
    return sensor.ativo;
}

// O U10 já está no 0x30 medindo em modo contínuo, de um boot anterior do ESP32 (reset
// sem cortar a alimentação). Iniciar de novo um VL53L0X nesse estado falha com
// frequência, e sem XSHUT não há como desligá-lo: ele ficava preso e derrubava os outros
// dois no 0x29. Como a configuração é a mesma deste firmware, basta voltar a ler as
// medidas dele. Retorna true se ele entregou uma medida nova.
static bool reaproveitar_direita(TwoWire &wire) {
    Sensor &sensor = sensor_direita;
    sensor.ativo = false;
    sensor.vl53 = VL53L0X();  // começa no 0x29, como o setAddress abaixo espera
    sensor.vl53.setBus(&wire);
    sensor.vl53.setTimeout(TIMEOUT_MS);
    // O setAddress escreve no endereço antigo do objeto (0x29), que está livre com U7 e
    // U9 desligados: a escrita não chega a ninguém e o objeto passa a falar com o 0x30.
    sensor.vl53.setAddress(DISTANCIA_ENDERECO_DIREITA);
    uint8_t id = sensor.vl53.readReg(VL53L0X::IDENTIFICATION_MODEL_ID);
    if (sensor.vl53.last_status != 0 || id != ID_MODELO_VL53L0X) {
        return false;
    }
    uint32_t inicio = millis();
    while (millis() - inicio < 3 * ORCAMENTO_MEDIDA_US / 1000) {
        uint8_t status = sensor.vl53.readReg(VL53L0X::RESULT_INTERRUPT_STATUS);
        if (sensor.vl53.last_status == 0 && (status & 0x07) != 0) {
            sensor.ativo = true;
            sensor.ultima_medida_ms = millis();
            return true;
        }
        delay(2);
    }
    return false;
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
    // Com o XSHUT solto, quem sobe a linha é o pull-up do módulo; em alguns boots o
    // sensor demorou mais que TEMPO_BOOT_MS para aparecer no 0x29 e a troca de endereço
    // falhava. Espera ele responder (até TEMPO_MAX_LIGAR_MS) antes de tentar.
    uint32_t inicio = millis();
    while (!responde(wire, ENDERECO_FABRICA) && millis() - inicio < TEMPO_MAX_LIGAR_MS) {
        delay(2);
    }
    if (iniciar_sensor(sensor, wire, endereco)) {
        return true;
    }
    // Mantém o sensor com defeito desligado, para não ocupar o 0x29.
    desligar(xshut);
    return false;
}

bool distancia_iniciar(TwoWire &wire, const PinosDistancia &pinos) {
    // Desliga U7 e U9, que voltam ao endereço de fábrica.
    desligar(pinos.xshut_frente);
    desligar(pinos.xshut_esquerda);
    delay(TEMPO_BOOT_MS);

    bool ok;
    if (!responde(wire, ENDERECO_FABRICA) && responde(wire, DISTANCIA_ENDERECO_DIREITA) &&
        reaproveitar_direita(wire)) {
        ok = true;
    } else {
        // O U10 não tem XSHUT: se o ESP32 reiniciou sem desligar a alimentação, ele continua
        // medindo no endereço que tinha. Nem sempre é o 0x30: um boot que falhou no meio pode
        // ter deixado ele no 0x31, no 0x32 ou no 0x33 (ex.: a esquerda ligou no 0x29 junto com
        // ele e os dois receberam o 0x32). Com U7 e U9 desligados, qualquer VL53L0X que
        // responda só pode ser o U10: ele para a medida e volta ao endereço de fábrica.
        if (!responde(wire, ENDERECO_FABRICA)) {
            for (uint8_t endereco : ENDERECOS_POSSIVEIS_DIREITA) {
                if (responde(wire, endereco)) {
                    escrever(wire, endereco, VL53L0X::SYSRANGE_START, 0x01);  // para a medida
                    delay(ORCAMENTO_MEDIDA_US / 1000 * 2);
                    escrever(wire, endereco, VL53L0X::SYSTEM_INTERRUPT_CLEAR, 0x01);
                    escrever(wire, endereco, VL53L0X::I2C_SLAVE_DEVICE_ADDRESS, ENDERECO_FABRICA);
                    delay(TEMPO_BOOT_MS);
                    break;
                }
            }
        }
        ok = iniciar_sensor(sensor_direita, wire, DISTANCIA_ENDERECO_DIREITA);
        if (!ok && responde(wire, ENDERECO_FABRICA)) {
            // O U10 falhou e continua no 0x29. Como não dá para desligá-lo, ele vai para um
            // endereço sem uso; senão a frente e a esquerda ligariam no 0x29 junto com ele.
            escrever(wire, ENDERECO_FABRICA, VL53L0X::I2C_SLAVE_DEVICE_ADDRESS, ENDERECO_ESTACIONADO);
            delay(TEMPO_BOOT_MS);
        }
    }
    ok &= ligar_e_iniciar(sensor_frente, wire, pinos.xshut_frente, DISTANCIA_ENDERECO_FRENTE);
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
    sensor.ultima_medida_ms = millis();
    return true;
}

void distancia_ler(Distancias &distancias) {
    ler_sensor(sensor_frente, distancias.frente);
    ler_sensor(sensor_esquerda, distancias.esquerda);
    ler_sensor(sensor_direita, distancias.direita);
}

static bool sensor_respondendo(const Sensor &sensor, uint32_t agora, uint32_t idade_maxima_ms) {
    return sensor.ativo && agora - sensor.ultima_medida_ms <= idade_maxima_ms;
}

uint8_t distancia_falhas(uint32_t idade_maxima_ms) {
    uint32_t agora = millis();
    uint8_t falhas = 0;
    if (!sensor_respondendo(sensor_frente, agora, idade_maxima_ms)) {
        falhas |= DISTANCIA_FALHA_FRENTE;
    }
    if (!sensor_respondendo(sensor_esquerda, agora, idade_maxima_ms)) {
        falhas |= DISTANCIA_FALHA_ESQUERDA;
    }
    if (!sensor_respondendo(sensor_direita, agora, idade_maxima_ms)) {
        falhas |= DISTANCIA_FALHA_DIREITA;
    }
    return falhas;
}

bool distancia_respondendo(uint32_t idade_maxima_ms) {
    return distancia_falhas(idade_maxima_ms) == 0;
}

// Nova tentativa só para os sensores em "falhas" (bits DISTANCIA_FALHA_*). Os que estão
// medindo não são tocados: desligá-los pelo XSHUT os devolveria ao 0x29, onde um U10 com
// defeito pode estar preso, e aí os três caíam juntos.
uint8_t distancia_reiniciar(TwoWire &wire, const PinosDistancia &pinos, uint8_t falhas) {
    if (falhas & DISTANCIA_FALHA_DIREITA) {
        sensor_direita.ativo = false;
        if (responde(wire, DISTANCIA_ENDERECO_DIREITA) && reaproveitar_direita(wire)) {
            falhas &= ~DISTANCIA_FALHA_DIREITA;
        } else {
            if (!responde(wire, ENDERECO_FABRICA) && responde(wire, ENDERECO_ESTACIONADO)) {
                escrever(wire, ENDERECO_ESTACIONADO, VL53L0X::I2C_SLAVE_DEVICE_ADDRESS,
                         ENDERECO_FABRICA);
                delay(TEMPO_BOOT_MS);
            }
            if (responde(wire, ENDERECO_FABRICA)) {
                if (iniciar_sensor(sensor_direita, wire, DISTANCIA_ENDERECO_DIREITA)) {
                    falhas &= ~DISTANCIA_FALHA_DIREITA;
                } else if (responde(wire, ENDERECO_FABRICA)) {
                    escrever(wire, ENDERECO_FABRICA, VL53L0X::I2C_SLAVE_DEVICE_ADDRESS,
                             ENDERECO_ESTACIONADO);
                    delay(TEMPO_BOOT_MS);
                }
            }
        }
    }
    // A frente e a esquerda só ligam com o 0x29 livre, para não colidir com o U10.
    if ((falhas & DISTANCIA_FALHA_FRENTE) && !responde(wire, ENDERECO_FABRICA)) {
        desligar(pinos.xshut_frente);
        delay(TEMPO_BOOT_MS);
        if (ligar_e_iniciar(sensor_frente, wire, pinos.xshut_frente, DISTANCIA_ENDERECO_FRENTE)) {
            falhas &= ~DISTANCIA_FALHA_FRENTE;
        }
    }
    if ((falhas & DISTANCIA_FALHA_ESQUERDA) && !responde(wire, ENDERECO_FABRICA)) {
        desligar(pinos.xshut_esquerda);
        delay(TEMPO_BOOT_MS);
        if (ligar_e_iniciar(sensor_esquerda, wire, pinos.xshut_esquerda,
                            DISTANCIA_ENDERECO_ESQUERDA)) {
            falhas &= ~DISTANCIA_FALHA_ESQUERDA;
        }
    }
    return falhas;
}
