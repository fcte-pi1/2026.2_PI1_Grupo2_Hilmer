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
//   - a curva fica perto de 90°. O robô ajusta a compensação sozinho a cada curva
//     (com a USB, o monitor mostra o erro e a compensação). Se a primeira curva ficar
//     muito longe, mudar o valor inicial COMPENSACAO_CURVA_GRAUS.
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
// No centro da célula a frente mede ~49 mm; os ~25 mm a mais cobrem a frenagem.
constexpr uint16_t LIMIAR_FRENTE_MM = 75;
// Acima dessa distância (ou sem medida), o lado é considerado livre.
constexpr uint16_t LIMIAR_LATERAL_MM = 100;
// Distância de cada sensor lateral até a parede com o robô no centro da célula (medida).
constexpr uint16_t DISTANCIA_CENTRO_MM = 50;
// Centralização na reta: as paredes laterais mudam o rumo que o giroscópio segue.
// Cada mm fora do centro inclina o rumo alvo em GANHO_CENTRO graus para o lado do
// centro (10 mm -> 3°). Assim o giroscópio ajuda a centralizar em vez de brigar.
constexpr float GANHO_CENTRO = 0.3f;  // graus por mm
// Inclinação máxima, para uma leitura ruim não jogar o robô de lado.
constexpr float INCLINACAO_CENTRO_MAX = 10.0f;  // graus
// Filtro da inclinação (0 a 1): os sensores variam ±3 mm entre leituras, e sem filtro
// o rumo alvo pularia ~1° a cada leitura, o que dá trancos pelo termo D do PID.
constexpr float FILTRO_CENTRO = 0.2f;
// Depois da curva, a frente só conta como livre com essa folga a mais.
constexpr uint16_t FOLGA_FRENTE_MM = 30;

constexpr float VELOCIDADE_RETA = 0.30f;
// Mais lento que a reta, para o robô escorregar pouco depois do freio. Com o duty
// máximo já descontando a queda da L298N, 0,32 e 0,24 dão perto do que 0,40 e 0,30
// davam antes dessa correção.
constexpr float VELOCIDADE_CURVA_MAX = 0.32f;
// Abaixo disso o N20 não vence o atrito e o robô para no meio da curva.
constexpr float VELOCIDADE_CURVA_MIN = 0.24f;
// O robô continua girando um pouco depois do freio; a curva termina antes desse tanto.
// É só o valor inicial: depois de cada curva o robô mede quanto girou depois do freio
// e ajusta a compensação sozinho (média entre a medida nova e a anterior).
constexpr float COMPENSACAO_CURVA_GRAUS = 13.0f;
// Medidas fora dessa faixa (curva que travou, robô empurrado) não entram no ajuste.
constexpr float COMPENSACAO_MAXIMA_GRAUS = 45.0f;
constexpr uint32_t TEMPO_PARADO_MS = 500;
// Ângulo pedido ao giroscópio numa curva de 90° no labirinto (a de 180° usa o dobro).
// Fica abaixo de 90 porque o giroscópio mede um pouco menos que o giro real; calibrado
// no labirinto, baixando de 5 em 5 graus até o robô sair reto da curva.
constexpr float ANGULO_CURVA_GRAUS = 85.0f;
// A L298N perde cerca de 2 V nos transistores: o motor recebe duty x (bateria - queda).
constexpr float QUEDA_L298N_V = 2.0f;
// Parado, o N20 precisa de mais tensão para vencer o atrito do que para continuar girando.
// No começo de cada movimento as rodas recebem pelo menos essa velocidade por um instante.
constexpr float VELOCIDADE_ARRANQUE = 0.60f;
constexpr uint32_t TEMPO_ARRANQUE_MS = 150;
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
// Rumo que o robô deve ter: múltiplos de 90° a partir do rumo do start. A reta segue
// esse rumo e a curva mira nele, então o erro de uma curva não se acumula nas próximas.
static float rumo = 0.0f;
static float inclinacao_filtrada = 0.0f;  // inclinação do rumo pelas paredes, filtrada
// Uma compensação por sentido: os motores não são iguais, então o robô escorrega
// diferente depois do freio para cada lado. Índice 0 = esquerda, 1 = direita.
static float compensacao[2] = {COMPENSACAO_CURVA_GRAUS, COMPENSACAO_CURVA_GRAUS};

static int indice_sentido(float sentido) {
    return sentido > 0 ? 0 : 1;
}

// Registro das curvas na memória, para ver depois pela USB ("r" no monitor serial)
// sem precisar do cabo durante o teste.
struct RegistroCurva {
    uint32_t t_ms;
    float graus;          // curva pedida
    float erro_final;     // onde parou em relação ao rumo
    float giro_apos;      // quanto girou depois do freio
    float compensacao;    // compensação usada nessa curva
};
constexpr int MAX_REGISTROS = 40;
static RegistroCurva registros[MAX_REGISTROS];
static int total_registros = 0;
static float graus_curva_atual = 0.0f;
static float compensacao_usada = 0.0f;

static void imprimir_registros() {
    Serial.printf("\n===== %d curvas registradas =====\n", total_registros);
    Serial.println(" n   t(s)   curva   erro final   giro após freio   compensação");
    int inicio = total_registros > MAX_REGISTROS ? total_registros - MAX_REGISTROS : 0;
    for (int i = inicio; i < total_registros; i++) {
        const RegistroCurva &r = registros[i % MAX_REGISTROS];
        Serial.printf("%2d %6.1f  %+6.0f    %+6.1f       %6.1f          %6.1f\n", i + 1,
                      r.t_ms / 1000.0f, r.graus, r.erro_final, r.giro_apos, r.compensacao);
    }
    Serial.printf("Compensação atual: esquerda %.1f, direita %.1f\n", compensacao[0],
                  compensacao[1]);
}
// Ângulo no momento do freio da curva e sentido dela (+1 esquerda, -1 direita), para
// medir quanto o robô girou depois do freio.
static bool medindo_curva = false;
static float angulo_freio = 0.0f;
static float sentido_curva = 0.0f;
static uint32_t inicio_curva = 0;
static uint32_t inicio_parado = 0;
static uint32_t inicio_movimento = 0;

// Manda a velocidade aos motores. Logo depois de sair do repouso, cada roda que
// deve girar recebe pelo menos VELOCIDADE_ARRANQUE, no mesmo sentido.
static void comandar(float esquerdo, float direito) {
    if (millis() - inicio_movimento < TEMPO_ARRANQUE_MS) {
        if (esquerdo != 0.0f) {
            esquerdo = copysignf(fmaxf(fabsf(esquerdo), VELOCIDADE_ARRANQUE), esquerdo);
        }
        if (direito != 0.0f) {
            direito = copysignf(fmaxf(fabsf(direito), VELOCIDADE_ARRANQUE), direito);
        }
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
        char comando = Serial.read();
        if (comando == 's') {
            apertou = true;
        } else if (comando == 'r') {
            imprimir_registros();
        }
    }
    return apertou;
}

static void comecar_reta() {
    pid_rumo.zerar();
    inclinacao_filtrada = 0.0f;
    inicio_movimento = millis();
    estado = ANDANDO;
    Serial.printf("Andando. frente %u mm\n", distancias.frente);
}

static void comecar_curva(float graus) {
    rumo += graus;
    graus_curva_atual = graus;
    inicio_curva = millis();
    inicio_movimento = millis();
    estado = VIRANDO;
    Serial.printf("Virando %.0f graus\n", graus);
}

static void parar() {
    motores_frear();
    inicio_parado = millis();
    estado = PARADO;
}

// Com o robô já parado depois de uma curva: quanto ele girou depois do freio é a
// compensação certa para a próxima curva.
static void ajustar_compensacao() {
    if (!medindo_curva) {
        return;
    }
    medindo_curva = false;
    float giro_apos_freio = (angulo - angulo_freio) * sentido_curva;
    float &comp = compensacao[indice_sentido(sentido_curva)];
    Serial.printf("Curva %s: parou a %+.1f graus do rumo. Girou %.1f graus depois do freio.",
                  sentido_curva > 0 ? "esquerda" : "direita", angulo - rumo, giro_apos_freio);
    if (giro_apos_freio >= 0.0f && giro_apos_freio <= COMPENSACAO_MAXIMA_GRAUS) {
        comp = 0.5f * comp + 0.5f * giro_apos_freio;
    }
    Serial.printf(" Compensação agora %.1f graus.\n", comp);

    RegistroCurva &r = registros[total_registros % MAX_REGISTROS];
    r = {millis(), graus_curva_atual, angulo - rumo, giro_apos_freio, compensacao_usada};
    total_registros++;
}

// Com o robô parado, escolhe a próxima ação pelas paredes.
static void decidir() {
    if (distancias.frente > LIMIAR_FRENTE_MM + FOLGA_FRENTE_MM) {
        comecar_reta();
    } else if (lado_livre(distancias.esquerda)) {
        comecar_curva(ANGULO_CURVA_GRAUS);
    } else if (lado_livre(distancias.direita)) {
        comecar_curva(-ANGULO_CURVA_GRAUS);
    } else {
        comecar_curva(2.0f * ANGULO_CURVA_GRAUS);
    }
}

// Inclinação do rumo, em graus, para levar o robô ao centro do corredor pelas paredes
// laterais. Positivo: perto demais da parede da direita, então inclina para a esquerda.
static float inclinacao_centro() {
    bool parede_esquerda = !lado_livre(distancias.esquerda);
    bool parede_direita = !lado_livre(distancias.direita);
    float erro_mm = 0.0f;
    if (parede_esquerda && parede_direita) {
        // No centro as duas distâncias são iguais.
        erro_mm = (float)distancias.esquerda - (float)distancias.direita;
    } else if (parede_esquerda) {
        erro_mm = (float)distancias.esquerda - DISTANCIA_CENTRO_MM;
    } else if (parede_direita) {
        erro_mm = DISTANCIA_CENTRO_MM - (float)distancias.direita;
    }
    return constrain(GANHO_CENTRO * erro_mm, -INCLINACAO_CENTRO_MAX, INCLINACAO_CENTRO_MAX);
}

static void andar() {
    if (distancias.frente < LIMIAR_FRENTE_MM) {
        Serial.printf("Parede a %u mm. esquerda %u mm, direita %u mm\n", distancias.frente,
                      distancias.esquerda, distancias.direita);
        parar();
        return;
    }
    // Correção positiva: o robô desviou para a direita e precisa virar para a esquerda.
    inclinacao_filtrada += FILTRO_CENTRO * (inclinacao_centro() - inclinacao_filtrada);
    float correcao = pid_rumo.atualizar(rumo + inclinacao_filtrada, angulo);
    comandar(VELOCIDADE_RETA - correcao, VELOCIDADE_RETA + correcao);
}

static void virar() {
    float falta = rumo - angulo;
    float sentido = falta > 0 ? 1.0f : -1.0f;
    float limite = compensacao[indice_sentido(sentido)];
    if (fabsf(falta) <= limite) {
        angulo_freio = angulo;
        sentido_curva = sentido;
        compensacao_usada = limite;
        medindo_curva = true;
        parar();
        return;
    }
    if (millis() - inicio_curva > TEMPO_MAX_CURVA_MS) {
        Serial.printf("Curva travou faltando %.1f graus. Decidindo de novo.\n", falta);
        // O rumo volta a ser o atual, para a reta seguinte não tentar terminar a curva.
        rumo = angulo;
        parar();
        return;
    }
    // Desacelera perto do fim para não passar do ângulo.
    float velocidade = constrain(fabsf(falta) * 0.01f, VELOCIDADE_CURVA_MIN, VELOCIDADE_CURVA_MAX);
    if (falta > 0) {
        comandar(-velocidade, velocidade);  // gira para a esquerda
    } else {
        comandar(velocidade, -velocidade);  // gira para a direita
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
            rumo = angulo;  // o rumo do start é a referência das curvas
            medindo_curva = false;
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
                ajustar_compensacao();
                decidir();
            }
        }
        break;
    case VIRANDO:
        virar();
        break;
    }
}
