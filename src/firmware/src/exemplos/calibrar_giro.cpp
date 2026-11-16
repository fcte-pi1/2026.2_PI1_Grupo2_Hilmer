// Calibração da escala do giroscópio pelas paredes (navegacao/calibracao_giro.*).
//
// Gravar:  pio run -e exemplo_calibrar_giro -t upload && pio device monitor
//
// O ANGULO_CURVA_GRAUS do exemplo_explorar (85°) foi achado no olho: o giroscópio
// mede um pouco menos que o giro real. Aqui o robô mede isso com as paredes:
//   1. gira no lugar, com o mesmo duty das curvas do exemplo_explorar, VOLTAS voltas para a esquerda e depois para a
//      direita, guardando o ângulo do giroscópio e a distância da frente;
//   2. cada vez que fica de frente para uma parede, a distância passa por um mínimo;
//      entre o mesmo mínimo uma volta depois o giro real é de 360°;
//   3. mostra quantos graus de giroscópio dão uma volta real em cada sentido, e o
//      ANGULO_CURVA_GRAUS certo para copiar no exemplo_explorar.
// Simulado no PC com o sensor de verdade modelado (cone, ruído, robô fora do centro):
// com 6 voltas, erro médio de 0,08° numa curva de 90° (pior caso 0,7°).
//
// Como colocar o robô: no beco do start (três paredes), centralizado. Funciona sem a
// USB: o LED acende quando está pronto, e o BOOT (ou "s" pelo monitor) começa. No fim,
// o LED pisca devagar se mediu e rápido se não deu. O resultado fica na flash: ligar a
// USB e enviar "r" para ver.
//
// O robô precisa ficar parado no chão enquanto o LED pisca (calibração do giroscópio).

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "navegacao/calibracao_giro.h"
#include "navegacao/equilibrio_rodas.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_MS = 10;

// Voltas em cada sentido. Simulado: 2,5 voltas dão 0,2° de erro médio por curva de 90°;
// 6 voltas, 0,08°.
constexpr float VOLTAS = 6.0f;
// Uma amostra a cada medida nova do sensor (~20 ms). Girando como nas curvas, sai uma
// leitura a cada ~3°: simulado, 0,10° de erro médio por curva de 90° (0,08° com 1°).
constexpr uint32_t PERIODO_AMOSTRA_MS = 20;
constexpr int MAX_AMOSTRAS = 2600;
// Se não completar as voltas nesse tempo (roda travada), para e analisa o que tiver.
constexpr uint32_t TEMPO_MAXIMO_GIRO_MS = 90000;

// Duty do giro: o mesmo das curvas do exemplo_explorar, que giram no lugar no chão.
// Mais devagar (0,26 com controle de velocidade), uma das rodas não vencia o atrito e o
// robô pivotava em volta dela.
constexpr float DUTY_GIRO = 0.28f;
// Roda que não andou RODA_PARADA_CONTAGENS em TEMPO_RODA_PARADA_MS recebe um novo
// impulso de VELOCIDADE_ARRANQUE por TEMPO_IMPULSO_MS.
constexpr int32_t RODA_PARADA_CONTAGENS = 5;
constexpr uint32_t TEMPO_RODA_PARADA_MS = 200;
constexpr uint32_t TEMPO_IMPULSO_MS = 100;
constexpr float VELOCIDADE_ARRANQUE = 0.60f;
constexpr uint32_t TEMPO_ARRANQUE_MS = 150;
constexpr uint32_t TEMPO_PARADO_MS = 1500;  // entre um sentido e o outro
constexpr float QUEDA_L298N_V = 2.0f;

// Equilíbrio das rodas pelos encoders: o motor esquerdo gira 6% a 8% mais rápido que o
// direito, e sem isso o robô anda enquanto gira (vários cm em 6 voltas).
constexpr float GANHO_EQUILIBRIO = 0.01f;
constexpr float EQUILIBRIO_MAXIMO = 0.2f;

// O MPU6050 está montado com o componente para baixo: o giro para a esquerda sai negativo.
constexpr bool IMU_INVERTIDA = true;
// Ângulo de curva usado hoje no exemplo_explorar, para comparar.
constexpr float ANGULO_CURVA_ATUAL = 85.0f;

constexpr uint8_t BOTAO_BOOT = 0;

static const PinosMotor MOTOR_ESQUERDO = {Pinos::MOTOR_ESQ_PWM, Pinos::MOTOR_ESQ_IN1,
                                          Pinos::MOTOR_ESQ_IN2, true};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, false};
static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, false};
static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

enum Estado { AGUARDANDO, GIRANDO, PARADO, FIM_OK, FIM_ERRO };

static Estado estado = AGUARDANDO;
static int sentido = 1;  // +1 esquerda, -1 direita
static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

static float angulo = 0.0f;       // ângulo integrado do giroscópio (sem escala)
static float velocidade_giro = 0.0f;
static float angulo_inicio = 0.0f;
// Detecção de roda parada: contagem e momento da última vez que cada roda andou, e até
// quando vai o impulso dela. Índice 0 esquerda, 1 direita.
static int32_t contagem_anterior[2];
static uint32_t ultimo_avanco[2];
static uint32_t fim_impulso[2];
static uint32_t inicio_giro = 0;
static uint32_t inicio_parado = 0;
static uint32_t ultima_amostra = 0;

static AmostraGiro amostras[MAX_AMOSTRAS];
static int total_amostras = 0;

// Resultado de cada sentido: índice 0 esquerda, 1 direita.
// Também guarda um diagnóstico de cada sentido, para entender um giro que não mediu
// sem precisar da USB durante o teste.
struct Medida {
    float graus_por_volta;
    float desvio;
    int voltas;
    int minimos;
    int amostras;
    int validas;            // leituras da frente até CALIB_DISTANCIA_MAXIMA_MM
    uint16_t menor_frente;  // menor leitura da frente (mm)
    bool completou;         // false: parado pelo botão ou pelo tempo máximo
    float graus_girados;    // ângulo do giroscópio no fim
    int32_t contagem_esquerda;
    int32_t contagem_direita;
    float segundos;
};
static Medida medidas[2];
static Preferences memoria;

static void atualizar_angulo() {
    static uint32_t ultimo_us = micros();
    uint32_t agora_us = micros();
    float dt = (agora_us - ultimo_us) / 1e6f;
    ultimo_us = agora_us;
    float giro_z;
    if (imu_ler_giro_z(giro_z)) {
        velocidade_giro = IMU_INVERTIDA ? -giro_z : giro_z;
        angulo += velocidade_giro * dt;
    }
}

static bool bateria_ok() {
    float tensao, corrente;
    if (!energia_ler(tensao, corrente)) {
        return true;
    }
    motores_definir_duty_maximo(
        bateria_duty_maximo(MOTOR_TENSAO_NOMINAL, tensao - QUEDA_L298N_V));
    return bateria_estado(tensao) != BATERIA_BLOQUEADA;
}

static void imprimir_resultado(const Medida m[2]) {
    Serial.println("\n===== Calibração do giroscópio =====");
    const char *nomes[2] = {"esquerda", "direita"};
    for (int s = 0; s < 2; s++) {
        if (m[s].amostras > 0) {
            Serial.printf("%-8s: %s em %.1f s, girou %.0f graus | %d amostras, %d válidas "
                          "(menor frente %u mm) | rodas: esquerda %ld, direita %ld\n",
                          nomes[s], m[s].completou ? "completou" : "PARADO ANTES", m[s].segundos,
                          m[s].graus_girados, m[s].amostras, m[s].validas, m[s].menor_frente,
                          (long)m[s].contagem_esquerda, (long)m[s].contagem_direita);
        }
        if (m[s].voltas == 0) {
            Serial.printf("%-8s: não mediu (%d mínimos). Confira se há paredes perto.\n", nomes[s],
                          m[s].minimos);
            continue;
        }
        float angulo_curva = 90.0f * m[s].graus_por_volta / 360.0f;
        Serial.printf("%-8s: %d voltas, %.2f graus de giroscópio por volta real (desvio %.2f)\n",
                      nomes[s], m[s].voltas, m[s].graus_por_volta, m[s].desvio);
        Serial.printf("          curva de 90° real = %.2f graus de giroscópio "
                      "(hoje o explorar usa %.1f: a curva sai com %.1f° reais)\n",
                      angulo_curva, ANGULO_CURVA_ATUAL,
                      ANGULO_CURVA_ATUAL * 360.0f / m[s].graus_por_volta);
    }
    if (m[0].voltas > 0 && m[1].voltas > 0) {
        float media = 0.5f * (m[0].graus_por_volta + m[1].graus_por_volta);
        Serial.printf("\nANGULO_CURVA_GRAUS recomendado (média dos dois sentidos): %.1f\n",
                      90.0f * media / 360.0f);
        Serial.printf("Diferença entre os sentidos: %.2f graus por volta\n",
                      fabsf(m[0].graus_por_volta - m[1].graus_por_volta));
    }
}

static void analisar(bool completou) {
    ResultadoCalibracaoGiro r = calibracao_giro_analisar(amostras, total_amostras);
    int i = sentido > 0 ? 0 : 1;
    int validas = 0;
    uint16_t menor = DISTANCIA_INVALIDA;
    for (int k = 0; k < total_amostras; k++) {
        if (amostras[k].frente > 0 && amostras[k].frente <= CALIB_DISTANCIA_MAXIMA_MM) {
            validas++;
        }
        if (amostras[k].frente < menor) {
            menor = amostras[k].frente;
        }
    }
    medidas[i] = {r.graus_por_volta, r.desvio_graus, r.voltas, r.minimos, total_amostras, validas,
                  menor, completou, (angulo - angulo_inicio) * sentido,
                  encoders_contagem_esquerda(), encoders_contagem_direita(),
                  (millis() - inicio_giro) / 1000.0f};
    // Salva já, para o diagnóstico ficar mesmo se o teste for interrompido depois.
    memoria.putBytes("medidas", medidas, sizeof(medidas));
    Serial.printf("Sentido %s: %d amostras, %d mínimos, %d voltas, %.2f graus por volta\n",
                  sentido > 0 ? "esquerda" : "direita", total_amostras, r.minimos, r.voltas,
                  r.graus_por_volta);
    // As duas rodas devem ter andado quase o mesmo tanto: senão o robô pivotou numa delas.
    Serial.printf("  rodas: esquerda %ld, direita %ld contagens (%.1f s)\n",
                  (long)encoders_contagem_esquerda(), (long)encoders_contagem_direita(),
                  (millis() - inicio_giro) / 1000.0f);
}

static void comecar_giro() {
    total_amostras = 0;
    angulo_inicio = angulo;
    for (int i = 0; i < 2; i++) {
        contagem_anterior[i] = 0;
        ultimo_avanco[i] = millis();
        fim_impulso[i] = millis() + TEMPO_ARRANQUE_MS;  // arranque das duas rodas
    }
    inicio_giro = millis();
    ultima_amostra = 0;
    encoders_zerar();
    estado = GIRANDO;
    Serial.printf("Girando %.0f voltas para a %s...\n", VOLTAS, sentido > 0 ? "esquerda" : "direita");
}

static void girar() {
    float girado = (angulo - angulo_inicio) * sentido;
    bool acabou = girado >= VOLTAS * 360.0f || total_amostras >= MAX_AMOSTRAS ||
                  millis() - inicio_giro > TEMPO_MAXIMO_GIRO_MS;
    if (acabou) {
        motores_frear();
        analisar(girado >= VOLTAS * 360.0f);
        inicio_parado = millis();
        estado = PARADO;
        return;
    }

    if (millis() - ultima_amostra >= PERIODO_AMOSTRA_MS) {
        ultima_amostra = millis();
        amostras[total_amostras++] = {angulo - angulo_inicio, distancias.frente};
    }

    // Roda parada: se não andou o bastante no último instante, ganha um novo impulso.
    int32_t contagens[2] = {abs(encoders_contagem_esquerda()), abs(encoders_contagem_direita())};
    for (int i = 0; i < 2; i++) {
        if (contagens[i] - contagem_anterior[i] >= RODA_PARADA_CONTAGENS) {
            contagem_anterior[i] = contagens[i];
            ultimo_avanco[i] = millis();
        } else if (millis() - ultimo_avanco[i] > TEMPO_RODA_PARADA_MS && millis() >= fim_impulso[i]) {
            fim_impulso[i] = millis() + TEMPO_IMPULSO_MS;
            ultimo_avanco[i] = millis();
        }
    }

    // A roda atrasada recebe um pouco mais de velocidade (a adiantada não é freada).
    float fe, fd;
    equilibrio_fatores(equilibrio_rodas(encoders_contagem_esquerda(), encoders_contagem_direita(),
                                        GANHO_EQUILIBRIO, EQUILIBRIO_MAXIMO),
                       fe, fd);
    float ve = millis() < fim_impulso[0] ? VELOCIDADE_ARRANQUE : DUTY_GIRO * fe;
    float vd = millis() < fim_impulso[1] ? VELOCIDADE_ARRANQUE : DUTY_GIRO * fd;
    // Esquerda: roda esquerda para trás, direita para a frente.
    motores_definir_velocidade(-sentido * ve, sentido * vd);
}

static bool comando_start() {
    static bool botao_anterior = false;
    static uint32_t ultimo_aperto = 0;
    bool botao = digitalRead(BOTAO_BOOT) == LOW;
    bool apertou = botao && !botao_anterior && millis() - ultimo_aperto > 300;
    if (apertou) {
        ultimo_aperto = millis();
    }
    botao_anterior = botao;
    while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 's') {
            apertou = true;
        } else if (c == 'r') {
            Medida salvas[2] = {};
            memoria.getBytes("medidas", salvas, sizeof(salvas));
            imprimir_resultado(salvas);
        }
    }
    return apertou;
}

void setup() {
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);
    pinMode(BOTAO_BOOT, INPUT_PULLUP);
    pinMode(Pinos::LED_STATUS, OUTPUT);
    Serial.begin(115200);
    delay(1000);

    memoria.begin("calib_giro", false);
    if (memoria.getBytesLength("medidas") == sizeof(medidas)) {
        Serial.println("Resultado da última calibração:");
        Medida salvas[2];
        memoria.getBytes("medidas", salvas, sizeof(salvas));
        imprimir_resultado(salvas);
    }

    if (!encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO)) {
        Serial.println("Aviso: falha ao configurar os encoders. O giro fica sem equilíbrio.");
    }
    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    if (!distancia_iniciar(Wire, PINOS_DISTANCIA)) {
        Serial.println("Aviso: algum sensor de distância não iniciou");
    }
    if (!energia_iniciar(Wire)) {
        Serial.println("Aviso: INA219 não respondeu. O duty máximo fica no padrão.");
    }
    if (!imu_iniciar(Wire)) {
        Serial.println("Erro: MPU6050 não respondeu. Teste cancelado.");
        for (;;) {
            digitalWrite(Pinos::LED_STATUS, !digitalRead(Pinos::LED_STATUS));
            delay(100);
        }
    }
    Serial.println("Calibrando o zero do giroscópio. Não mexa no robô.");
    while (!imu_calibrar()) {
        Serial.println("O robô se mexeu. Calibrando de novo.");
        digitalWrite(Pinos::LED_STATUS, !digitalRead(Pinos::LED_STATUS));
    }
    digitalWrite(Pinos::LED_STATUS, HIGH);
    Serial.println("Pronto. Robô no beco do start, centralizado. Aperte BOOT ou envie \"s\".");
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
        if (estado == GIRANDO || estado == PARADO) {
            motores_frear();
            if (estado == GIRANDO) {
                analisar(false);  // guarda o que já tinha sido medido neste sentido
            }
            motores_habilitar(false);
            estado = AGUARDANDO;
            Serial.println("Parado pelo botão.");
        } else if (!bateria_ok()) {
            Serial.println("Bateria abaixo de 6,6 V. Recarregue antes de usar.");
            estado = FIM_ERRO;
        } else {
            medidas[0] = medidas[1] = {};
            motores_habilitar(true);
            sentido = 1;
            comecar_giro();
        }
        return;
    }

    switch (estado) {
    case AGUARDANDO:
        break;
    case GIRANDO:
        girar();
        break;
    case PARADO:
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            if (sentido > 0) {
                sentido = -1;
                comecar_giro();
            } else {
                motores_habilitar(false);
                memoria.putBytes("medidas", medidas, sizeof(medidas));
                imprimir_resultado(medidas);
                estado = (medidas[0].voltas > 0 && medidas[1].voltas > 0) ? FIM_OK : FIM_ERRO;
            }
        }
        break;
    case FIM_OK:
        digitalWrite(Pinos::LED_STATUS, (millis() / 500) % 2);
        break;
    case FIM_ERRO:
        digitalWrite(Pinos::LED_STATUS, (millis() / 100) % 2);
        break;
    }
}
