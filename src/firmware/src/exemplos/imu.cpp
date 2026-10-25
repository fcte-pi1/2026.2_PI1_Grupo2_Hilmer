// Exemplo mínimo do driver da IMU MPU6050 (#236), para testar na placa.
//
// Gravar:  pio run -e exemplo_imu -t upload && pio device monitor
//
// Calibra o offset com o robô parado e depois integra a velocidade angular do
// eixo Z a 200 Hz, mostrando o ângulo a cada 200 ms.
// Conferir:
//   - o scan do I2C mostra o MPU6050 em 0x68;
//   - girar o robô 90 graus à mão, para a esquerda (anti-horário visto de cima),
//     mostra cerca de +90 graus; para a direita, cerca de -90 graus;
//   - parado, anotar a deriva do ângulo depois de 1 minuto.
// Pelo monitor serial: "z" zera o ângulo e o tempo, "c" recalibra (robô parado).

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "sensores/imu.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_INTEGRACAO_US = 5000;  // 200 Hz, a taxa do giroscópio
constexpr uint32_t PERIODO_IMPRESSAO_MS = 200;

static float angulo = 0.0f;
static uint32_t ultimo_us = 0;
static uint32_t inicio_ms = 0;
static uint32_t ultima_impressao_ms = 0;

// Mostra os endereços que respondem no barramento I2C.
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

static void calibrar() {
    Serial.println("Calibrando, mantenha o robô parado...");
    while (!imu_calibrar()) {
        Serial.println("O robô se mexeu, calibrando de novo...");
    }
    Serial.println("Calibração concluída");
    angulo = 0.0f;
    inicio_ms = millis();
    ultimo_us = micros();
}

void setup() {
    // Mantém a ponte H desligada.
    pinMode(Pinos::MOTOR_ESQ_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_ESQ_PWM, LOW);
    pinMode(Pinos::MOTOR_DIR_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_DIR_PWM, LOW);

    Serial.begin(115200);
    delay(2000);

    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    escanear_i2c();
    // Os NACKs do scan deixam o driver I2C do ESP-IDF 5 travado; reinicia o barramento.
    Wire.end();
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    while (!imu_iniciar(Wire)) {
        Serial.printf("MPU6050 não respondeu no endereço 0x%02X, tentando de novo\n",
                      IMU_ENDERECO_PADRAO);
        delay(1000);
    }
    Serial.println("MPU6050 iniciado");
    calibrar();
}

void loop() {
    if (Serial.available() > 0) {
        char comando = Serial.read();
        if (comando == 'z') {
            angulo = 0.0f;
            inicio_ms = millis();
            Serial.println("Ângulo zerado");
        } else if (comando == 'c') {
            calibrar();
        }
    }

    uint32_t agora_us = micros();
    if (agora_us - ultimo_us < PERIODO_INTEGRACAO_US) {
        return;
    }
    float dt = (agora_us - ultimo_us) * 1e-6f;
    ultimo_us = agora_us;

    float taxa;
    if (!imu_ler_giro_z(taxa)) {
        Serial.println("Falha na leitura do MPU6050");
        return;
    }
    angulo += taxa * dt;

    if (millis() - ultima_impressao_ms >= PERIODO_IMPRESSAO_MS) {
        ultima_impressao_ms = millis();
        Serial.printf("t = %6.1f s   giro_z: %8.2f graus/s   ângulo: %8.2f graus\n",
                      (millis() - inicio_ms) / 1000.0f, taxa, angulo);
    }
}
