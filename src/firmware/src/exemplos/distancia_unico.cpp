// Teste de um único sensor de distância VL53L0X (módulo VL53L0X V2), sozinho no ESP32.
// Não usa o driver de distancia/ nem os XSHUT: serve para saber se o módulo funciona
// antes de montar os três na placa.
//
// Gravar:  pio run -e exemplo_distancia_unico -t upload && pio device monitor
//
// Ligação (módulo -> ESP32), toda no lado esquerdo da placa:
//   VIN -> 3V3    GND -> GND
//   SDA -> D32    SCL -> D25
//   XSHUT e GPIO1 do módulo ficam soltos (o módulo tem pull-up no XSHUT).
// No robô o I2C fica em GPIO21/22; aqui usa 32/25 só para ficar no mesmo lado.
//
// Conferir:
//   - o scan do I2C mostra 0x29 (endereço padrão do sensor). Sem nada no scan,
//     revisar fios e alimentação;
//   - com a mão ou uma parede a distâncias conhecidas, a leitura acompanha a
//     distância real (erro de alguns milímetros até uns 50 mm);
//   - sem alvo na frente (ou além de ~1,2 m), aparece "fora de alcance".
//
// Calibração por dois pontos (correção linear: real = ganho * medido + offset):
//   1. colocar uma parede branca a uma distância conhecida, perto (ex.: 50 mm),
//      medida da frente do sensor;
//   2. digitar a distância real em mm no monitor e Enter (ex.: "50");
//   3. repetir com a parede longe (ex.: "180", uma célula do labirinto);
//   4. anotar o ganho e o offset impressos, que serão usados no driver.

#include <Arduino.h>
#include <VL53L0X.h>
#include <Wire.h>

#include "config/pinos.h"

constexpr uint8_t I2C_SDA = 32;
constexpr uint8_t I2C_SCL = 25;
constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint16_t TIMEOUT_MS = 100;
constexpr uint32_t PERIODO_IMPRESSAO_MS = 200;
constexpr int AMOSTRAS_MEDIA = 50;
// Sem alvo, o VL53L0X devolve 8190 ou 8191.
constexpr uint16_t DISTANCIA_MAXIMA_MM = 8000;

static VL53L0X sensor;

// Pontos de calibração: distância real e média medida.
static float real_mm[2];
static float medido_mm[2];
static int pontos = 0;
static float ganho = 1.0f;
static float offset_mm = 0.0f;
static bool calibrado = false;

// Nível das linhas antes de iniciar o I2C. Com o módulo ligado e alimentado, os
// pull-ups dele deixam SDA e SCL em 1 mesmo sem o pull-up interno.
//   sem pull-up 0, com pull-up 1 -> módulo sem alimentação ou fio solto;
//   0 nos dois casos            -> linha em curto com o GND ou módulo travando.
static void verificar_linhas() {
    pinMode(I2C_SDA, INPUT);
    pinMode(I2C_SCL, INPUT);
    delay(5);
    int sda_solto = digitalRead(I2C_SDA);
    int scl_solto = digitalRead(I2C_SCL);
    pinMode(I2C_SDA, INPUT_PULLUP);
    pinMode(I2C_SCL, INPUT_PULLUP);
    delay(5);
    Serial.printf("Linhas sem pull-up interno: SDA=%d SCL=%d | com pull-up: SDA=%d SCL=%d\n",
                  sda_solto, scl_solto, digitalRead(I2C_SDA), digitalRead(I2C_SCL));
}

static void escanear_i2c() {
    Serial.print("Dispositivos no I2C:");
    for (uint8_t endereco = 1; endereco < 127; endereco++) {
        Wire.beginTransmission(endereco);
        if (Wire.endTransmission() == 0) {
            Serial.printf(" 0x%02X", endereco);
        }
    }
    Serial.println();
}

// Média de AMOSTRAS_MEDIA leituras válidas. Devolve -1 se nenhuma for válida.
static float medir_media() {
    float soma = 0;
    float soma_quadrados = 0;
    int validas = 0;
    for (int i = 0; i < AMOSTRAS_MEDIA; i++) {
        uint16_t d = sensor.readRangeContinuousMillimeters();
        if (!sensor.timeoutOccurred() && d < DISTANCIA_MAXIMA_MM) {
            soma += d;
            soma_quadrados += (float)d * d;
            validas++;
        }
    }
    if (validas == 0) {
        return -1;
    }
    float media = soma / validas;
    float desvio = sqrtf(fmaxf(soma_quadrados / validas - media * media, 0.0f));
    Serial.printf("média %.1f mm, desvio %.1f mm (%d leituras)\n", media, desvio, validas);
    return media;
}

static void adicionar_ponto(float real) {
    Serial.printf("Ponto %d: parede a %.0f mm, medindo... ", pontos + 1, real);
    float media = medir_media();
    if (media < 0) {
        Serial.println("sem leituras válidas, ponto descartado.");
        return;
    }
    real_mm[pontos] = real;
    medido_mm[pontos] = media;
    pontos++;

    if (pontos < 2) {
        Serial.println("Agora coloque a parede em outra distância e digite o valor.");
        return;
    }
    if (fabsf(medido_mm[1] - medido_mm[0]) < 10) {
        Serial.println("Os dois pontos estão muito próximos. Recomeçando a calibração.");
        pontos = 0;
        return;
    }
    ganho = (real_mm[1] - real_mm[0]) / (medido_mm[1] - medido_mm[0]);
    offset_mm = real_mm[0] - ganho * medido_mm[0];
    calibrado = true;
    pontos = 0;
    Serial.printf("Calibração: real = %.4f * medido + %.1f mm\n", ganho, offset_mm);
    Serial.println("Para calibrar de novo, digite outra distância.");
}

// Junta os caracteres recebidos até o Enter e trata a distância digitada.
static void ler_comando() {
    static char linha[8];
    static int tamanho = 0;
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (tamanho > 0) {
                linha[tamanho] = '\0';
                tamanho = 0;
                int real = atoi(linha);
                if (real > 0) {
                    adicionar_ponto(real);
                }
            }
        } else if (tamanho < (int)sizeof(linha) - 1) {
            linha[tamanho++] = c;
        }
    }
}

void setup() {
    // Mantém a ponte H desligada, caso o teste seja feito na placa do robô.
    pinMode(Pinos::MOTOR_ESQ_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_ESQ_PWM, LOW);
    pinMode(Pinos::MOTOR_DIR_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_DIR_PWM, LOW);

    Serial.begin(115200);
    delay(2000);

    verificar_linhas();
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQUENCIA_HZ);
    escanear_i2c();
    // Os NACKs do scan deixam o driver I2C do ESP-IDF 5 travado; reinicia o barramento.
    Wire.end();
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQUENCIA_HZ);

    sensor.setBus(&Wire);
    sensor.setTimeout(TIMEOUT_MS);
    while (!sensor.init()) {
        Serial.println("Sensor não respondeu. Conferir a ligação; nova tentativa em 1 s.");
        delay(1000);
    }
    sensor.startContinuous();
    Serial.println("Sensor iniciado. Digite a distância real em mm e Enter para calibrar.");
}

void loop() {
    ler_comando();

    uint16_t distancia = sensor.readRangeContinuousMillimeters();

    if (sensor.timeoutOccurred()) {
        Serial.println("timeout na leitura");
    } else if (distancia >= DISTANCIA_MAXIMA_MM) {
        Serial.println("fora de alcance");
    } else if (calibrado) {
        Serial.printf("medida: %4u mm   corrigida: %6.1f mm\n", distancia,
                      ganho * distancia + offset_mm);
    } else {
        Serial.printf("distância: %4u mm\n", distancia);
    }

    delay(PERIODO_IMPRESSAO_MS);
}
