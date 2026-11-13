// Exemplo de resolver o labirinto com o Flood Fill (passo B da navegação).
//
// Gravar:  pio run -e exemplo_explorar -t upload && pio device monitor
//
// Junta o exemplo_celula (andar uma célula), o exemplo_parede_curva (curvas) e o
// Flood Fill. A cada célula, com o robô parado no centro:
//   1. lê as paredes da frente, da esquerda e da direita e marca no mapa;
//   2. se chegou ao canto oposto ao start, para e pisca o LED;
//   3. o Flood Fill escolhe a vizinha mais perto do objetivo;
//   4. vira para ela (0°, 90° ou 180°) e anda uma célula.
//
// Como colocar o robô: no canto de saída, centralizado na célula e virado para a
// saída do beco. Antes de gravar, ajustar LARGURA, ALTURA e LABIRINTO_A_DIREITA.
// Funciona sem a USB: o LED acende quando o robô está pronto, e o botão BOOT da placa
// (ou "s" pelo monitor serial) começa e para. Pelo monitor, "m" mostra o mapa e "c"
// zera a calibração das contagens por célula.
//
// LED no fim: piscando devagar, chegou ao objetivo; piscando rápido, parou por erro.
// Registro sem USB e sem cartão SD: cada célula fica gravada na flash da ESP, que não
// se apaga ao desligar. Depois da corrida, desligar a bateria, soltar o fio do 5V,
// ligar a USB, abrir o monitor e enviar "r".
//
// O robô precisa ficar parado no chão enquanto o LED pisca (calibração do giroscópio).

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "navegacao/floodfill.h"
#include "navegacao/labirinto.h"
#include "navegacao/pid.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_MS = 10;

// Tamanho do labirinto, visto do start: ALTURA é o número de células para a frente
// do robô e LARGURA para o lado em que o labirinto continua. Por enquanto a ALTURA
// vai até 4 (LABIRINTO_MAX_ALTURA): o robô sai virado para o lado curto.
constexpr int LARGURA = 4;
constexpr int ALTURA = 4;
// true: o labirinto continua para a direita do robô no start; false: para a esquerda.
constexpr bool LABIRINTO_A_DIREITA = false;

// Parado no centro, a parede da frente mede ~49 mm e a da célula seguinte ~230 mm.
constexpr uint16_t LIMIAR_PAREDE_FRENTE_MM = 120;

// Alinhamento antes de virar: com parede na frente, o robô anda para a frente ou para
// trás até a parede ficar na distância do centro da célula. Assim a curva acontece no
// centro e a ponta do robô não raspa na parede (principalmente no 180°).
constexpr uint16_t DISTANCIA_FRENTE_CENTRO_MM = 54;
constexpr uint16_t TOLERANCIA_ALINHAMENTO_MM = 6;
constexpr float GANHO_ALINHAMENTO = 0.01f;  // velocidade por mm de erro
// Abaixo disso o N20 para no meio do ajuste; acima, passa do ponto (o sensor mede a
// cada ~20 ms).
constexpr float VELOCIDADE_ALINHAMENTO_MIN = 0.30f;
constexpr float VELOCIDADE_ALINHAMENTO_MAX = 0.38f;
// Parado, o N20 não sai do lugar com a velocidade do ajuste: no começo ele recebe
// VELOCIDADE_ARRANQUE por um instante, mais curto que o da reta para não passar do ponto.
constexpr uint32_t TEMPO_IMPULSO_ALINHAMENTO_MS = 60;
constexpr uint32_t TEMPO_MAX_ALINHAMENTO_MS = 1500;
// Trava de segurança: um labirinto 4x4 não precisa de tantas células.
constexpr int MAX_CELULAS = 60;

// Valor calibrado no exemplo_celula. É só o valor inicial: o robô se recalibra pela
// parede da frente (ver calibrar_celula) e guarda o valor na flash entre corridas.
constexpr int32_t CONTAGENS_POR_CELULA = 1350;
constexpr float TAMANHO_CELULA_MM = 180.0f;
// A calibração só usa a parede da frente até essa distância: mais longe, o VL53L0X
// erra mais e a medida piora em vez de melhorar.
constexpr uint16_t ALCANCE_CALIBRACAO_MM = 500;
// Para contar quantas células o robô andou basta errar menos de meia célula (90 mm),
// então a parede da frente vale até mais longe.
constexpr uint16_t ALCANCE_CONTAGEM_MM = 1000;
// Uma medida que dá mais de 10% longe do valor inicial é descartada (roda escorregou,
// leitura ruim): o valor calibrado nunca sai dessa faixa. 10% são 18 mm por célula;
// com 25%, leituras ruins dos sensores levavam o robô a parar entre células.
constexpr float VARIACAO_MAXIMA_CALIBRACAO = 0.10f;
constexpr int32_t COMPENSACAO_FREIO_CONTAGENS = 100;
constexpr int32_t COMPENSACAO_FREIO_MAXIMA = 400;

// Valores calibrados no exemplo_parede_curva.
constexpr uint16_t LIMIAR_FRENTE_MM = 75;
constexpr uint16_t LIMIAR_LATERAL_MM = 100;
constexpr uint16_t DISTANCIA_CENTRO_MM = 50;
constexpr float GANHO_CENTRO = 0.3f;            // graus por mm
constexpr float INCLINACAO_CENTRO_MAX = 10.0f;  // graus
constexpr float FILTRO_CENTRO = 0.2f;
constexpr float VELOCIDADE_RETA = 0.30f;
constexpr float VELOCIDADE_CURVA_MAX = 0.32f;
constexpr float VELOCIDADE_CURVA_MIN = 0.24f;
constexpr float COMPENSACAO_CURVA_GRAUS = 13.0f;
constexpr float COMPENSACAO_MAXIMA_GRAUS = 45.0f;
constexpr float ANGULO_CURVA_GRAUS = 85.0f;
constexpr uint32_t TEMPO_PARADO_MS = 500;
constexpr float QUEDA_L298N_V = 2.0f;
constexpr float VELOCIDADE_ARRANQUE = 0.60f;
constexpr uint32_t TEMPO_ARRANQUE_MS = 150;
constexpr uint32_t TEMPO_MAX_CURVA_MS = 3000;
// O MPU6050 está montado com o componente para baixo: o giro para a esquerda sai negativo.
constexpr bool IMU_INVERTIDA = true;

// Correção do zero do giroscópio durante a corrida. O zero medido no boot muda com o
// tempo e a temperatura, e o erro integrado faz as curvas saírem tortas depois de
// algumas dezenas de segundos. Parado no centro das células, o robô não gira: o que o
// giroscópio mede ali é o erro de zero, que é medido de novo a cada parada.
constexpr uint32_t TEMPO_ASSENTAR_MS = 250;   // espera o balanço do freio passar
constexpr int AMOSTRAS_ZERO = 20;             // 0,2 s de leituras por medida
constexpr float GIRO_MAXIMO_PARADO = 2.0f;    // graus/s; acima disso o robô está se mexendo
constexpr float FILTRO_ZERO = 0.5f;           // peso da medida nova

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

static Labirinto labirinto(LARGURA, ALTURA);
static FloodFill floodfill(labirinto);

enum Estado {
    AGUARDANDO,
    ANDANDO,
    VIRANDO,
    ALINHANDO,        // ajusta a distância até a parede da frente antes de virar
    PARADO_CELULA,    // parado no centro de uma célula: lê paredes e decide
    PARADO_ALINHADO,  // parado depois do alinhamento: faz a curva pendente
    PARADO_CURVA,     // parado depois de uma curva: anda a célula (ou a 2ª metade do 180°)
    CHEGOU,
    ERRO,
};

static Estado estado = AGUARDANDO;
static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

// Posição no mapa. O start é a célula (0, 0) virado para o norte.
static int pos_x = 0;
static int pos_y = 0;
static Direcao direcao = NORTE;
static Direcao direcao_alvo = NORTE;  // para onde o robô fica virado depois da curva
static int celulas_andadas = 0;
static uint32_t inicio_corrida = 0;

// Ângulo de guinada integrado do giroscópio, em graus. Positivo é para a esquerda.
static float angulo = 0.0f;
static float erro_zero_giro = 0.0f;  // graus/s, medido nas paradas e descontado do giro
static float soma_zero = 0.0f;
static int amostras_zero = 0;
// Rumo que o robô deve ter: múltiplos de ANGULO_CURVA_GRAUS a partir do start.
static float rumo = 0.0f;
static float inclinacao_filtrada = 0.0f;
// Compensação das curvas por sentido. Índice 0 = esquerda, 1 = direita.
static float compensacao[2] = {COMPENSACAO_CURVA_GRAUS, COMPENSACAO_CURVA_GRAUS};
static bool medindo_curva = false;
static float angulo_freio = 0.0f;
static float sentido_curva = 0.0f;

static int32_t contagens_por_celula = CONTAGENS_POR_CELULA;
static uint16_t frente_inicio_celula = DISTANCIA_INVALIDA;  // lida parado, antes de andar
static int32_t compensacao_freio = COMPENSACAO_FREIO_CONTAGENS;
static int32_t contagem_freio = 0;
static bool parou_pela_parede = false;

// Curva que espera o fim do alinhamento, e a segunda metade do 180°, que é feito como
// duas curvas de 90° com um alinhamento no meio (a parede do lado vira a da frente).
static float curva_pendente = 0.0f;
static float segunda_metade_180 = 0.0f;

static uint32_t inicio_curva = 0;
static uint32_t inicio_alinhamento = 0;
static uint32_t inicio_parado = 0;
static uint32_t inicio_movimento = 0;

// ---------------------------------------------------------------------------
// Direções: o mapa usa norte, leste, sul e oeste; o robô vê frente, direita,
// trás e esquerda. Com o índice N=0, L=1, S=2, O=3, virar à direita soma 1.

static const Direcao DIRECOES[] = {NORTE, LESTE, SUL, OESTE};
static const char *NOMES_DIRECAO[] = {"norte", "leste", "sul", "oeste"};

static int indice(Direcao d) {
    switch (d) {
        case NORTE: return 0;
        case LESTE: return 1;
        case SUL: return 2;
        default: return 3;
    }
}

// Direção no mapa de um lado do robô. "passos_direita": 0 frente, 1 direita,
// 2 trás, 3 esquerda. Com o labirinto à esquerda do start, o mapa fica espelhado:
// o leste do mapa é a esquerda do robô.
static Direcao lado_no_mapa(int passos_direita) {
    int passos = LABIRINTO_A_DIREITA ? passos_direita : (4 - passos_direita) % 4;
    return DIRECOES[(indice(direcao) + passos) % 4];
}

// ---------------------------------------------------------------------------
// Registro da corrida na flash (NVS), para ler depois pela USB.

// Ação decidida em cada célula: 0 segue, 1 direita, 2 volta, 3 esquerda.
// Os códigos acima de 3 marcam o fim da corrida.
enum CodigoRegistro : uint8_t {
    ACAO_SEGUE,
    ACAO_DIREITA,
    ACAO_VOLTA,
    ACAO_ESQUERDA,
    FIM_OBJETIVO,
    FIM_SEM_CAMINHO,
    FIM_FORA_DO_MAPA,
    FIM_CELULAS_DEMAIS,
    FIM_BOTAO,
    FIM_BATERIA,
    CURVA_TRAVOU,
};
static const char *NOMES_CODIGO[] = {
    "segue", "vira à direita", "volta (180)", "vira à esquerda",
    "FIM: objetivo atingido", "FIM: sem caminho até o objetivo",
    "FIM: posição fora do mapa", "FIM: células demais", "FIM: parado pelo botão",
    "FIM: bateria baixa", "curva travou",
};

struct __attribute__((packed)) RegistroCelula {
    uint8_t x;
    uint8_t y;
    uint8_t direcao;    // índice N=0, L=1, S=2, O=3
    uint8_t paredes;    // bit 0 frente, bit 1 direita, bit 2 esquerda (vistas pelo robô)
    uint8_t codigo;     // CodigoRegistro
    uint16_t frente_mm;
    uint16_t direita_mm;
    uint16_t esquerda_mm;
    uint16_t t_ds;      // tempo desde o start, em décimos de segundo
};
constexpr int MAX_REGISTROS = 80;
static RegistroCelula registros[MAX_REGISTROS];
static int total_registros = 0;
static Preferences memoria;

// Guarda na RAM e regrava o vetor inteiro na flash. Leva poucos ms e só acontece com
// o robô parado, então não atrapalha o controle.
static void registrar(uint8_t codigo, uint8_t paredes) {
    if (total_registros >= MAX_REGISTROS) {
        return;
    }
    registros[total_registros++] = {
        (uint8_t)pos_x, (uint8_t)pos_y, (uint8_t)indice(direcao), paredes, codigo,
        distancias.frente, distancias.direita, distancias.esquerda,
        (uint16_t)((millis() - inicio_corrida) / 100)};
    memoria.putBytes("log", registros, total_registros * sizeof(RegistroCelula));
}

static void imprimir_registro() {
    size_t tamanho = memoria.getBytesLength("log");
    int n = tamanho / sizeof(RegistroCelula);
    if (n > MAX_REGISTROS) {
        n = MAX_REGISTROS;
    }
    RegistroCelula lidos[MAX_REGISTROS];
    memoria.getBytes("log", lidos, n * sizeof(RegistroCelula));
    Serial.printf("\n===== Última corrida: %d registros =====\n", n);
    Serial.println("  t(s)  célula  virado   paredes F D E   frente  direita esquerda  ação");
    for (int i = 0; i < n; i++) {
        const RegistroCelula &r = lidos[i];
        Serial.printf("%6.1f  (%d,%d)   %-6s     %c %c %c    %5u   %5u   %5u    %s\n",
                      r.t_ds / 10.0f, r.x, r.y, NOMES_DIRECAO[r.direcao & 3],
                      (r.paredes & 1) ? 'X' : '.', (r.paredes & 2) ? 'X' : '.',
                      (r.paredes & 4) ? 'X' : '.', r.frente_mm, r.direita_mm, r.esquerda_mm,
                      r.codigo <= CURVA_TRAVOU ? NOMES_CODIGO[r.codigo] : "?");
    }
}

// ---------------------------------------------------------------------------

// Distância andada na reta, em contagens: a roda que mais contou. Na reta as duas
// contam quase igual, e se um encoder falhar (fio solto) o robô continua medindo pelo
// outro, em vez de achar que andou metade e passar direto das células.
static int32_t contagem_andada() {
    return max(encoders_contagem_esquerda(), encoders_contagem_direita());
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

// Parado no chão, com os motores desligados ou freados há pelo menos TEMPO_ASSENTAR_MS.
static bool parado_assentado() {
    bool parado = estado == AGUARDANDO || estado == PARADO_CELULA ||
                  estado == PARADO_CURVA || estado == PARADO_ALINHADO;
    return parado && (estado == AGUARDANDO || millis() - inicio_parado >= TEMPO_ASSENTAR_MS);
}

static void atualizar_angulo() {
    static uint32_t ultimo_us = micros();
    uint32_t agora_us = micros();
    float dt = (agora_us - ultimo_us) / 1e6f;
    ultimo_us = agora_us;

    float giro_z;
    if (!imu_ler_giro_z(giro_z)) {
        return;
    }
    giro_z = (IMU_INVERTIDA ? -giro_z : giro_z) - erro_zero_giro;

    if (!parado_assentado() || fabsf(giro_z) > GIRO_MAXIMO_PARADO) {
        // Em movimento (ou alguém mexeu no robô parado): integra normalmente.
        angulo += giro_z * dt;
        soma_zero = 0.0f;
        amostras_zero = 0;
        return;
    }
    // Parado: o ângulo não muda, e o que o giroscópio mede é o erro de zero.
    soma_zero += giro_z;
    amostras_zero++;
    if (amostras_zero >= AMOSTRAS_ZERO) {
        erro_zero_giro += FILTRO_ZERO * soma_zero / amostras_zero;
        soma_zero = 0.0f;
        amostras_zero = 0;
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

// Desenha o mapa no monitor serial, com o norte para cima. R é o robô, G o objetivo.
static void imprimir_mapa() {
    Serial.println();
    for (int y = ALTURA - 1; y >= 0; y--) {
        for (int x = 0; x < LARGURA; x++) {
            Serial.print(labirinto.temParede(x, y, NORTE) ? "+---" : "+   ");
        }
        Serial.println("+");
        for (int x = 0; x < LARGURA; x++) {
            Serial.print(labirinto.temParede(x, y, OESTE) ? "|" : " ");
            if (x == pos_x && y == pos_y) {
                Serial.print(" R ");
            } else if (floodfill.objetivo(x, y)) {
                Serial.print(" G ");
            } else {
                Serial.printf("%2u ", floodfill.distancia(x, y));
            }
        }
        Serial.println("|");
    }
    for (int x = 0; x < LARGURA; x++) {
        Serial.print("+---");
    }
    Serial.println("+");
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
            imprimir_registro();
        } else if (comando == 'c') {
            // Volta a calibração da célula para o valor medido no exemplo_celula.
            contagens_por_celula = CONTAGENS_POR_CELULA;
            memoria.remove("celula");
            Serial.printf("Calibração zerada: %ld contagens por célula.\n",
                          (long)contagens_por_celula);
        } else if (comando == 'm') {
            floodfill.calcular();
            imprimir_mapa();
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

// ---------------------------------------------------------------------------
// Movimentos

static void parar(Estado proximo) {
    motores_frear();
    inicio_parado = millis();
    estado = proximo;
}

static void comecar_celula() {
    // O robô ainda está parado: a leitura da frente é boa para a calibração.
    frente_inicio_celula = distancias.frente;
    encoders_zerar();
    pid_rumo.zerar();
    inclinacao_filtrada = 0.0f;
    inicio_movimento = millis();
    estado = ANDANDO;
}

static void andar() {
    bool parede = distancias.frente < LIMIAR_FRENTE_MM;
    if (parede || contagem_andada() >= contagens_por_celula - compensacao_freio) {
        contagem_freio = contagem_andada();
        parou_pela_parede = parede;
        parar(PARADO_CELULA);
        return;
    }
    // Correção positiva: o robô desviou para a direita e precisa virar para a esquerda.
    inclinacao_filtrada += FILTRO_CENTRO * (inclinacao_centro() - inclinacao_filtrada);
    float correcao = pid_rumo.atualizar(rumo + inclinacao_filtrada, angulo);
    comandar(VELOCIDADE_RETA - correcao, VELOCIDADE_RETA + correcao);
}

// Com o robô parado depois da célula: ajusta quanto ele anda depois do freio.
static void ajustar_freio() {
    if (parou_pela_parede) {
        return;  // não andou uma célula inteira, a medida não serve
    }
    int32_t apos_freio = contagem_andada() - contagem_freio;
    if (apos_freio >= 0 && apos_freio <= COMPENSACAO_FREIO_MAXIMA) {
        compensacao_freio = (compensacao_freio + apos_freio) / 2;
    }
}

// Com o robô parado depois da célula: se havia parede na frente no começo e no fim,
// o sensor da frente mede quanto ele andou de verdade, em mm. Com as contagens dos
// encoders nesse trecho, dá para corrigir as contagens por célula.
static void calibrar_celula() {
    uint16_t frente_fim = distancias.frente;
    if (frente_inicio_celula > ALCANCE_CALIBRACAO_MM || frente_fim == DISTANCIA_INVALIDA ||
        frente_fim >= frente_inicio_celula) {
        return;  // sem parede na frente ao alcance, nos dois momentos
    }
    float andou_mm = frente_inicio_celula - frente_fim;
    if (andou_mm < 0.5f * TAMANHO_CELULA_MM || andou_mm > 1.5f * TAMANHO_CELULA_MM) {
        return;  // não parece uma célula: leitura ruim ou robô empurrado
    }
    int32_t medida = lroundf(contagem_andada() * TAMANHO_CELULA_MM / andou_mm);
    int32_t minimo = lroundf(CONTAGENS_POR_CELULA * (1.0f - VARIACAO_MAXIMA_CALIBRACAO));
    int32_t maximo = lroundf(CONTAGENS_POR_CELULA * (1.0f + VARIACAO_MAXIMA_CALIBRACAO));
    if (medida < minimo || medida > maximo) {
        Serial.printf("Calibração descartada: %ld contagens por célula.\n", (long)medida);
        return;
    }
    // Média entre a medida nova e o valor atual, para uma leitura ruim não pesar demais.
    contagens_por_celula = (contagens_por_celula + medida) / 2;
    memoria.putInt("celula", contagens_por_celula);
    Serial.printf("Calibração pela parede: andou %.0f mm em %ld contagens. "
                  "Contagens por célula agora %ld.\n",
                  andou_mm, (long)contagem_andada(), (long)contagens_por_celula);
}

// graus > 0 vira para a esquerda.
static void comecar_curva(float graus) {
    rumo += graus;
    inicio_curva = millis();
    inicio_movimento = millis();
    estado = VIRANDO;
}

static void virar() {
    float falta = rumo - angulo;
    float sentido = falta > 0 ? 1.0f : -1.0f;
    float limite = compensacao[sentido > 0 ? 0 : 1];
    if (fabsf(falta) <= limite) {
        angulo_freio = angulo;
        sentido_curva = sentido;
        medindo_curva = true;
        parar(PARADO_CURVA);
        return;
    }
    if (millis() - inicio_curva > TEMPO_MAX_CURVA_MS) {
        Serial.printf("Curva travou faltando %.1f graus.\n", falta);
        registrar(CURVA_TRAVOU, 0);
        // O rumo volta a ser o atual, para a reta seguinte não tentar terminar a curva.
        rumo = angulo;
        parar(PARADO_CURVA);
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

// Com o robô parado depois de uma curva: quanto ele girou depois do freio é a
// compensação certa para a próxima curva do mesmo lado.
static void ajustar_compensacao() {
    if (!medindo_curva) {
        return;
    }
    medindo_curva = false;
    float giro_apos_freio = (angulo - angulo_freio) * sentido_curva;
    float &comp = compensacao[sentido_curva > 0 ? 0 : 1];
    if (giro_apos_freio >= 0.0f && giro_apos_freio <= COMPENSACAO_MAXIMA_GRAUS) {
        comp = 0.5f * comp + 0.5f * giro_apos_freio;
    }
}

// Anda para a frente ou para trás até a parede da frente ficar a
// DISTANCIA_FRENTE_CENTRO_MM. Sem a curva do rumo: o trecho é de poucos mm.
static void alinhar() {
    uint16_t frente = distancias.frente;
    float erro = (float)frente - DISTANCIA_FRENTE_CENTRO_MM;  // > 0: longe demais
    bool acabou = frente == DISTANCIA_INVALIDA || fabsf(erro) <= TOLERANCIA_ALINHAMENTO_MM ||
                  millis() - inicio_alinhamento > TEMPO_MAX_ALINHAMENTO_MS;
    if (acabou) {
        parar(PARADO_ALINHADO);
        return;
    }
    float velocidade = constrain(fabsf(erro) * GANHO_ALINHAMENTO, VELOCIDADE_ALINHAMENTO_MIN,
                                 VELOCIDADE_ALINHAMENTO_MAX);
    if (millis() - inicio_alinhamento < TEMPO_IMPULSO_ALINHAMENTO_MS) {
        velocidade = VELOCIDADE_ARRANQUE;
    }
    velocidade = copysignf(velocidade, erro);
    motores_definir_velocidade(velocidade, velocidade);
}

// Faz a curva no centro da célula: com parede na frente, alinha antes de virar.
static void preparar_curva(float graus) {
    if (distancias.frente < LIMIAR_PAREDE_FRENTE_MM) {
        curva_pendente = graus;
        inicio_alinhamento = millis();
        estado = ALINHANDO;
    } else {
        comecar_curva(graus);
    }
}

// ---------------------------------------------------------------------------
// Navegação

// Fim da corrida: para os motores, registra o motivo e mostra o mapa. O estado final
// define o LED: CHEGOU pisca devagar, ERRO pisca rápido.
static void encerrar(CodigoRegistro codigo, Estado final, const char *motivo) {
    motores_frear();
    motores_habilitar(false);
    estado = final;
    registrar(codigo, 0);
    Serial.println(motivo);
    Serial.printf("Correção do zero do giroscópio: %.3f graus/s\n", erro_zero_giro);
    floodfill.calcular();
    imprimir_mapa();
}

// Marca no mapa as paredes da frente, da direita e da esquerda.
static void ler_paredes(bool &frente, bool &direita, bool &esquerda) {
    frente = distancias.frente < LIMIAR_PAREDE_FRENTE_MM;
    direita = !lado_livre(distancias.direita);
    esquerda = !lado_livre(distancias.esquerda);
    if (frente) {
        labirinto.colocarParede(pos_x, pos_y, lado_no_mapa(0));
    }
    if (direita) {
        labirinto.colocarParede(pos_x, pos_y, lado_no_mapa(1));
    }
    if (esquerda) {
        labirinto.colocarParede(pos_x, pos_y, lado_no_mapa(3));
    }
}

// Robô parado no centro de (pos_x, pos_y): lê as paredes e decide a próxima célula.
static void decidir() {
    bool frente, direita, esquerda;
    ler_paredes(frente, direita, esquerda);

    if (floodfill.objetivo(pos_x, pos_y)) {
        encerrar(FIM_OBJETIVO, CHEGOU, "");
        Serial.printf("Objetivo atingido em %d células e %.1f s.\n", celulas_andadas,
                      (millis() - inicio_corrida) / 1000.0f);
        return;
    }
    if (celulas_andadas >= MAX_CELULAS) {
        encerrar(FIM_CELULAS_DEMAIS, ERRO,
                 "Células demais sem chegar ao objetivo. Confira o mapa.");
        return;
    }

    Direcao proxima;
    if (!floodfill.proximaDirecao(pos_x, pos_y, direcao, proxima)) {
        encerrar(FIM_SEM_CAMINHO, ERRO,
                 "Sem caminho até o objetivo: alguma parede foi lida errada.");
        return;
    }

    // Quantas vezes 90° para a direita (do mapa) separam a direção atual da próxima.
    int passos = (indice(proxima) - indice(direcao) + 4) % 4;
    if (!LABIRINTO_A_DIREITA) {
        passos = (4 - passos) % 4;  // mapa espelhado: direita do mapa é esquerda do robô
    }
    static const char *ACOES[] = {"segue", "vira à direita", "volta", "vira à esquerda"};
    Serial.printf("(%d,%d) virado para %s. Paredes: frente %s, direita %s, esquerda %s. "
                  "Distância %u. Vai para %s: %s.\n",
                  pos_x, pos_y, NOMES_DIRECAO[indice(direcao)], frente ? "sim" : "não",
                  direita ? "sim" : "não", esquerda ? "sim" : "não",
                  floodfill.distancia(pos_x, pos_y), NOMES_DIRECAO[indice(proxima)],
                  ACOES[passos]);

    registrar(passos, (frente ? 1 : 0) | (direita ? 2 : 0) | (esquerda ? 4 : 0));

    direcao_alvo = proxima;
    if (passos == 0) {
        comecar_celula();
    } else if (passos == 1) {
        preparar_curva(-ANGULO_CURVA_GRAUS);
    } else if (passos == 3) {
        preparar_curva(ANGULO_CURVA_GRAUS);
    } else {
        // 180°: duas curvas de 90° para o mesmo lado. Entre elas, a parede que estava do
        // lado fica na frente e o alinhamento centraliza o robô também nesse eixo.
        // O lado é o da parede mais longe: com o robô fora do centro, girar para o lado
        // mais perto faz a ponta raspar na parede. Sem leitura (65535) conta como longe.
        float graus = distancias.esquerda >= distancias.direita ? ANGULO_CURVA_GRAUS
                                                                 : -ANGULO_CURVA_GRAUS;
        Serial.printf("180° pela %s (esquerda %u mm, direita %u mm).\n",
                      graus > 0 ? "esquerda" : "direita", distancias.esquerda,
                      distancias.direita);
        segunda_metade_180 = graus;
        preparar_curva(graus);
    }
}

// Quantas células o robô andou no último trecho. Com parede na frente ao alcance no
// começo e no fim, o sensor da frente responde melhor que os encoders (que escorregam
// ou podem falhar); sem ela, vale o encoder.
static int celulas_no_trecho() {
    uint16_t frente_fim = distancias.frente;
    if (frente_inicio_celula <= ALCANCE_CONTAGEM_MM && frente_fim != DISTANCIA_INVALIDA) {
        float andou_mm = (float)frente_inicio_celula - (float)frente_fim;
        return max(0L, lroundf(andou_mm / TAMANHO_CELULA_MM));
    }
    return contagem_andada() >= contagens_por_celula / 2 ? 1 : 0;
}

// Robô parado depois de andar uma célula: atualiza a posição e decide.
static void chegou_na_celula() {
    int celulas = celulas_no_trecho();
    if (celulas == 0) {
        // Parou pela parede quase sem sair do lugar: ela estava logo na frente e não foi
        // vista antes de virar. O robô continua na mesma célula; o decidir() marca essa
        // parede e escolhe outro caminho. Sem isso, a posição avançaria sem o robô sair
        // do lugar e o mapa ficaria todo deslocado.
        Serial.printf("Parede logo na frente (%u mm): continua em (%d,%d).\n",
                      distancias.frente, pos_x, pos_y);
        decidir();
        return;
    }
    if (celulas > 1) {
        // Passou de células sem parar (encoder com problema): a posição acompanha o que
        // o sensor da frente mediu. As paredes laterais dessas células ficam sem marcar.
        Serial.printf("Andou %d células sem parar: confira os encoders.\n", celulas);
    } else {
        ajustar_freio();
        calibrar_celula();
    }
    for (int i = 0; i < celulas; i++) {
        avancar(direcao, pos_x, pos_y);
        celulas_andadas++;
    }
    if (!labirinto.dentro(pos_x, pos_y)) {
        encerrar(FIM_FORA_DO_MAPA, ERRO,
                 "Posição fora do mapa: confira LARGURA, ALTURA e LABIRINTO_A_DIREITA.");
        return;
    }
    decidir();
}

void setup() {
    // A ponte H só liga depois do start.
    motores_iniciar(MOTOR_ESQUERDO, MOTOR_DIREITO);

    pinMode(BOTAO_BOOT, INPUT_PULLUP);
    pinMode(Pinos::LED_STATUS, OUTPUT);

    Serial.begin(115200);
    delay(1000);

    // O registro da última corrida aparece antes de tudo: assim ele é lido só com a
    // USB, mesmo que os sensores não respondam sem a bateria. "r" mostra de novo.
    memoria.begin("explorar", false);
    if (memoria.getBytesLength("log") > 0) {
        imprimir_registro();
    }
    // Volta com a calibração da última corrida, se ela estiver dentro da faixa.
    int32_t salvo = memoria.getInt("celula", CONTAGENS_POR_CELULA);
    if (fabsf(salvo - CONTAGENS_POR_CELULA) <=
        VARIACAO_MAXIMA_CALIBRACAO * CONTAGENS_POR_CELULA) {
        contagens_por_celula = salvo;
    }
    Serial.printf("Contagens por célula: %ld\n", (long)contagens_por_celula);

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
    Serial.printf("Pronto para o labirinto %dx%d. Aperte BOOT ou envie \"s\".\n", LARGURA,
                  ALTURA);
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
        if (estado == AGUARDANDO || estado == CHEGOU || estado == ERRO) {
            // Recomeça do start, mas guarda o mapa: numa segunda corrida o robô já
            // conhece as paredes e vai pelo caminho mais curto.
            pos_x = 0;
            pos_y = 0;
            direcao = NORTE;
            celulas_andadas = 0;
            total_registros = 0;
            memoria.remove("log");
            inicio_corrida = millis();
            if (!bateria_ok()) {
                // Sem USB, o LED piscando rápido e o registro mostram o motivo.
                encerrar(FIM_BATERIA, ERRO, "Bateria abaixo de 6,6 V. Recarregue antes de usar.");
                return;
            }
            rumo = angulo;
            medindo_curva = false;
            segunda_metade_180 = 0.0f;
            motores_habilitar(true);
            digitalWrite(Pinos::LED_STATUS, HIGH);
            decidir();
        } else {
            encerrar(FIM_BOTAO, AGUARDANDO,
                     "Parado pelo botão. Aperte de novo para recomeçar do start.");
        }
        return;
    }

    switch (estado) {
    case AGUARDANDO:
        break;
    case CHEGOU:
        // LED piscando devagar: objetivo atingido.
        digitalWrite(Pinos::LED_STATUS, (millis() / 500) % 2);
        break;
    case ERRO:
        // LED piscando rápido: parou por erro. O motivo fica no registro ("r").
        digitalWrite(Pinos::LED_STATUS, (millis() / 100) % 2);
        break;
    case ANDANDO:
        andar();
        break;
    case VIRANDO:
        virar();
        break;
    case ALINHANDO:
        alinhar();
        break;
    case PARADO_ALINHADO:
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            comecar_curva(curva_pendente);
        }
        break;
    case PARADO_CELULA:
        // Espera o robô assentar e os sensores medirem de novo antes de ler as paredes.
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            if (!bateria_ok()) {
                encerrar(FIM_BATERIA, ERRO, "Bateria abaixo de 6,6 V. Teste encerrado.");
            } else {
                chegou_na_celula();
            }
        }
        break;
    case PARADO_CURVA:
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            ajustar_compensacao();
            if (segunda_metade_180 != 0.0f) {
                float graus = segunda_metade_180;
                segunda_metade_180 = 0.0f;
                preparar_curva(graus);
            } else {
                direcao = direcao_alvo;
                comecar_celula();
            }
        }
        break;
    }
}
