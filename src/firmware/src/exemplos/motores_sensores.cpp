// Exemplo dos motores comandados com dados dos sensores (testes 3.8 e 3.17 do doc 7.3).
//
// Gravar:  pio run -e exemplo_motores_sensores -t upload && pio device monitor
//
// Usa os motores, os encoders, o INA219 e os sensores de distância ao mesmo tempo.
// Com os motores soltos na bancada (ou o robô suspenso), enviar pelo monitor serial:
//   "c": curva duty x rpm. Os dois motores giram em quatro velocidades e, para cada
//        uma, aparecem o duty aplicado, a rotação de cada roda (pelos encoders) e a
//        tensão e a corrente da bateria (pelo INA219);
//   "p": parada por obstáculo. Os motores giram para a frente e freiam quando o
//        sensor da frente mede menos que LIMIAR_PAREDE_MM. Voltam a girar
//        quando o obstáculo sai. Qualquer tecla encerra.
// Conferir:
//   - a rotação cresce com o duty;
//   - o duty máximo segue 6,0 V / tensão da bateria (cerca de 0,71 com 8,4 V);
//   - a corrente do INA219 bate com o multímetro em série com a bateria, parado e
//     com os motores girando (teste 3.8);
//   - a roda freia ao aproximar a mão do sensor da frente.
//
// Antes de usar, trocar CONTAGENS_POR_VOLTA pelo valor medido no teste 3.10
// (exemplo_encoders) e LIMIAR_PAREDE_MM pelo valor definido no teste 3.4.

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;

// Contagens do encoder por volta da roda (quadratura). Valor provisório: medir no teste 3.10.
constexpr float CONTAGENS_POR_VOLTA = 1400.0f;
// Abaixo dessa distância, o sensor da frente indica parede. Valor provisório: definir no teste 3.4.
constexpr uint16_t LIMIAR_PAREDE_MM = 100;
// Folga para voltar a girar, para o motor não ficar ligando e desligando no limiar.
constexpr uint16_t FOLGA_LIMIAR_MM = 20;

constexpr float VELOCIDADES_CURVA[] = {0.25f, 0.5f, 0.75f, 1.0f};
constexpr uint32_t TEMPO_ACOMODAR_MS = 1000;  // espera a rotação estabilizar
constexpr uint32_t TEMPO_MEDIDA_MS = 2000;
constexpr float VELOCIDADE_PARADA = 0.3f;
constexpr uint32_t PERIODO_IMPRESSAO_MS = 250;

static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, false};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, true};
static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, true};
static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

static float duty_maximo = MOTOR_TENSAO_NOMINAL / MOTOR_TENSAO_BATERIA_CHEIA;

// Lê o INA219 e ajusta o teto do duty pela tensão atual da bateria (doc 4.3, seção 6.3).
static bool ler_bateria(float &tensao, float &corrente) {
    if (!energia_ler(tensao, corrente)) {
        return false;
    }
    duty_maximo = bateria_duty_maximo(MOTOR_TENSAO_NOMINAL, tensao);
    motores_definir_duty_maximo(duty_maximo);
    return true;
}

static void imprimir_bateria() {
    float tensao, corrente;
    if (ler_bateria(tensao, corrente)) {
        Serial.printf("tensão %.3f V   corrente %.1f mA   duty máximo %.2f", tensao,
                      1000.0f * corrente, duty_maximo);
    } else {
        Serial.print("falha na leitura do INA219");
    }
}

static void curva_duty_rpm() {
    Serial.print("Parado: ");
    imprimir_bateria();
    Serial.println();

    motores_habilitar(true);
    for (float velocidade : VELOCIDADES_CURVA) {
        motores_definir_velocidade(velocidade, velocidade);
        delay(TEMPO_ACOMODAR_MS);

        int32_t esquerda_inicio = encoders_contagem_esquerda();
        int32_t direita_inicio = encoders_contagem_direita();
        delay(TEMPO_MEDIDA_MS);
        int32_t esquerda = encoders_contagem_esquerda() - esquerda_inicio;
        int32_t direita = encoders_contagem_direita() - direita_inicio;

        float minutos = TEMPO_MEDIDA_MS / 60000.0f;
        float rpm_esquerda = esquerda / CONTAGENS_POR_VOLTA / minutos;
        float rpm_direita = direita / CONTAGENS_POR_VOLTA / minutos;

        Serial.printf("duty %.2f   esquerda %6.0f cont/s %6.1f rpm   direita %6.0f cont/s %6.1f rpm   ",
                      velocidade * duty_maximo, esquerda * 1000.0f / TEMPO_MEDIDA_MS, rpm_esquerda,
                      direita * 1000.0f / TEMPO_MEDIDA_MS, rpm_direita);
        imprimir_bateria();
        Serial.println();
    }
    motores_frear();
    Serial.println("Curva concluída");
}

// Retorna true se chegou alguma tecla, ignorando o Enter (\r e \n).
static bool tecla_pressionada() {
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c != '\r' && c != '\n') {
            return true;
        }
    }
    return false;
}

static bool tem_obstaculo(const Distancias &d, uint16_t limiar) {
    return d.frente < limiar;
}

static void parada_por_obstaculo() {
    Serial.println("Motores girando. Aproxime a mão do sensor da frente. Qualquer tecla encerra.");
    Distancias d = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};
    bool parado = false;
    uint32_t ultima_impressao = 0;

    motores_habilitar(true);
    motores_definir_velocidade(VELOCIDADE_PARADA, VELOCIDADE_PARADA);

    while (!tecla_pressionada()) {
        distancia_ler(d);

        if (!parado && tem_obstaculo(d, LIMIAR_PAREDE_MM)) {
            motores_frear();
            parado = true;
            Serial.printf(">>> obstáculo: frente %u mm. Freando.\n", d.frente);
        } else if (parado && !tem_obstaculo(d, LIMIAR_PAREDE_MM + FOLGA_LIMIAR_MM)) {
            motores_definir_velocidade(VELOCIDADE_PARADA, VELOCIDADE_PARADA);
            parado = false;
            Serial.println(">>> caminho livre. Girando de novo.");
        }

        if (millis() - ultima_impressao >= PERIODO_IMPRESSAO_MS) {
            ultima_impressao = millis();
            Serial.printf("frente %5u mm   %s   ", d.frente, parado ? "PARADO " : "GIRANDO");
            imprimir_bateria();
            Serial.println();
        }
        delay(10);
    }

    motores_frear();
    Serial.println("Teste de parada encerrado");
}

void setup() {
    // A ponte H só liga dentro dos testes.
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);

    Serial.begin(115200);
    delay(2000);

    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    if (!energia_iniciar(Wire)) {
        Serial.println("Aviso: INA219 não respondeu. O duty máximo fica no padrão.");
    }
    if (!distancia_iniciar(Wire, PINOS_DISTANCIA)) {
        Serial.println("Aviso: algum sensor de distância não iniciou");
    }
    if (!encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO)) {
        Serial.println("Erro: falha ao configurar o PCNT dos encoders");
    }

    Serial.println("Envie \"c\" para a curva duty x rpm ou \"p\" para a parada por obstáculo.");
}

void loop() {
    if (Serial.available() == 0) {
        return;
    }
    char comando = Serial.read();
    if (comando == 'c') {
        curva_duty_rpm();
    } else if (comando == 'p') {
        parada_por_obstaculo();
    }
}
