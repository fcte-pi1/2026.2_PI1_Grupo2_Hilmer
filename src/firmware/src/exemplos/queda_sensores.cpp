// Confere se os sensores de distância caem quando os motores ligam.
//
// Gravar:  pio run -e exemplo_queda_sensores -t upload && pio device monitor
//
// Com o ROBÔ SUSPENSO (rodas sem tocar em nada), aperte BOOT ou envie "s". O teste:
//   1. 2 s parado;
//   2. 3 s com os dois motores para a frente, na velocidade da reta do explorar;
//   3. 2 s parado de novo.
// A cada 100 ms mostra a tensão e a corrente da bateria (INA219), as distâncias e
// quais sensores estão sem medida. Se os sensores caírem só com os motores girando,
// o problema é elétrico: queda na alimentação ou ruído dos motores no I2C/GND.
// No fim, um scan do I2C mostra quem ainda responde.

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t IDADE_MAXIMA_MS = 200;
constexpr float VELOCIDADE = 0.30f;  // VELOCIDADE_RETA do explorar
constexpr uint32_t PARADO_ANTES_MS = 2000;
constexpr uint32_t GIRANDO_MS = 3000;
constexpr uint32_t PARADO_DEPOIS_MS = 2000;
constexpr uint32_t PERIODO_LINHA_MS = 100;
constexpr uint8_t BOTAO_BOOT = 0;

static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};
static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, true};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, false};

static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, false};

static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

static void escanear() {
    Serial.print("Dispositivos no I2C:");
    for (uint8_t endereco = 1; endereco < 127; endereco++) {
        Wire.beginTransmission(endereco);
        if (Wire.endTransmission() == 0) {
            Serial.printf(" 0x%02X", endereco);
        }
    }
    Serial.println();
}

static const char *fase(uint32_t t) {
    if (t < PARADO_ANTES_MS) {
        return "parado ";
    }
    if (t < PARADO_ANTES_MS + GIRANDO_MS) {
        return "MOTORES";
    }
    return "parado ";
}

static void rodar_teste() {
    encoders_zerar();
    Serial.println("\n  t(s)  fase      bateria V   I (mA) | frente esquerda direita | enc esq  enc dir | sem medida");
    uint32_t inicio = millis();
    uint32_t ultima_linha = 0;
    bool motores_ligados = false;
    float tensao_minima = 99.0f;
    uint32_t primeira_falha = 0;
    while (true) {
        uint32_t t = millis() - inicio;
        if (t >= PARADO_ANTES_MS + GIRANDO_MS + PARADO_DEPOIS_MS) {
            break;
        }
        bool girar = t >= PARADO_ANTES_MS && t < PARADO_ANTES_MS + GIRANDO_MS;
        if (girar && !motores_ligados) {
            motores_habilitar(true);
            motores_definir_velocidade(VELOCIDADE, VELOCIDADE);
            motores_ligados = true;
        } else if (!girar && motores_ligados) {
            motores_frear();
            motores_habilitar(false);
            motores_ligados = false;
        }

        distancia_ler(distancias);
        uint8_t falhas = distancia_falhas(IDADE_MAXIMA_MS);
        float tensao = 0.0f, corrente = 0.0f;
        bool energia = energia_ler(tensao, corrente);
        if (energia && tensao < tensao_minima) {
            tensao_minima = tensao;
        }
        if (falhas != 0 && primeira_falha == 0) {
            primeira_falha = t;
            Serial.printf(">>> sensores caíram em t = %.2f s (%s)\n", t / 1000.0f, fase(t));
        }

        if (t - ultima_linha >= PERIODO_LINHA_MS) {
            ultima_linha = t;
            Serial.printf("%6.2f  %s   %s%6.2f %7.0f   | %6u %8u %7u | %7ld %8ld | %s%s%s\n",
                          t / 1000.0f, fase(t), energia ? "" : "?", tensao, corrente,
                          distancias.frente, distancias.esquerda, distancias.direita,
                          (long)encoders_contagem_esquerda(), (long)encoders_contagem_direita(),
                          (falhas & DISTANCIA_FALHA_FRENTE) ? "F " : "",
                          (falhas & DISTANCIA_FALHA_ESQUERDA) ? "E " : "",
                          (falhas & DISTANCIA_FALHA_DIREITA) ? "D" : "");
        }
        delay(10);
    }
    motores_frear();
    motores_habilitar(false);
    Serial.printf("Tensão mínima da bateria: %.2f V\n", tensao_minima);
    Serial.printf("Encoders em %.1f s de motor: esquerdo %ld, direito %ld contagens "
                  "(1 célula = ~1407).\n", GIRANDO_MS / 1000.0f,
                  (long)encoders_contagem_esquerda(), (long)encoders_contagem_direita());
    if (primeira_falha == 0) {
        Serial.println("Resultado: nenhum sensor caiu.");
    } else {
        Serial.printf("Resultado: sensores caíram em t = %.2f s.\n", primeira_falha / 1000.0f);
    }
    escanear();
    Serial.println("Para repetir, reinicie a placa (os sensores precisam iniciar de novo).");
}

void setup() {
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);
    pinMode(BOTAO_BOOT, INPUT_PULLUP);
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Teste: sensores x motores (robô suspenso) ===");
    Serial.printf("Encoders: %s\n",
                  encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO) ? "ok" : "falha no PCNT");
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    bool ok = distancia_iniciar(Wire, PINOS_DISTANCIA);
    Serial.printf("Sensores de distância: %s\n", ok ? "os três iniciaram" : "algum falhou");
    Serial.printf("INA219: %s\n", energia_iniciar(Wire) ? "ok" : "não respondeu");
    escanear();
    Serial.println("Suspenda o robô e aperte BOOT ou envie \"s\".");
}

void loop() {
    bool start = digitalRead(BOTAO_BOOT) == LOW;
    while (Serial.available() > 0) {
        if (Serial.read() == 's') {
            start = true;
        }
    }
    distancia_ler(distancias);
    if (start) {
        rodar_teste();
        while (true) {
            delay(1000);
        }
    }
    delay(10);
}
