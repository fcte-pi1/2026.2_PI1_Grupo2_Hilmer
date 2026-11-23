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
// saída do beco. Antes de gravar, ajustar LARGURA e ALTURA. O lado em que o labirinto
// continua é descoberto sozinho (LADO_CONFIGURADO).
// Funciona sem a USB: o LED acende quando o robô está pronto, e o botão BOOT da placa
// (ou "s" pelo monitor serial) começa e para. Pelo monitor, "m" mostra o mapa e "c"
// zera a calibração das contagens por célula.
//
// LED no fim: piscando devagar, chegou ao objetivo; piscando rápido, parou por erro.
// Com o env exemplo_explorar_wifi, também manda a telemetria pelo Wi-Fi ao backend
// (config/credenciais.h), numa tarefa no núcleo 0: o controle segue no núcleo 1 e não
// espera pela rede.
// Registro sem USB e sem cartão SD: cada célula fica gravada na flash da ESP, que não
// se apaga ao desligar. Depois da corrida, desligar a bateria, soltar o fio do 5V,
// ligar a USB, abrir o monitor e enviar "r".
//
// O robô precisa ficar parado no chão enquanto o LED pisca (calibração do giroscópio).

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_system.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "motores/motores.h"
#include "navegacao/bateria.h"
#include "navegacao/centralizacao.h"
#include "navegacao/contagem_celulas.h"
#include "navegacao/deteccao_lado.h"
#include "navegacao/equilibrio_rodas.h"
#include "navegacao/espelho.h"
#include "navegacao/floodfill.h"
#include "navegacao/labirinto.h"
#include "navegacao/passagens.h"
#include "navegacao/travamento.h"
#include "navegacao/pid.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"
#include "sensores/imu.h"

#ifdef EXPLORAR_WIFI
#include "comunicacao/comunicacao.h"
#include "navegacao/telemetria.h"
#endif

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;
constexpr uint32_t PERIODO_MS = 10;

// Cada VL53L0X mede a cada ~20 ms. Sem medida nova por mais que isso, o sensor caiu
// (I2C, fio ou alimentação). Sem esse teste, um sensor que não iniciou fica em 65535,
// que o robô lê como "sem parede": foi assim que uma corrida andou cega pelo mapa todo.
constexpr uint32_t IDADE_MAXIMA_DISTANCIA_MS = 200;
// A inicialização dos VL53L0X às vezes falha num boot e funciona no seguinte (o da
// direita não tem XSHUT e pode ficar num estado ruim). Tenta algumas vezes.
constexpr int TENTATIVAS_INICIAR_DISTANCIA = 3;
constexpr uint32_t TEMPO_TENTAR_IMU_MS = 500;
constexpr uint32_t TEMPO_TENTAR_SENSOR_MS = 1000;  // sensor sem medida no meio da corrida
constexpr uint32_t TEMPO_TESTE_GIRO_MS = 600;  // comando "g": giro curto no lugar

// Tamanho do labirinto, visto do start: ALTURA é o número de células para a frente
// do robô e LARGURA para o lado em que o labirinto continua. Por enquanto a ALTURA
// vai até 4 (LABIRINTO_MAX_ALTURA): o robô sai virado para o lado curto.
constexpr int LARGURA = 4;
constexpr int ALTURA = 4;
// Lado em que o labirinto continua a partir do start. LADO_DESCONHECIDO: o robô descobre
// sozinho (navegacao/deteccao_lado): a borda externa nunca tem abertura, então a primeira
// abertura lateral da primeira coluna mostra o lado. Até lá ele só anda reto, que é o
// único caminho dessa coluna. LADO_DIREITA ou LADO_ESQUERDA forçam o lado, sem detecção.
// Simulado em test_simulacao_explorar: 4x4, 8x4 e 12x4, com start nos dois cantos.
constexpr LadoLabirinto LADO_CONFIGURADO = LADO_DESCONHECIDO;
// Usado só se nenhuma abertura for confirmada até a parede no fim da primeira coluna.
constexpr LadoLabirinto LADO_PADRAO = LADO_ESQUERDA;
// Amostras das laterais em cada célula da primeira coluna, espaçadas para o VL53L0X ter
// medida nova (~20 ms cada). Tolera uma amostra errada de cada lado.
constexpr int AMOSTRAS_LADO = 5;
constexpr uint32_t INTERVALO_AMOSTRA_LADO_MS = 30;
// Com parede na frente e o lado ainda desconhecido, a abertura tem de estar ali: relê
// antes de escolher pelo padrão.
constexpr int RELEITURAS_SEM_SAIDA = 3;

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
// Subiu de 0,30/0,38 e depois de 0,33/0,40: nas corridas a roda parava no ajuste e a
// ponte H ficava apitando.
constexpr float VELOCIDADE_ALINHAMENTO_MIN = 0.28f;
constexpr float VELOCIDADE_ALINHAMENTO_MAX = 0.34f;
// Parado, o N20 não sai do lugar com a velocidade do ajuste: no começo ele recebe
// VELOCIDADE_ARRANQUE por um instante, mais curto que o da reta para não passar do ponto.
constexpr uint32_t TEMPO_IMPULSO_ALINHAMENTO_MS = 60;
constexpr uint32_t TEMPO_MAX_ALINHAMENTO_MS = 1500;
// Trava de segurança: um labirinto 4x4 não precisa de tantas células.
// No dia da apresentação ninguém pode mexer no robô durante a corrida: ele não desiste
// sozinho (curva travada, mapa sem caminho, sensor sem medida) e só para no objetivo, pela
// bateria baixa ou pelo BOOT. Este limite é só uma trava contra laço infinito.
constexpr int MAX_CELULAS = 400;

// Valor calibrado no exemplo_celula. É só o valor inicial: o robô se recalibra pela
// parede da frente (ver calibrar_celula) e guarda o valor na flash entre corridas.
constexpr int32_t CONTAGENS_POR_CELULA = 1350;
constexpr float TAMANHO_CELULA_MM = 180.0f;
// A calibração só usa a parede da frente até essa distância: mais longe, o VL53L0X
// erra mais e a medida piora em vez de melhorar.
constexpr uint16_t ALCANCE_CALIBRACAO_MM = 500;
// Uma medida que dá mais de 10% longe do valor inicial é descartada (roda escorregou,
// leitura ruim): o valor calibrado nunca sai dessa faixa. 10% são 18 mm por célula;
// com 25%, leituras ruins dos sensores levavam o robô a parar entre células.
constexpr float VARIACAO_MAXIMA_CALIBRACAO = 0.10f;
constexpr int32_t COMPENSACAO_FREIO_CONTAGENS = 100;
constexpr int32_t COMPENSACAO_FREIO_MAXIMA = 400;

// Valores calibrados no exemplo_parede_curva.
constexpr uint16_t LIMIAR_FRENTE_MM = 75;
constexpr uint16_t LIMIAR_LATERAL_MM = 100;
// Leitura de cada sensor lateral com o robô no centro do corredor (27,7 mm entre cada
// lateral e a parede, medido com régua; média de 3 x 50 leituras, sensor da esquerda já
// trocado). Os dois leem diferente (posição no chassi e offset de cada VL53L0X). Com um
// valor só para os dois lados (50 mm), o robô ficava fora do centro: ~10 mm para a
// direita com parede dos dois lados e ~24 mm com parede só à esquerda (simulado).
// A ±10 mm do centro a esquerda acompanha o movimento; a direita, perto da parede,
// responde só ~metade (41,5 mm em vez de ~36), então a correção desse lado é mais fraca.
constexpr float CENTRO_ESQUERDA_MM = 22.0f;
constexpr float CENTRO_DIREITA_MM = 46.0f;
// Simulado no corredor (com ruído de até 4 mm): 0,3 a 0,8 ficam estáveis; 0,5 corrige
// mais rápido que 0,3 e deixa margem para atrasos que a simulação não pega.
constexpr float GANHO_CENTRO = 0.5f;            // graus por mm
constexpr float INCLINACAO_CENTRO_MAX = 10.0f;  // graus
constexpr float FILTRO_CENTRO = 0.2f;
// Histórico: 0,30 / 0,32 / 0,24 -> 0,34 / 0,36 / 0,28 -> 0,40 / 0,45 / 0,34, quando as
// curvas travavam (bateria a 7,2 V e o MPU6050 com contato ruim). Com a bateria cheia e o
// MPU firme, 0,45 girava 190 graus em 0,6 s (comando "g"): rápido demais para parar no
// ângulo.
// 23/11, robô com a fiação refeita, bateria a 8,2 V e MPU firme: com 0,34 / 0,38 / 0,30
// ainda rápido demais. A curva passava do ponto e o robô parava fora do centro, e os
// sensores pegavam a quina das paredes (lado aberto lido como parede a 75 e 93 mm), o que
// fechava o caminho no mapa. Mais devagar, a centralização e o rumo têm tempo de corrigir.
// O impulso de arranque (VELOCIDADE_ARRANQUE) continua tirando a roda da inércia. Se a
// curva voltar a dar "curva travou", subir VELOCIDADE_CURVA_MIN.
constexpr float VELOCIDADE_RETA = 0.26f;
constexpr float VELOCIDADE_CURVA_MAX = 0.30f;
constexpr float VELOCIDADE_CURVA_MIN = 0.25f;
constexpr float COMPENSACAO_CURVA_GRAUS = 13.0f;
constexpr float COMPENSACAO_MAXIMA_GRAUS = 45.0f;
// Ângulo pedido ao giroscópio em cada curva de 90°, um por lado: os dois sentidos podem
// sair diferentes, e numa escada de curvas (esquerda, esquerda, direita...) os erros se
// somam. Medir com o exemplo_calibrar_giro e copiar o valor de cada lado aqui.
constexpr float ANGULO_CURVA_ESQUERDA_GRAUS = 85.0f;
constexpr float ANGULO_CURVA_DIREITA_GRAUS = 85.0f;
// Equilíbrio das rodas na curva: o motor esquerdo gira 6% a 8% mais rápido que o direito
// com o mesmo duty, e o centro de giro escorrega (~3 mm por curva; simulado: com ganho
// 0,01 por contagem, ~0,2 mm). Se um encoder parar de contar, o equilíbrio desliga.
constexpr float GANHO_EQUILIBRIO = 0.01f;     // fração da velocidade por contagem de diferença
constexpr float EQUILIBRIO_MAXIMO = 0.2f;
// Curva travada (ponta na parede): dá uma ré curta e tenta de novo, até esse tanto de vezes.
// Depois disso para com erro: seguir com a curva incompleta desalinha o mapa.
constexpr int32_t RECUO_CONTAGENS = 200;      // ~25 mm
constexpr float VELOCIDADE_RECUO = 0.30f;
constexpr uint32_t TEMPO_MAX_RECUO_MS = 800;
// Travamento na reta (encostado numa parede ou preso numa quina): sem andar
// PROGRESSO_MINIMO_RETA contagens em JANELA_TRAVAMENTO_MS, dá ré curta e retoma a célula.
// Andando normal são ~400 a 500 contagens nessa janela. Logo depois de sair do repouso
// não conta (CARENCIA_TRAVAMENTO_MS). Já perto do fim da célula, considera que chegou.
constexpr uint32_t JANELA_TRAVAMENTO_MS = 400;
constexpr int32_t PROGRESSO_MINIMO_RETA = 60;
constexpr uint32_t CARENCIA_TRAVAMENTO_MS = 300;
constexpr float FRACAO_CHEGOU_TRAVADO = 0.75f;
constexpr uint32_t TEMPO_PARADO_MS = 500;
constexpr float QUEDA_L298N_V = 2.0f;
// Impulso para tirar a roda da inércia no começo de cada movimento. Era 0,60 por 200 ms:
// numa curva de 90° (~0,5 s) o impulso, o dobro da velocidade de curva, dominava o giro e
// o robô passava do ângulo; voltando devagar, sem impulso, a roda não vencia o atrito e
// a curva estourava o tempo ("curva travou").
constexpr float VELOCIDADE_ARRANQUE = 0.45f;
constexpr uint32_t TEMPO_ARRANQUE_MS = 80;
constexpr uint32_t TEMPO_MAX_CURVA_MS = 3000;
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
                                          Pinos::MOTOR_ESQ_IN2, Pinos::MOTOR_ESQ_INVERTIDO};
static const PinosMotor MOTOR_DIREITO = {Pinos::MOTOR_DIR_PWM, Pinos::MOTOR_DIR_IN1,
                                         Pinos::MOTOR_DIR_IN2, Pinos::MOTOR_DIR_INVERTIDO};
static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, Pinos::ENC_ESQ_INVERTIDO};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, Pinos::ENC_DIR_INVERTIDO};
static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

// Correção do rumo na reta: 0,02 de diferença de velocidade por grau de desvio.
static Pid pid_rumo(0.02f, 0.0f, 0.001f, PERIODO_MS / 1000.0f, 0.2f);

static Labirinto labirinto(LARGURA, ALTURA);
static FloodFill floodfill(labirinto);
static DeteccaoLado deteccao(ALTURA);
// Passagens por onde o robô já andou: com parede logo na frente numa delas, ele está mais
// adiante do que contou (navegacao/passagens).
static Passagens passagens(LARGURA, ALTURA);
// Lado em uso no mapa. Enquanto a detecção não termina, só a primeira coluna (x = 0) é
// usada, e nela o lado não muda nada além das laterais, que ficam guardadas na detecção.
static bool labirinto_a_direita = (LADO_CONFIGURADO != LADO_DESCONHECIDO ? LADO_CONFIGURADO
                                                                          : LADO_PADRAO) ==
                                  LADO_DIREITA;

enum Estado {
    AGUARDANDO,
    ANDANDO,
    VIRANDO,
    ALINHANDO,        // ajusta a distância até a parede da frente antes de virar
    PARADO_CELULA,    // parado no centro de uma célula: lê paredes e decide
    PARADO_ALINHADO,  // parado depois do alinhamento: faz a curva pendente
    PARADO_CURVA,     // parado depois de uma curva: anda a célula (ou a 2ª metade do 180°)
    RECUANDO,         // a curva travou: ré curta antes de tentar de novo
    PARADO_RECUO,     // parado depois da ré: retoma a curva
    RECUANDO_RETA,    // travou andando para a frente (parede ou quina): ré curta
    PARADO_RECUO_RETA,  // parado depois da ré da reta: volta a andar a mesma célula
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
// Rumo que o robô deve ter: soma dos ângulos das curvas a partir do start.
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
static int tentativas_curva = 0;
static bool equilibrio_desligado = false;
static uint32_t inicio_recuo = 0;
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
    return lado_no_mapa(direcao, passos_direita, labirinto_a_direita);
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
    FIM_CURVA_TRAVOU,
    FIM_SENSOR,
    SENSOR_RECUPERADO,
    POSICAO_CORRIGIDA,
    MAPA_ESQUECIDO,
    RETA_TRAVOU,
};
static const char *NOMES_CODIGO[] = {
    "segue", "vira à direita", "volta (180)", "vira à esquerda",
    "FIM: objetivo atingido", "FIM: sem caminho até o objetivo",
    "FIM: posição fora do mapa", "FIM: células demais", "FIM: parado pelo botão",
    "FIM: bateria baixa", "curva travou", "FIM: curva travou de novo",
    "FIM: sensor de distância sem medida", "sensor sem medida: reiniciado, continua",
    "posição corrigida pela parede", "sem caminho: esqueceu as paredes e segue",
    "reta travou: ré e segue",
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

// Ao largar, a corrida que estava no registro vai para "anterior", se ela chegou a
// andar (mais de um registro). Assim apertar BOOT de novo depois de uma corrida, ou uma
// largada que para na hora, não apaga o registro da corrida que interessa.
static void guardar_corrida_anterior() {
    size_t tamanho = memoria.getBytesLength("log");
    if (tamanho <= sizeof(RegistroCelula) || tamanho > sizeof(registros)) {
        return;
    }
    RegistroCelula lidos[MAX_REGISTROS];
    memoria.getBytes("log", lidos, tamanho);
    memoria.putBytes("anterior", lidos, tamanho);
}

static void imprimir_registro(const char *chave = "log", const char *titulo = "Última corrida") {
    size_t tamanho = memoria.getBytesLength(chave);
    int n = tamanho / sizeof(RegistroCelula);
    if (n > MAX_REGISTROS) {
        n = MAX_REGISTROS;
    }
    RegistroCelula lidos[MAX_REGISTROS];
    memoria.getBytes(chave, lidos, n * sizeof(RegistroCelula));
    Serial.printf("\n===== %s: %d registros =====\n", titulo, n);
    Serial.println("  t(s)  célula  virado   paredes F D E   frente  direita esquerda  ação");
    for (int i = 0; i < n; i++) {
        const RegistroCelula &r = lidos[i];
        Serial.printf("%6.1f  (%d,%d)   %-6s     %c %c %c    %5u   %5u   %5u    %s\n",
                      r.t_ds / 10.0f, r.x, r.y, NOMES_DIRECAO[r.direcao & 3],
                      (r.paredes & 1) ? 'X' : '.', (r.paredes & 2) ? 'X' : '.',
                      (r.paredes & 4) ? 'X' : '.', r.frente_mm, r.direita_mm, r.esquerda_mm,
                      r.codigo <= RETA_TRAVOU ? NOMES_CODIGO[r.codigo] : "?");
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
                  estado == PARADO_CURVA || estado == PARADO_ALINHADO || estado == PARADO_RECUO ||
                  estado == PARADO_RECUO_RETA;
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
    giro_z = (Pinos::IMU_INVERTIDA ? -giro_z : giro_z) - erro_zero_giro;

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
static float ultima_tensao = 0.0f;  // para a telemetria (o I2C só é lido aqui)

static bool bateria_ok() {
    float tensao, corrente;
    if (!energia_ler(tensao, corrente)) {
        return true;  // sem INA219, segue com o duty máximo padrão
    }
    ultima_tensao = tensao;
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
static void imprimir_falhas(uint8_t falhas);

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
        } else if (comando == 'p') {
            imprimir_registro("anterior", "Corrida anterior");
        } else if (comando == 'c') {
            // Volta a calibração da célula para o valor medido no exemplo_celula.
            contagens_por_celula = CONTAGENS_POR_CELULA;
            memoria.remove("celula");
            Serial.printf("Calibração zerada: %ld contagens por célula.\n",
                          (long)contagens_por_celula);
        } else if (comando == 'g') {
            // Teste do sentido do giroscópio com o robô no chão, parado no "Pronto": gira no
            // lugar para a esquerda (roda esquerda para trás, direita para a frente) por
            // TEMPO_TESTE_GIRO_MS. Os encoders confirmam o sentido físico; o ângulo tem de subir.
            if (estado == AGUARDANDO || estado == CHEGOU || estado == ERRO) {
                float angulo_antes = angulo;
                encoders_zerar();
                motores_habilitar(true);
                inicio_movimento = millis();
                uint32_t inicio_teste = millis();
                while (millis() - inicio_teste < TEMPO_TESTE_GIRO_MS) {
                    atualizar_angulo();
                    comandar(-VELOCIDADE_CURVA_MAX, VELOCIDADE_CURVA_MAX);
                    delay(PERIODO_MS);
                }
                motores_frear();
                uint32_t inicio_parada = millis();
                while (millis() - inicio_parada < TEMPO_ASSENTAR_MS) {
                    atualizar_angulo();
                    delay(PERIODO_MS);
                }
                motores_habilitar(false);
                int32_t esquerda = encoders_contagem_esquerda();
                int32_t direita = encoders_contagem_direita();
                float giro = angulo - angulo_antes;
                Serial.printf("Teste do giro para a esquerda: ângulo %+.1f graus, encoders %ld / %ld.\n",
                              giro, (long)esquerda, (long)direita);
                if (esquerda < 0 && direita > 0) {
                    Serial.println(giro > 0 ? "Giroscópio certo: girou para a esquerda e o ângulo subiu."
                                            : "Giroscópio INVERTIDO: girou para a esquerda e o ângulo "
                                              "desceu. Trocar Pinos::IMU_INVERTIDA.");
                } else {
                    Serial.println("As rodas não giraram como esperado (esquerda para trás, direita "
                                   "para a frente): confira os motores antes do giroscópio.");
                }
                angulo = angulo_antes + giro;
                rumo = angulo;
            }
        } else if (comando == 'a') {
            // Ângulo integrado pelo giroscópio (já com Pinos::IMU_INVERTIDA): girando o robô à mão
            // para a esquerda, tem de aumentar; para a direita, diminuir.
            Serial.printf("[%lu s] ângulo %.1f graus (zero do giro %.3f graus/s)\n",
                          millis() / 1000, angulo, erro_zero_giro);
        } else if (comando == 'd') {
            // Estado dos sensores de distância, sem ligar os motores.
            uint8_t falhas = distancia_falhas(IDADE_MAXIMA_DISTANCIA_MS);
            Serial.printf("[%lu s] frente %u, esquerda %u, direita %u mm. Encoders %ld / %ld. ",
                          millis() / 1000, distancias.frente, distancias.esquerda,
                          distancias.direita, (long)encoders_contagem_esquerda(),
                          (long)encoders_contagem_direita());
            if (falhas == 0) {
                Serial.println("Os três medindo.");
            } else {
                imprimir_falhas(falhas);
            }
        } else if (comando == 'm') {
            floodfill.calcular();
            imprimir_mapa();
        }
    }
    return apertou;
}

// Inclinação do rumo, em graus, para levar o robô ao centro do corredor pelas paredes
// laterais. Positivo: perto demais da parede da direita, então inclina para a esquerda.
static DetectorTravamento travamento_reta(JANELA_TRAVAMENTO_MS, PROGRESSO_MINIMO_RETA,
                                          CARENCIA_TRAVAMENTO_MS);
static int32_t contagem_inicio_recuo_reta = 0;
// O trecho desta célula travou: a contagem e a frente não servem para calibrar o freio
// nem as contagens por célula.
static bool trecho_travou = false;

static float inclinacao_centro() {
    static const CalibracaoCentro CENTRO = {CENTRO_ESQUERDA_MM, CENTRO_DIREITA_MM,
                                            LIMIAR_LATERAL_MM, GANHO_CENTRO,
                                            INCLINACAO_CENTRO_MAX};
    return centralizacao_inclinacao(distancias.esquerda, distancias.direita, CENTRO);
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
    travamento_reta.reiniciar(millis(), 0);
    trecho_travou = false;
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
    if (travamento_reta.atualizar(millis(), contagem_andada())) {
        Serial.printf("Reta travou em %ld de %ld contagens (frente %u, direita %u, esquerda %u).\n",
                      (long)contagem_andada(), (long)contagens_por_celula, distancias.frente,
                      distancias.direita, distancias.esquerda);
        registrar(RETA_TRAVOU, 0);
        trecho_travou = true;
        if (contagem_andada() >= FRACAO_CHEGOU_TRAVADO * contagens_por_celula) {
            // Quase no fim da célula: em vez de brigar com a quina, considera que chegou.
            contagem_freio = contagem_andada();
            parou_pela_parede = false;
            parar(PARADO_CELULA);
            return;
        }
        // Ré curta sem zerar os encoders: a contagem da célula desconta a ré sozinha.
        motores_frear();
        contagem_inicio_recuo_reta = contagem_andada();
        inicio_recuo = millis();
        inicio_movimento = millis();
        estado = RECUANDO_RETA;
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
static void encerrar(CodigoRegistro codigo, Estado final, const char *motivo);
static void esquecer_mapa(const char *motivo);

// Começa (ou retoma, depois de uma ré) a curva até o rumo atual.
static void retomar_curva() {
    encoders_zerar();  // o equilíbrio compara as rodas desde o começo da curva
    inicio_curva = millis();
    inicio_movimento = millis();
    estado = VIRANDO;
}

static void comecar_curva(float graus) {
    rumo += graus;
    tentativas_curva = 0;
    retomar_curva();
}

// Correção do equilíbrio das rodas na curva. Com um encoder parado e o outro contando
// (fio solto), o equilíbrio jogaria toda a velocidade numa roda só: aí ele desliga.
static float correcao_equilibrio() {
    int32_t esquerda = encoders_contagem_esquerda();
    int32_t direita = encoders_contagem_direita();
    int32_t me = abs(esquerda), md = abs(direita);
    int32_t maior = max(me, md), menor = min(me, md);
    if (!equilibrio_desligado && maior >= 100 && menor * 10 < maior) {
        equilibrio_desligado = true;
        Serial.printf("Equilíbrio das rodas desligado: encoder parado (esquerda %ld, direita %ld).\n",
                      (long)esquerda, (long)direita);
    }
    if (equilibrio_desligado) {
        return 0.0f;
    }
    return equilibrio_rodas(esquerda, direita, GANHO_EQUILIBRIO, EQUILIBRIO_MAXIMO);
}

static void virar() {
    static float sentido_anterior = 0.0f;
    float falta = rumo - angulo;
    float sentido = falta > 0 ? 1.0f : -1.0f;
    if (millis() - inicio_curva < PERIODO_MS * 2) {
        sentido_anterior = sentido;  // começo da curva (ou retomada depois da ré)
    } else if (sentido != sentido_anterior) {
        // Passou do ângulo e vai voltar: a roda está quase parada, então recebe o
        // impulso de arranque de novo para vencer o atrito.
        inicio_movimento = millis();
        sentido_anterior = sentido;
    }
    float limite = compensacao[sentido > 0 ? 0 : 1];
    if (fabsf(falta) <= limite) {
        angulo_freio = angulo;
        sentido_curva = sentido;
        medindo_curva = true;
        parar(PARADO_CURVA);
        return;
    }
    if (millis() - inicio_curva > TEMPO_MAX_CURVA_MS) {
        Serial.printf("Curva travou faltando %.1f graus (tentativa %d).\n", falta,
                      tentativas_curva + 1);
        registrar(CURVA_TRAVOU, 0);
        // Ré curta para soltar a ponta da parede e nova tentativa, sem limite: o rumo alvo
        // continua o mesmo (antes parava depois de 2 tentativas, com FIM_CURVA_TRAVOU).
        tentativas_curva++;
        motores_frear();
        encoders_zerar();
        inicio_recuo = millis();
        inicio_movimento = millis();
        estado = RECUANDO;
        return;
    }
    // Desacelera perto do fim para não passar do ângulo.
    float velocidade = constrain(fabsf(falta) * 0.01f, VELOCIDADE_CURVA_MIN, VELOCIDADE_CURVA_MAX);
    // A roda atrasada recebe um pouco mais de velocidade (a adiantada não é freada).
    float fe, fd;
    equilibrio_fatores(correcao_equilibrio(), fe, fd);
    if (falta > 0) {
        comandar(-velocidade * fe, velocidade * fd);  // gira para a esquerda
    } else {
        comandar(velocidade * fe, -velocidade * fd);  // gira para a direita
    }
}

// Ré curta depois de uma curva travada.
static void recuar_reta() {
    int32_t recuou = contagem_inicio_recuo_reta - contagem_andada();
    if (recuou >= RECUO_CONTAGENS || millis() - inicio_recuo > TEMPO_MAX_RECUO_MS) {
        parar(PARADO_RECUO_RETA);
        return;
    }
    comandar(-VELOCIDADE_RECUO, -VELOCIDADE_RECUO);
}

// Depois da ré da reta: volta a andar a mesma célula, com o impulso de arranque. Com
// espaço, a centralização afasta o robô da parede.
static void retomar_reta() {
    pid_rumo.zerar();
    inicio_movimento = millis();
    travamento_reta.reiniciar(millis(), contagem_andada());
    estado = ANDANDO;
}

static void recuar() {
    int32_t andou = max(abs(encoders_contagem_esquerda()), abs(encoders_contagem_direita()));
    if (andou >= RECUO_CONTAGENS || millis() - inicio_recuo > TEMPO_MAX_RECUO_MS) {
        parar(PARADO_RECUO);
        return;
    }
    comandar(-VELOCIDADE_RECUO, -VELOCIDADE_RECUO);
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
// Sensores sem medida, para o registro do FIM_SENSOR (bits do driver de distância).
static uint8_t falhas_distancia = 0;

// Converte as falhas do driver para as colunas "F D E" do registro.
static uint8_t falhas_para_colunas(uint8_t falhas) {
    return ((falhas & DISTANCIA_FALHA_FRENTE) ? 1 : 0) |
           ((falhas & DISTANCIA_FALHA_DIREITA) ? 2 : 0) |
           ((falhas & DISTANCIA_FALHA_ESQUERDA) ? 4 : 0);
}

static void imprimir_falhas(uint8_t falhas) {
    Serial.printf("Sensores sem medida:%s%s%s\n",
                  (falhas & DISTANCIA_FALHA_FRENTE) ? " frente" : "",
                  (falhas & DISTANCIA_FALHA_ESQUERDA) ? " esquerda" : "",
                  (falhas & DISTANCIA_FALHA_DIREITA) ? " direita" : "");
}

// Inicia os sensores de distância e espera a primeira medida de cada um. "completo":
// inicia os três do zero (boot); senão, tenta de novo só os que estão sem medida, sem
// mexer nos que funcionam. Retorna as falhas que sobraram (0 = os três medindo).
static uint8_t esperar_medidas() {
    uint32_t inicio = millis();
    uint8_t falhas;
    do {
        distancia_ler(distancias);
        delay(5);
        falhas = distancia_falhas(IDADE_MAXIMA_DISTANCIA_MS);
    } while (falhas != 0 && millis() - inicio < 2 * IDADE_MAXIMA_DISTANCIA_MS);
    return falhas;
}

static uint8_t iniciar_distancia(bool completo) {
    if (completo) {
        distancia_iniciar(Wire, PINOS_DISTANCIA);
    }
    uint8_t falhas = esperar_medidas();
    for (int tentativa = 0; falhas != 0 && tentativa < TENTATIVAS_INICIAR_DISTANCIA; tentativa++) {
        imprimir_falhas(falhas);
        Serial.println("Tentando de novo só esses.");
        distancia_reiniciar(Wire, PINOS_DISTANCIA, falhas);
        falhas = esperar_medidas();
    }
    if (falhas != 0) {
        imprimir_falhas(falhas);
    }
    return falhas;
}

// ---------------------------------------------------------------------------
// Histórico dos últimos boots na flash: motivo do reinício e se o Wi-Fi conectou.
// Sem a USB não há serial; isso mostra depois, pela USB, se na bateria a placa ficou
// reiniciando (ex.: queda de tensão nos picos do rádio) ou se só não achou a rede.
constexpr int BOOTS_GUARDADOS = 8;
struct __attribute__((packed)) RegistroBoot {
    uint8_t motivo;      // esp_reset_reason_t
    uint8_t chegou;      // 0 = não terminou o setup, 1 = pronto, 2 = Wi-Fi conectou
    uint16_t segundos;   // quanto tempo ficou ligado (atualizado de tempos em tempos)
};
static RegistroBoot boots[BOOTS_GUARDADOS];
static volatile uint8_t etapa_boot = 0;

static const char *nome_motivo(uint8_t motivo) {
    switch (motivo) {
    case ESP_RST_POWERON: return "ligou (energia)";
    case ESP_RST_EXT: return "pino EN";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "travou (panic)";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "QUEDA DE TENSÃO (brownout)";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    default: return "outro";
    }
}

// No começo do setup: imprime o histórico e abre um registro novo para este boot.
static void comecar_registro_boot() {
    memset(boots, 0, sizeof(boots));
    memoria.getBytes("boots", boots, sizeof(boots));
    Serial.println("Últimos boots (mais recente primeiro): motivo, até onde chegou, tempo ligado");
    for (int i = 0; i < BOOTS_GUARDADOS; i++) {
        const RegistroBoot &b = boots[i];
        if (b.motivo == 0 && b.segundos == 0 && b.chegou == 0) {
            continue;
        }
        static const char *ETAPAS[] = {"não terminou o setup", "pronto, sem Wi-Fi",
                                       "Wi-Fi conectou"};
        Serial.printf("  %d. %-28s %-22s %u s\n", i + 1, nome_motivo(b.motivo),
                      ETAPAS[b.chegou < 3 ? b.chegou : 0], b.segundos);
    }
    memmove(&boots[1], &boots[0], sizeof(RegistroBoot) * (BOOTS_GUARDADOS - 1));
    boots[0] = {(uint8_t)esp_reset_reason(), 0, 0};
    memoria.putBytes("boots", boots, sizeof(boots));
}

// Chamado no loop(): grava a etapa e o tempo ligado a cada 5 s (ou quando a etapa muda).
static void atualizar_registro_boot() {
    static uint32_t ultimo = 0;
    static uint8_t etapa_gravada = 0;
    if (etapa_boot == etapa_gravada && millis() - ultimo < 5000) {
        return;
    }
    ultimo = millis();
    etapa_gravada = etapa_boot;
    boots[0].chegou = etapa_boot;
    boots[0].segundos = millis() / 1000 > 65535 ? 65535 : millis() / 1000;
    // Não grava durante a corrida: escrever na flash pode atrasar o controle.
    if (estado == AGUARDANDO || estado == CHEGOU || estado == ERRO) {
        memoria.putBytes("boots", boots, sizeof(boots));
    }
}

#ifdef EXPLORAR_WIFI
// ---------------------------------------------------------------------------
// Telemetria pelo Wi-Fi. O loop() (núcleo 1) só copia o estado numa estrutura
// protegida; a tarefa do Wi-Fi (núcleo 0) monta o JSON e envia. Conectar e reconectar
// pode demorar mais de 1 s, e o controle não pode esperar por isso.
constexpr uint32_t PERIODO_TELEMETRIA_MS = 100;  // 10 pacotes por segundo
constexpr uint32_t PERIODO_TAREFA_WIFI_MS = 10;
constexpr uint32_t PILHA_WIFI = 8192;
constexpr BaseType_t NUCLEO_WIFI = 0;
constexpr const char *ROBOT_ID = "mm-01";
// Campo "maze" do RUN_START: o backend abre a corrida nesse labirinto e a web desenha a
// grade com esse tamanho (sem o campo, os dois assumem 4x4).
static_assert(ALTURA == 4 && (LARGURA == 4 || LARGURA == 8 || LARGURA == 12),
              "o contrato só tem os labirintos 4x4, 8x4 e 12x4");
constexpr const char *NOME_LABIRINTO = LARGURA == 4 ? "4x4" : LARGURA == 8 ? "8x4" : "12x4";
constexpr uint8_t EVENTO_INICIO = 1;
constexpr uint8_t EVENTO_OBJETIVO = 2;
constexpr uint8_t EVENTO_FIM = 4;

static portMUX_TYPE trava_wifi = portMUX_INITIALIZER_UNLOCKED;
static DadosTelemetria dados_wifi = {};
static uint8_t eventos_wifi = 0;

static void evento_wifi(uint8_t evento) {
    portENTER_CRITICAL(&trava_wifi);
    eventos_wifi |= evento;
    portEXIT_CRITICAL(&trava_wifi);
}

static EstadoRobo estado_telemetria() {
    switch (estado) {
    case AGUARDANDO: return ROBO_PARADO;
    case CHEGOU: return ROBO_OBJETIVO;
    case ERRO: return ROBO_ERRO;
    default: return ROBO_EXPLORANDO;
    }
}

// Chamado a cada ciclo do loop(): copia posição, paredes, velocidade e estado.
static void publicar_telemetria() {
    static uint32_t ultimo_ms = 0;
    static int32_t ultima_contagem = 0;
    static float velocidade = 0.0f;
    uint32_t agora = millis();
    int32_t contagem = estado == ANDANDO ? contagem_andada() : 0;
    float mm_por_contagem = TAMANHO_CELULA_MM / contagens_por_celula;
    if (agora - ultimo_ms >= PERIODO_TELEMETRIA_MS) {
        // A contagem volta a zero a cada trecho; uma queda conta desde o zero.
        int32_t andou = contagem >= ultima_contagem ? contagem - ultima_contagem : contagem;
        velocidade = andou * mm_por_contagem * 1000.0f / (agora - ultimo_ms);
        ultimo_ms = agora;
        ultima_contagem = contagem;
    }

    // A web usa as coordenadas do mapa do explorar, sem espelhar: start em (0,0), objetivo
    // no canto oposto e x crescendo para o lado em que o labirinto continua (MazeMap.tsx e
    // o RUN_START do frontend põem o robô em (0,0)). Com o labirinto à esquerda do start,
    // o desenho fica espelhado em relação ao real, mas start, objetivo e paredes batem.
    uint8_t paredes = 0;
    if (labirinto.dentro(pos_x, pos_y)) {
        for (Direcao lado : {NORTE, LESTE, SUL, OESTE}) {
            if (labirinto.temParede(pos_x, pos_y, lado)) {
                paredes |= lado;
            }
        }
        // Primeira coluna com o lado ainda desconhecido: no mapa, a borda é sempre o oeste
        // (já marcada) e o lado do labirinto é o leste. As laterais lidas ficam guardadas
        // na detecção até o lado aparecer; com as duas fechadas, o leste é parede.
        if (deteccao.lado() == LADO_DESCONHECIDO && deteccao.parede(pos_y, 1) &&
            deteccao.parede(pos_y, 3)) {
            paredes |= LESTE;
        }
    }
    DadosTelemetria dados;
    dados.x = pos_x;
    dados.y = pos_y;
    dados.direcao = direcao;
    dados.velocidade_mm_s = velocidade;
    dados.odometria_mm = lroundf(celulas_andadas * TAMANHO_CELULA_MM + contagem * mm_por_contagem);
    dados.bateria_pct = ultima_tensao > 0.0f ? bateria_carga(ultima_tensao) : 0.0f;
    dados.paredes = paredes;
    dados.estado = estado_telemetria();

    portENTER_CRITICAL(&trava_wifi);
    dados_wifi = dados;
    portEXIT_CRITICAL(&trava_wifi);
}

static void tarefa_wifi(void *) {
    comunicacao_iniciar();
    Telemetria telemetria(ROBOT_ID);
    char texto[TELEMETRIA_TAMANHO_MAX];
    uint32_t ultimo_pacote = 0;
    bool estava_conectado = false;
    bool em_corrida = false;
    for (;;) {
        comunicacao_atualizar();
        bool conectado = comunicacao_conectada();
        if (conectado != estava_conectado) {
            Serial.println(conectado ? "Wi-Fi: WebSocket conectado" : "Wi-Fi: sem conexão");
            estava_conectado = conectado;
            if (conectado) {
                etapa_boot = 2;
            }
            if (conectado) {
                // O giroscópio já calibrou no setup: a web mostra "Sensores calibrados"
                // (C2). O evento também zera o controle de seq do backend, que descarta
                // seq <= o último visto e o robô zera o seq a cada reinício (o backend
                // também zera na primeira mensagem de cada conexão, PR #372).
                int tamanho = telemetria.calibracao(millis(), true, texto, sizeof(texto));
                if (tamanho > 0) {
                    comunicacao_enviar(texto, tamanho);
                }
            }
        }

        portENTER_CRITICAL(&trava_wifi);
        uint8_t eventos = eventos_wifi;
        eventos_wifi = 0;
        portEXIT_CRITICAL(&trava_wifi);
        if (eventos & EVENTO_FIM) {
            // O loop() atualiza o estado final no ciclo seguinte ao encerrar: espera um
            // pouco para o último pacote já sair com GOAL_REACHED ou ERROR.
            vTaskDelay(pdMS_TO_TICKS(3 * PERIODO_MS));
        }
        portENTER_CRITICAL(&trava_wifi);
        DadosTelemetria dados = dados_wifi;
        portEXIT_CRITICAL(&trava_wifi);

        uint32_t agora = millis();
        int tamanho;
        if (eventos & EVENTO_INICIO) {
            tamanho = telemetria.inicio(agora, dados.bateria_pct, texto, sizeof(texto),
                                        NOME_LABIRINTO);
            if (tamanho > 0) comunicacao_enviar(texto, tamanho);
            em_corrida = true;
            ultimo_pacote = agora - PERIODO_TELEMETRIA_MS;  // primeiro pacote já
        }
        // Telemetria só durante a corrida (contrato, seção 2): o backend abre uma corrida
        // automática para telemetria que chega sem corrida ativa, então pacotes do robô
        // parado no "Pronto" ou depois do fim criariam corridas falsas no histórico.
        bool fim = (eventos & EVENTO_FIM) != 0;
        if (em_corrida && (fim || agora - ultimo_pacote >= PERIODO_TELEMETRIA_MS)) {
            ultimo_pacote = agora;
            tamanho = telemetria.pacote(agora, dados, texto, sizeof(texto));
            if (tamanho > 0) {
                // Sem conexão o pacote é descartado; o backend vê a lacuna no seq.
                comunicacao_enviar(texto, tamanho);
            }
        }
        if (eventos & EVENTO_OBJETIVO) {
            tamanho = telemetria.objetivo(agora, dados.x, dados.y, texto, sizeof(texto));
            if (tamanho > 0) comunicacao_enviar(texto, tamanho);
        }
        if (fim) {
            tamanho = telemetria.fim(agora, texto, sizeof(texto));
            if (tamanho > 0) comunicacao_enviar(texto, tamanho);
            em_corrida = false;
        }
        vTaskDelay(pdMS_TO_TICKS(PERIODO_TAREFA_WIFI_MS));
    }
}
#endif

// Um sensor parou de medir no meio da corrida (nas corridas, o da esquerda, por
// alguns instantes). Em vez de encerrar: freia, tenta iniciar de novo só esse sensor e,
// se ele voltar, segue do mesmo estado. Os prazos (curva, ré, alinhamento, parado)
// andam junto com a pausa, e o motor volta com o impulso de arranque.
static bool recuperar_sensores() {
    motores_frear();
    uint32_t inicio_pausa = millis();
    uint8_t falhas_antes = falhas_distancia;
    imprimir_falhas(falhas_distancia);
    Serial.println("Freou para iniciar de novo esse sensor.");
    // Enquanto o robô termina de parar, o giroscópio continua integrando; senão o giro
    // desse instante (no meio de uma curva) ficaria fora do ângulo.
    while (millis() - inicio_pausa < TEMPO_ASSENTAR_MS) {
        atualizar_angulo();
        delay(PERIODO_MS);
    }
    // Tenta de novo a cada TEMPO_TENTAR_SENSOR_MS, parado, até o sensor voltar. Só sai sem
    // ele pelo BOOT (parar) ou com a bateria baixa.
    for (;;) {
        falhas_distancia = iniciar_distancia(false);
        if (falhas_distancia == 0) {
            break;
        }
        uint32_t espera = millis();
        while (millis() - espera < TEMPO_TENTAR_SENSOR_MS) {
            atualizar_angulo();
            digitalWrite(Pinos::LED_STATUS, (millis() / 100) % 2);
            if (digitalRead(BOTAO_BOOT) == LOW || !bateria_ok()) {
                return false;
            }
            delay(PERIODO_MS);
        }
    }
    digitalWrite(Pinos::LED_STATUS, HIGH);
    uint32_t pausa = millis() - inicio_pausa;
    inicio_curva += pausa;
    inicio_recuo += pausa;
    inicio_alinhamento += pausa;
    inicio_parado += pausa;
    inicio_movimento = millis();
    registrar(SENSOR_RECUPERADO, falhas_para_colunas(falhas_antes));
    Serial.printf("Sensor de volta em %lu ms. Continua.\n", (unsigned long)pausa);
    return true;
}

static void encerrar(CodigoRegistro codigo, Estado final, const char *motivo) {
    motores_frear();
    motores_habilitar(false);
    estado = final;
#ifdef EXPLORAR_WIFI
    // Contrato: GOAL_REACHED e depois RUN_END ao chegar; só RUN_END nos outros fins.
    evento_wifi(final == CHEGOU ? (EVENTO_OBJETIVO | EVENTO_FIM) : EVENTO_FIM);
#endif
    // No FIM_SENSOR, as colunas de parede marcam com X os sensores que falharam.
    registrar(codigo, codigo == FIM_SENSOR ? falhas_para_colunas(falhas_distancia) : 0);
    Serial.println(motivo);
    Serial.printf("Correção do zero do giroscópio: %.3f graus/s\n", erro_zero_giro);
    floodfill.calcular();
    imprimir_mapa();
}

// Marca no mapa as paredes da frente, da direita e da esquerda.
// "so_frente": o robô parou fora do centro da célula (parede logo na frente). Dali os
// sensores laterais enxergam as paredes da célula seguinte, e como o mapa só ganha
// paredes, uma leitura errada fecharia a saída de vez (foi o que deu "sem caminho" em
// (0,2)). Nesse caso só a frente é lida; as laterais ficam como foram lidas no centro.
// Define o lado do labirinto e passa para o mapa as paredes laterais guardadas.
static void definir_lado(LadoLabirinto lado, const char *como) {
    labirinto_a_direita = lado == LADO_DIREITA;
    deteccao.aplicar(labirinto);
    Serial.printf("Lado do labirinto: %s (%s).\n",
                  labirinto_a_direita ? "à direita do start" : "à esquerda do start", como);
}

// Primeira coluna, lado ainda desconhecido: amostra as laterais algumas vezes, com o
// robô parado no centro da célula. Com parede na frente, relê antes de escolher.
static void detectar_lado(bool frente) {
    int leituras = frente ? 1 + RELEITURAS_SEM_SAIDA : 1;
    for (int k = 0; k < leituras && deteccao.lado() == LADO_DESCONHECIDO; k++) {
        bool direita_aberta[AMOSTRAS_LADO], esquerda_aberta[AMOSTRAS_LADO];
        for (int i = 0; i < AMOSTRAS_LADO; i++) {
            uint32_t inicio = millis();
            while (millis() - inicio < INTERVALO_AMOSTRA_LADO_MS) {
                atualizar_angulo();
                distancia_ler(distancias);
                delay(5);
            }
            direita_aberta[i] = lado_livre(distancias.direita);
            esquerda_aberta[i] = lado_livre(distancias.esquerda);
        }
        if (deteccao.celula(pos_y, direita_aberta, esquerda_aberta, AMOSTRAS_LADO) !=
            LADO_DESCONHECIDO) {
            definir_lado(deteccao.lado(), "abertura na primeira coluna");
            return;
        }
    }
    if (frente) {
        definir_lado(deteccao.escolher(LADO_PADRAO), "parede no fim da coluna, sem abertura clara");
    }
}

static void ler_paredes(bool &frente, bool &direita, bool &esquerda, bool so_frente) {
    frente = distancias.frente < LIMIAR_PAREDE_FRENTE_MM;
    if (deteccao.lado() == LADO_DESCONHECIDO) {
        if (so_frente) {
            // Fora do centro da célula: as laterais não servem para a detecção.
            if (frente) {
                definir_lado(deteccao.escolher(LADO_PADRAO), "parede logo na frente");
            }
        } else {
            detectar_lado(frente);
        }
        if (deteccao.lado() == LADO_DESCONHECIDO) {
            // Ainda na primeira coluna: o norte é igual nos dois mapas; as laterais
            // ficam guardadas na detecção.
            direita = deteccao.parede(pos_y, 1);
            esquerda = deteccao.parede(pos_y, 3);
            if (frente) {
                labirinto.colocarParede(pos_x, pos_y, NORTE);
            }
            return;
        }
        // Lado definido nesta célula: as laterais dela já foram para o mapa.
        so_frente = true;
    }
    if (so_frente) {
        direita = labirinto.temParede(pos_x, pos_y, lado_no_mapa(1));
        esquerda = labirinto.temParede(pos_x, pos_y, lado_no_mapa(3));
    } else {
        direita = !lado_livre(distancias.direita);
        esquerda = !lado_livre(distancias.esquerda);
    }
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
static void decidir(bool so_frente = false) {
    bool frente, direita, esquerda;
    ler_paredes(frente, direita, esquerda, so_frente);

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

    if (deteccao.lado() == LADO_DESCONHECIDO) {
        // Primeira coluna sem abertura confirmada: o único caminho é seguir reto.
        Serial.printf("(%d,%d) lado do labirinto ainda desconhecido (direita %s, esquerda %s): "
                      "segue.\n",
                      pos_x, pos_y, direita ? "parede" : "aberta", esquerda ? "parede" : "aberta");
        registrar(ACAO_SEGUE, (frente ? 1 : 0) | (direita ? 2 : 0) | (esquerda ? 4 : 0));
        direcao_alvo = direcao;
        comecar_celula();
        return;
    }

    Direcao proxima;
    if (!floodfill.proximaDirecao(pos_x, pos_y, direcao, proxima)) {
        // Alguma parede foi lida errada e fechou o caminho. Em vez de parar: esquece as
        // paredes internas e segue explorando daqui, marcando só as desta célula.
        esquecer_mapa("sem caminho até o objetivo");
        if (frente) labirinto.colocarParede(pos_x, pos_y, lado_no_mapa(0));
        if (direita) labirinto.colocarParede(pos_x, pos_y, lado_no_mapa(1));
        if (esquerda) labirinto.colocarParede(pos_x, pos_y, lado_no_mapa(3));
        if (!floodfill.proximaDirecao(pos_x, pos_y, direcao, proxima)) {
            // As três leituras daqui fecham tudo, mas alguma saída existe: vai para o lado
            // que mede mais longe (uma quina vista na diagonal dá 75 a 93 mm; parede de
            // verdade, 20 a 60). Voltar não serve: na célula do start, atrás é a borda.
            uint16_t medidas[3] = {distancias.frente, distancias.direita, distancias.esquerda};
            static const int LADOS[3] = {0, 1, 3};
            int melhor = 0;
            for (int k = 1; k < 3; k++) {
                if (medidas[k] > medidas[melhor]) {
                    melhor = k;
                }
            }
            proxima = lado_no_mapa(LADOS[melhor]);
            Serial.printf("Todas as saídas lidas como parede: vai para o lado mais aberto (%u mm).\n",
                          medidas[melhor]);
        }
    }

    // Quantas vezes 90° para a direita (do mapa) separam a direção atual da próxima.
    int passos = passos_ate(direcao, proxima, labirinto_a_direita);
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
        preparar_curva(-ANGULO_CURVA_DIREITA_GRAUS);
    } else if (passos == 3) {
        preparar_curva(ANGULO_CURVA_ESQUERDA_GRAUS);
    } else {
        // 180°: duas curvas de 90° para o mesmo lado. Entre elas, a parede que estava do
        // lado fica na frente e o alinhamento centraliza o robô também nesse eixo.
        // O lado é o da parede mais longe: com o robô fora do centro, girar para o lado
        // mais perto faz a ponta raspar na parede. Sem leitura (65535) conta como longe.
        float graus = distancias.esquerda >= distancias.direita ? ANGULO_CURVA_ESQUERDA_GRAUS
                                                                 : -ANGULO_CURVA_DIREITA_GRAUS;
        Serial.printf("180° pela %s (esquerda %u mm, direita %u mm).\n",
                      graus > 0 ? "esquerda" : "direita", distancias.esquerda,
                      distancias.direita);
        segunda_metade_180 = graus;
        preparar_curva(graus);
    }
}

// Quantas células o robô andou no último trecho: pelos encoders, com a parede da frente
// só conferindo (navegacao/contagem_celulas.*). De longe a frente erra: numa corrida ela
// leu 383 mm no começo e 53 mm no fim de uma célula só, e o robô ficou uma célula
// adiantado no mapa.
static int celulas_no_trecho() {
    LeituraTrecho trecho = {contagem_andada(), contagens_por_celula, frente_inicio_celula,
                            distancias.frente};
    return contagem_celulas(trecho, TAMANHO_CELULA_MM);
}

// Robô parado depois de andar uma célula: atualiza a posição e decide.
// O mapa ficou inconsistente (caminho fechado por parede lida errada, posição fora do
// mapa): volta só com a borda externa e segue explorando. As passagens já percorridas
// continuam valendo e o lado do labirinto também.
static void esquecer_mapa(const char *motivo) {
    labirinto = Labirinto(LARGURA, ALTURA);
    Serial.printf("%s: esqueceu as paredes internas e segue explorando de (%d,%d).\n", motivo,
                  pos_x, pos_y);
    registrar(MAPA_ESQUECIDO, 0);
}

// Parado com parede logo na frente numa passagem que o robô já percorreu: ele andou mais
// do que contou (aconteceu ao voltar de um beco depois da meia-volta). Avança a posição
// até a primeira célula sem passagem conhecida à frente. Retorna as células corrigidas.
static int corrigir_posicao() {
    if (distancias.frente >= LIMIAR_PAREDE_FRENTE_MM) {
        return 0;
    }
    int antes_x = pos_x, antes_y = pos_y;
    int corrigidas = passagens.corrigir(pos_x, pos_y, direcao, labirinto);
    if (corrigidas > 0) {
        celulas_andadas += corrigidas;
        Serial.printf("Parede a %u mm numa passagem já percorrida: posição corrigida de "
                      "(%d,%d) para (%d,%d).\n",
                      distancias.frente, antes_x, antes_y, pos_x, pos_y);
        registrar(POSICAO_CORRIGIDA, 0);
    }
    return corrigidas;
}

static void chegou_na_celula() {
    int celulas = celulas_no_trecho();
    if (celulas == 0 && corrigir_posicao() > 0) {
        // Contou zero, mas a parede está numa passagem conhecida: já está em outra célula,
        // parado com a parede logo na frente (perto do centro). Lê as paredes normalmente.
        decidir();
        return;
    }
    if (celulas == 0) {
        // Parou pela parede quase sem sair do lugar: ela estava logo na frente e não foi
        // vista antes de virar. O robô continua na mesma célula; o decidir() marca essa
        // parede e escolhe outro caminho. Sem isso, a posição avançaria sem o robô sair
        // do lugar e o mapa ficaria todo deslocado.
        Serial.printf("Parede logo na frente (%u mm): continua em (%d,%d).\n",
                      distancias.frente, pos_x, pos_y);
        decidir(true);
        return;
    }
    if (celulas > 1) {
        // Passou de células sem parar (os encoders contaram mais de 1,5 célula): a posição
        // acompanha a contagem. As paredes laterais dessas células ficam sem marcar.
        Serial.printf("Andou %d células sem parar: confira os encoders.\n", celulas);
    } else if (!trecho_travou) {
        ajustar_freio();
        calibrar_celula();
    }
    for (int i = 0; i < celulas; i++) {
        passagens.marcar(pos_x, pos_y, direcao);
        avancar(direcao, pos_x, pos_y);
        celulas_andadas++;
    }
    if (!labirinto.dentro(pos_x, pos_y)) {
        // A contagem passou da borda: a posição está errada. Traz para dentro e recomeça o
        // mapa (antes parava com FIM_FORA_DO_MAPA). Confira LARGURA, ALTURA e o lado.
        pos_x = constrain(pos_x, 0, LARGURA - 1);
        pos_y = constrain(pos_y, 0, ALTURA - 1);
        esquecer_mapa("posição fora do mapa");
    }
    corrigir_posicao();
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
    comecar_registro_boot();
    if (LADO_CONFIGURADO != LADO_DESCONHECIDO) {
        deteccao.definir(LADO_CONFIGURADO);  // lado forçado no código: sem detecção
    }
    if (memoria.getBytesLength("anterior") > 0) {
        imprimir_registro("anterior", "Corrida anterior");
    }
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
    if (iniciar_distancia(true) != 0) {
        // O start tenta iniciar de novo; se não der, recusa a corrida (LED piscando rápido).
        Serial.println("Erro: algum sensor de distância não iniciou. O start tenta de novo.");
    }
    if (!energia_iniciar(Wire)) {
        Serial.println("Aviso: INA219 não respondeu. O duty máximo fica no padrão.");
    }
    // Sem giroscópio o robô não anda reto nem sabe quando terminou a curva. O contato do
    // MPU6050 falhou várias vezes e voltou sozinho: em vez de desistir, tenta de novo a
    // cada TEMPO_TENTAR_IMU_MS (LED piscando rápido) e segue assim que ele responder.
    for (int tentativa = 1; !imu_iniciar(Wire); tentativa++) {
        if (tentativa == 1) {
            Serial.println("Erro: MPU6050 não respondeu. Tentando de novo (confira os fios dele).");
        } else if (tentativa % 20 == 0) {
            Serial.printf("MPU6050 ainda sem resposta (%d tentativas).\n", tentativa);
        }
        uint32_t inicio = millis();
        while (millis() - inicio < TEMPO_TENTAR_IMU_MS) {
            digitalWrite(Pinos::LED_STATUS, (millis() / 100) % 2);
            delay(10);
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
    bateria_ok();  // lê a tensão uma vez, para a telemetria já começar com a bateria
    etapa_boot = 1;
    Serial.printf("Bateria: %.2f V\n", ultima_tensao);
#ifdef EXPLORAR_WIFI
    // Só depois da calibração do giroscópio: o rádio liga com o robô já pronto.
    xTaskCreatePinnedToCore(tarefa_wifi, "wifi", PILHA_WIFI, nullptr, 1, nullptr, NUCLEO_WIFI);
    Serial.println("Wi-Fi: conectando (telemetria a cada 100 ms).");
#endif
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
    atualizar_registro_boot();
#ifdef EXPLORAR_WIFI
    publicar_telemetria();
#endif

    if (comando_start()) {
        if (estado == AGUARDANDO || estado == CHEGOU || estado == ERRO) {
            // Recomeça do start, mas guarda o mapa: numa segunda corrida o robô já
            // conhece as paredes e vai pelo caminho mais curto.
            pos_x = 0;
            pos_y = 0;
            direcao = NORTE;
            celulas_andadas = 0;
            total_registros = 0;
            guardar_corrida_anterior();
            memoria.remove("log");
            inicio_corrida = millis();
            if (!bateria_ok()) {
                // Sem USB, o LED piscando rápido e o registro mostram o motivo.
                encerrar(FIM_BATERIA, ERRO, "Bateria abaixo de 6,6 V. Recarregue antes de usar.");
                return;
            }
            falhas_distancia = distancia_falhas(IDADE_MAXIMA_DISTANCIA_MS);
            if (falhas_distancia != 0) {
                // Robô parado: dá para tentar iniciar os sensores de novo.
                imprimir_falhas(falhas_distancia);
                Serial.println("Iniciando os sensores de distância de novo.");
                falhas_distancia = iniciar_distancia(false);
            }
            if (falhas_distancia != 0) {
                encerrar(FIM_SENSOR, ERRO,
                         "Algum sensor de distância não está medindo. Confira o I2C e reinicie.");
                return;
            }
            rumo = angulo;
            medindo_curva = false;
            segunda_metade_180 = 0.0f;
            equilibrio_desligado = false;
            motores_habilitar(true);
            digitalWrite(Pinos::LED_STATUS, HIGH);
#ifdef EXPLORAR_WIFI
            evento_wifi(EVENTO_INICIO);
#endif
            decidir();
        } else {
            encerrar(FIM_BOTAO, AGUARDANDO,
                     "Parado pelo botão. Aperte de novo para recomeçar do start.");
        }
        return;
    }

    if (estado != AGUARDANDO && estado != CHEGOU && estado != ERRO) {
        falhas_distancia = distancia_falhas(IDADE_MAXIMA_DISTANCIA_MS);
        if (falhas_distancia != 0 && !recuperar_sensores()) {
            if (!bateria_ok()) {
                encerrar(FIM_BATERIA, ERRO, "Bateria abaixo de 6,6 V. Teste encerrado.");
            } else {
                encerrar(FIM_BOTAO, AGUARDANDO,
                         "Parado pelo botão. Aperte de novo para recomeçar do start.");
            }
            return;
        }
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
    case RECUANDO:
        recuar();
        break;
    case PARADO_RECUO:
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            retomar_curva();
        }
        break;
    case RECUANDO_RETA:
        recuar_reta();
        break;
    case PARADO_RECUO_RETA:
        if (millis() - inicio_parado >= TEMPO_PARADO_MS) {
            retomar_reta();
        }
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
