// Exemplo de andar até a parede e virar 90° para o lado livre.
//
// Gravar:  pio run -e exemplo_parede_curva -t upload && pio device monitor
//
// Funciona sem a USB: o LED acende quando o robô está pronto, e o botão BOOT da placa
// (ou "s" pelo monitor serial) começa e para o teste.
//   1. Anda reto. O giroscópio mantém o rumo: se o robô desvia, o PID corrige
//      a velocidade das rodas.
//   2. Quando o sensor da frente mede menos que LIMIAR_FRENTE_MM, freia.
//   3. Vira 90° para o lado sem parede (esquerda primeiro). Com parede dos dois
//      lados, vira 180°.
//   4. Se a frente estiver livre depois da curva, volta ao passo 1. Se não,
//      escolhe outra curva.
// Conferir:
//   - o robô anda reto, sem puxar para um lado;
//   - para antes de bater na parede;
//   - a curva fica perto de 90°. Se passar ou faltar, ajustar COMPENSACAO_CURVA_GRAUS.
//
// O robô precisa ficar parado no chão enquanto o LED pisca (calibração do giroscópio).

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "navegacao/pid.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_MS = 10;

// Abaixo dessa distância, o sensor da frente indica parede e o robô freia.
constexpr uint16_t LIMIAR_FRENTE_MM = 200;
// Acima dessa distância (ou sem medida), o lado é considerado livre.
constexpr uint16_t LIMIAR_LATERAL_MM = 100;
// Depois da curva, a frente só conta como livre com essa folga a mais.
constexpr uint16_t FOLGA_FRENTE_MM = 30;

constexpr float VELOCIDADE_RETA = 0.30f;
constexpr float VELOCIDADE_CURVA_MAX = 0.40f;
// Abaixo disso o N20 não vence o atrito e o robô para no meio da curva.
constexpr float VELOCIDADE_CURVA_MIN = 0.30f;
// O robô continua girando um pouco depois do freio; a curva termina antes desse tanto.
constexpr float COMPENSACAO_CURVA_GRAUS = 5.0f;
constexpr uint32_t TEMPO_PARADO_MS = 500;
// Se a curva não terminar nesse tempo (roda travada), desiste e decide de novo.
constexpr uint32_t TEMPO_MAX_CURVA_MS = 3000;
// O MPU6050 está montado com o componente para baixo: o giro para a esquerda sai negativo.
constexpr bool IMU_INVERTIDA = true;

// Botão BOOT da própria placa (GPIO0, com pull-up na placa). Só influi no boot, então
// depois da inicialização serve de botão de partida, no lugar do SW2 (GPIO15).
constexpr uint8_t BOTAO_BOOT = 0;

static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, true};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, false};
static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

// Correção do rumo na reta: 0,02 de diferença de velocidade por grau de desvio.
static Pid pid_rumo(0.02f, 0.0f, 0.001f, PERIODO_MS / 1000.0f, 0.2f);

enum Estado {
    AGUARDANDO,
    ANDANDO,
    PARADO,
    VIRANDO,
};

static Estado estado = AGUARDANDO;
static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

// Ângulo de guinada integrado do giroscópio, em graus. Positivo é para a esquerda.
static float angulo = 0.0f;
static float rumo_alvo = 0.0f;
static float curva_alvo = 0.0f;  // ângulo em que a curva atual termina
static uint32_t inicio_curva = 0;
static uint32_t inicio_parado = 0;

static void atualizar_angulo() {
    static uint32_t ultimo_us = micros();
    uint32_t agora_us = micros();
    float dt = (agora_us - ultimo_us) / 1e6f;
    ultimo_us = agora_us;

    float giro_z;
    if (imu_ler_giro_z(giro_z)) {
        angulo += (IMU_INVERTIDA ? -giro_z : giro_z) * dt;
    }
}

// Ajusta o teto do duty pela tensão da bateria e bloqueia com a bateria no fim.
static bool bateria_ok() {
    float tensao, corrente;
    if (!energia_ler(tensao, corrente)) {
        return true;  // sem INA219, segue com o duty máximo padrão
    }
    motores_definir_duty_maximo(bateria_duty_maximo(MOTOR_TENSAO_NOMINAL, tensao));
    return bateria_estado(tensao) != BATERIA_BLOQUEADA;
}

static bool lado_livre(uint16_t distancia) {
    return distancia > LIMIAR_LATERAL_MM;  // DISTANCIA_INVALIDA também conta como livre
}

// Retorna true uma vez a cada aperto do botão BOOT ou "s" no monitor serial.
static bool comando_start() {
    static bool botao_anterior = false;
    static uint32_t ultimo_aperto = 0;
    bool botao = digitalRead(BOTAO_BOOT) == LOW;
    // Ignora o repique do contato: um aperto a cada 300 ms no máximo.
    bool apertou = botao && !botao_anterior && millis() - ultimo_aperto > 300;
    if (apertou) {
        ultimo_aperto = millis();
    }
    botao_anterior = botao;

    while (Serial.available() > 0) {
        if (Serial.read() == 's') {
            apertou = true;
        }
    }
    return apertou;
}

static void comecar_reta() {
    rumo_alvo = angulo;
    pid_rumo.zerar();
    estado = ANDANDO;
    Serial.printf("Andando. frente %u mm\n", distancias.frente);
}

static void comecar_curva(float graus) {
    curva_alvo = angulo + graus;
    inicio_curva = millis();
    estado = VIRANDO;
    Serial.printf("Virando %.0f graus\n", graus);
}

static void parar() {
    motores_frear();
    inicio_parado = millis();
    estado = PARADO;
}

// Com o robô parado, escolhe a próxima ação pelas paredes.
static void decidir() {
    if (distancias.frente > LIMIAR_FRENTE_MM + FOLGA_FRENTE_MM) {
        comecar_reta();
    } else if (lado_livre(distancias.esquerda)) {
        comecar_curva(90.0f);
    } else if (lado_livre(distancias.direita)) {
        comecar_curva(-90.0f);
    } else {
        comecar_curva(180.0f);
    }
}

static void andar() {
    if (distancias.frente < LIMIAR_FRENTE_MM) {
        Serial.printf("Parede a %u mm. esquerda %u mm, direita %u mm\n", distancias.frente,
                      distancias.esquerda, distancias.direita);
        parar();
        return;
    }
    // Correção positiva: o robô desviou para a direita e precisa virar para a esquerda.
    float correcao = pid_rumo.atualizar(rumo_alvo, angulo);
    motores_definir_velocidade(VELOCIDADE_RETA - correcao, VELOCIDADE_RETA + correcao);
}

static void virar() {
    float falta = curva_alvo - angulo;
    if (fabsf(falta) <= COMPENSACAO_CURVA_GRAUS) {
        Serial.printf("Curva concluída. ângulo %.1f\n", angulo);
        parar();
        return;
    }
    if (millis() - inicio_curva > TEMPO_MAX_CURVA_MS) {
        Serial.printf("Curva travou faltando %.1f graus. Decidindo de novo.\n", falta);
        parar();
        return;
    }
    // Desacelera perto do fim para não passar do ângulo.
    float velocidade = constrain(fabsf(falta) * 0.01f, VELOCIDADE_CURVA_MIN, VELOCIDADE_CURVA_MAX);
    if (falta > 0) {
        motores_definir_velocidade(-velocidade, velocidade);  // gira para a esquerda
    } else {
        motores_definir_velocidade(velocidade, -velocidade);  // gira para a direita
    }
}

void setup() {
    // A ponte H só liga depois do start.
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);

    pinMode(BOTAO_BOOT, INPUT_PULLUP);
    pinMode(Pinos::LED_STATUS, OUTPUT);

    Serial.begin(115200);
    delay(1000);

    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    if (!distancia_iniciar(Wire, PINOS_DISTANCIA)) {
        Serial.println("Aviso: algum sensor de distância não iniciou");
    }
    if (!energia_iniciar(Wire)) {
        Serial.println("Aviso: INA219 não respondeu. O duty máximo fica no padrão.");
    }
    if (!imu_iniciar(Wire)) {
        // Sem giroscópio o robô não anda reto nem sabe quando terminou a curva.
        Serial.println("Erro: MPU6050 não respondeu. Teste cancelado.");
        for (;;) {
            digitalWrite(Pinos::LED_STATUS, !digitalRead(Pinos::LED_STATUS));
            delay(100);
        }
    }

    // LED piscando: calibrando o giroscópio, não mexer no robô.
    Serial.println("Calibrando o giroscópio. Não mexa no robô.");
    digitalWrite(Pinos::LED_STATUS, HIGH);
    while (!imu_calibrar()) {
        Serial.println("O robô se mexeu. Calibrando de novo.");
        digitalWrite(Pinos::LED_STATUS, !digitalRead(Pinos::LED_STATUS));
    }

    digitalWrite(Pinos::LED_STATUS, HIGH);
    Serial.println("Pronto. Aperte o botão BOOT ou envie \"s\" para começar e para parar.");
}

void loop() {
    static uint32_t ultimo = millis();
    if (millis() - ultimo < PERIODO_MS) {
        return;
    }
    ultimo = millis();

    atualizar_angulo();
    distancia_ler(distancias);

    if (comando_start()) {
        if (estado == AGUARDANDO) {
            if (!bateria_ok()) {
                Serial.println("Bateria abaixo de 6,6 V. Recarregue antes de usar.");
                return;
            }
            motores_habilitar(true);
            decidir();
        } else {
            motores_frear();
            motores_habilitar(false);
            estado = AGUARDANDO;
            Serial.println("Parado pelo botão. Aperte de novo para recomeçar.");
        }
        return;
    }

    switch (estado) {
    case AGUARDANDO:
        break;
    case ANDANDO:
        andar();
        break;
    case PARADO:
        // Espera o robô assentar e os sensores medirem de novo antes de decidir.
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            if (!bateria_ok()) {
                motores_habilitar(false);
                estado = AGUARDANDO;
                Serial.println("Bateria abaixo de 6,6 V. Teste encerrado.");
            } else {
                decidir();
            }
        }
        break;
    case VIRANDO:
        virar();
        break;
    }
}
