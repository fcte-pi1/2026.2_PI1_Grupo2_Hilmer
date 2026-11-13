// Exemplo de andar célula por célula e parar no centro de cada uma.
//
// Gravar:  pio run -e exemplo_celula -t upload && pio device monitor
//
// É o passo que falta para o Flood Fill: o robô precisa parar no centro de cada
// célula para ler as paredes, mesmo num corredor sem parede na frente.
//
// Funciona sem a USB: o LED acende quando o robô está pronto, e o botão BOOT da placa
// (ou "s" pelo monitor serial) começa e para o teste.
//   1. Anda reto com o mesmo controle do exemplo_parede_curva (giroscópio no rumo e
//      paredes laterais para centralizar).
//   2. Para quando os encoders contam CONTAGENS_POR_CELULA, ou antes, se o sensor da
//      frente vê a parede (assim o erro do encoder não se acumula até a parede).
//   3. Espera, mostra as contagens e anda a próxima célula, até NUM_CELULAS ou até
//      ter parede logo na frente.
// Conferir, com uma régua:
//   - o robô para no centro de cada célula (entre as emendas das paredes laterais);
//   - se ele para sempre antes do centro, aumentar CONTAGENS_POR_CELULA; se passa,
//     diminuir. O monitor mostra a média das contagens de cada célula.
//
// O robô precisa ficar parado no chão enquanto o LED pisca (calibração do giroscópio).

#include <Arduino.h>
#include <Wire.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "navegacao/pid.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_MS = 10;

// Contagens dos encoders (média das duas rodas) para andar uma célula de 180 mm.
// Média de 3 medidas empurrando o robô à mão no exemplo_encoders: 1430, 1321 e 1317.
constexpr int32_t CONTAGENS_POR_CELULA = 1350;
// Células andadas a cada aperto do BOOT.
constexpr int NUM_CELULAS = 3;
// O robô continua andando um pouco depois do freio; a célula termina antes desse tanto.
// É só o valor inicial: depois de cada célula o robô mede quanto andou depois do freio
// e ajusta sozinho, como a compensação das curvas no exemplo_parede_curva.
constexpr int32_t COMPENSACAO_FREIO_CONTAGENS = 100;
// Medidas fora dessa faixa (roda escorregou, robô empurrado) não entram no ajuste.
constexpr int32_t COMPENSACAO_FREIO_MAXIMA = 400;

// Valores calibrados no exemplo_parede_curva.
constexpr uint16_t LIMIAR_FRENTE_MM = 75;
constexpr uint16_t LIMIAR_LATERAL_MM = 100;
constexpr uint16_t DISTANCIA_CENTRO_MM = 50;
constexpr float GANHO_CENTRO = 0.3f;            // graus por mm
constexpr float INCLINACAO_CENTRO_MAX = 10.0f;  // graus
constexpr float FILTRO_CENTRO = 0.2f;
constexpr uint16_t FOLGA_FRENTE_MM = 30;
constexpr float VELOCIDADE_RETA = 0.30f;
constexpr uint32_t TEMPO_PARADO_MS = 500;
constexpr float QUEDA_L298N_V = 2.0f;
constexpr float VELOCIDADE_ARRANQUE = 0.60f;
constexpr uint32_t TEMPO_ARRANQUE_MS = 150;
// O MPU6050 está montado com o componente para baixo: o giro para a esquerda sai negativo.
constexpr bool IMU_INVERTIDA = true;

// Botão BOOT da própria placa (GPIO0, com pull-up na placa). Só influi no boot, então
// depois da inicialização serve de botão de partida, no lugar do SW2 (GPIO15).
constexpr uint8_t BOTAO_BOOT = 0;

static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, true};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, false};
static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, true};
static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

// Correção do rumo na reta: 0,02 de diferença de velocidade por grau de desvio.
static Pid pid_rumo(0.02f, 0.0f, 0.001f, PERIODO_MS / 1000.0f, 0.2f);

enum Estado {
    AGUARDANDO,
    ANDANDO,
    PARADO,
};

static Estado estado = AGUARDANDO;
static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

// Ângulo de guinada integrado do giroscópio, em graus. Positivo é para a esquerda.
static float angulo = 0.0f;
static float rumo = 0.0f;  // rumo do start, que a reta segue
static float inclinacao_filtrada = 0.0f;

static int celula = 0;  // células andadas desde o start
static int32_t compensacao_freio = COMPENSACAO_FREIO_CONTAGENS;
static int32_t contagem_freio = 0;  // média das contagens no momento do freio
static bool parou_pela_parede = false;
static uint32_t inicio_parado = 0;
static uint32_t inicio_movimento = 0;

// Média das duas rodas: cancela o efeito de o robô girar um pouco durante a reta.
static int32_t contagem_media() {
    return (encoders_contagem_esquerda() + encoders_contagem_direita()) / 2;
}

// Manda a velocidade aos motores. Logo depois de sair do repouso, cada roda que
// deve girar recebe pelo menos VELOCIDADE_ARRANQUE, no mesmo sentido.
static void comandar(float esquerdo, float direito) {
    if (millis() - inicio_movimento < TEMPO_ARRANQUE_MS) {
        esquerdo = copysignf(fmaxf(fabsf(esquerdo), VELOCIDADE_ARRANQUE), esquerdo);
        direito = copysignf(fmaxf(fabsf(direito), VELOCIDADE_ARRANQUE), direito);
    }
    motores_definir_velocidade(esquerdo, direito);
}

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
    motores_definir_duty_maximo(
        bateria_duty_maximo(MOTOR_TENSAO_NOMINAL, tensao - QUEDA_L298N_V));
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

// Inclinação do rumo, em graus, para levar o robô ao centro do corredor pelas paredes
// laterais. Positivo: perto demais da parede da direita, então inclina para a esquerda.
static float inclinacao_centro() {
    bool parede_esquerda = !lado_livre(distancias.esquerda);
    bool parede_direita = !lado_livre(distancias.direita);
    float erro_mm = 0.0f;
    if (parede_esquerda && parede_direita) {
        erro_mm = (float)distancias.esquerda - (float)distancias.direita;
    } else if (parede_esquerda) {
        erro_mm = (float)distancias.esquerda - DISTANCIA_CENTRO_MM;
    } else if (parede_direita) {
        erro_mm = DISTANCIA_CENTRO_MM - (float)distancias.direita;
    }
    return constrain(GANHO_CENTRO * erro_mm, -INCLINACAO_CENTRO_MAX, INCLINACAO_CENTRO_MAX);
}

static void comecar_celula() {
    encoders_zerar();
    pid_rumo.zerar();
    inclinacao_filtrada = 0.0f;
    inicio_movimento = millis();
    estado = ANDANDO;
}

static void parar(bool pela_parede) {
    motores_frear();
    contagem_freio = contagem_media();
    parou_pela_parede = pela_parede;
    inicio_parado = millis();
    estado = PARADO;
}

static void andar() {
    if (distancias.frente < LIMIAR_FRENTE_MM) {
        parar(true);
        return;
    }
    if (contagem_media() >= CONTAGENS_POR_CELULA - compensacao_freio) {
        parar(false);
        return;
    }
    // Correção positiva: o robô desviou para a direita e precisa virar para a esquerda.
    inclinacao_filtrada += FILTRO_CENTRO * (inclinacao_centro() - inclinacao_filtrada);
    float correcao = pid_rumo.atualizar(rumo + inclinacao_filtrada, angulo);
    comandar(VELOCIDADE_RETA - correcao, VELOCIDADE_RETA + correcao);
}

// Com o robô já parado: mostra a célula e ajusta a compensação do freio.
static void terminar_celula() {
    celula++;
    int32_t total = contagem_media();
    int32_t apos_freio = total - contagem_freio;
    Serial.printf("Célula %d: esquerda %ld, direita %ld, média %ld (alvo %ld). ", celula,
                  (long)encoders_contagem_esquerda(), (long)encoders_contagem_direita(),
                  (long)total, (long)CONTAGENS_POR_CELULA);
    if (parou_pela_parede) {
        // Parou pela parede: a distância andada não é uma célula inteira, não serve de medida.
        Serial.printf("Parou pela parede, frente %u mm.\n", distancias.frente);
        return;
    }
    if (apos_freio >= 0 && apos_freio <= COMPENSACAO_FREIO_MAXIMA) {
        compensacao_freio = (compensacao_freio + apos_freio) / 2;
    }
    Serial.printf("Andou %ld depois do freio, compensação agora %ld. frente %u mm\n",
                  (long)apos_freio, (long)compensacao_freio, distancias.frente);
}

// Com o robô parado no centro da célula: anda a próxima ou encerra.
static void decidir() {
    if (celula >= NUM_CELULAS) {
        Serial.printf("%d células andadas. Aperte BOOT para andar mais.\n", celula);
        motores_habilitar(false);
        estado = AGUARDANDO;
    } else if (distancias.frente <= LIMIAR_FRENTE_MM + FOLGA_FRENTE_MM) {
        Serial.printf("Parede na frente (%u mm). Aperte BOOT depois de virar o robô.\n",
                      distancias.frente);
        motores_habilitar(false);
        estado = AGUARDANDO;
    } else {
        comecar_celula();
    }
}

void setup() {
    // A ponte H só liga depois do start.
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);

    pinMode(BOTAO_BOOT, INPUT_PULLUP);
    pinMode(Pinos::LED_STATUS, OUTPUT);

    Serial.begin(115200);
    delay(1000);

    if (!encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO)) {
        Serial.println("Erro: falha ao configurar o PCNT dos encoders");
    }
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    if (!distancia_iniciar(Wire, PINOS_DISTANCIA)) {
        Serial.println("Aviso: algum sensor de distância não iniciou");
    }
    if (!energia_iniciar(Wire)) {
        Serial.println("Aviso: INA219 não respondeu. O duty máximo fica no padrão.");
    }
    if (!imu_iniciar(Wire)) {
        // Sem giroscópio o robô não anda reto.
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
    Serial.printf("Pronto. Aperte o botão BOOT ou envie \"s\" para andar %d células.\n",
                  NUM_CELULAS);
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
            rumo = angulo;  // o rumo do start é a referência da reta
            celula = 0;
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
        // Espera o robô assentar antes de medir quanto andou depois do freio.
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            terminar_celula();
            if (!bateria_ok()) {
                motores_habilitar(false);
                estado = AGUARDANDO;
                Serial.println("Bateria abaixo de 6,6 V. Teste encerrado.");
            } else {
                decidir();
            }
        }
        break;
    }
}
