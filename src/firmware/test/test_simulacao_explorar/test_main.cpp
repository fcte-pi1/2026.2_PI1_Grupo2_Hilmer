// Simulação célula a célula da exploração do exemplo_explorar, nos dois lados de start.
// Rodar com: pio test -e native -f test_simulacao_explorar
//
// O robô simulado segue as mesmas regras do explorar, com as mesmas funções puras:
//   - lê as paredes da frente, direita e esquerda no labirinto real e marca no mapa
//     com lado_no_mapa();
//   - o Flood Fill escolhe a próxima direção e passos_ate() diz a curva do robô;
//   - vira e anda uma célula no labirinto real.
// O mapa do explorar começa no start (0,0) virado para o norte, com x crescendo para o
// lado em que o labirinto continua. Com o labirinto à direita do start (robô começa
// no canto esquerdo) o mapa é igual ao real; à esquerda, é espelhado.
// Os sensores e o movimento são perfeitos: a simulação confere a lógica de mapa e de
// decisão, não o controle dos motores.
#include <unity.h>

#include <cstdio>
#include <random>

#include "navegacao/deteccao_lado.h"
#include "navegacao/espelho.h"
#include "navegacao/floodfill.h"
#include "navegacao/labirinto.h"
#include "navegacao/passagens.h"

void setUp() {}
void tearDown() {}

// Labirinto real gerado por busca em profundidade (labirinto perfeito: um único caminho
// entre duas células quaisquer, com becos sem saída).
struct LabirintoReal {
    int largura;
    int altura;
    // aberto[x][y] em bits de Direcao: passagem para a vizinha naquela direção.
    uint8_t aberto[LABIRINTO_MAX_LARGURA][LABIRINTO_MAX_ALTURA] = {};

    bool parede(int x, int y, Direcao d) const {
        if (x < 0 || y < 0 || x >= largura || y >= altura) {
            return true;
        }
        return (aberto[x][y] & d) == 0;
    }
};

static Direcao oposta(Direcao d) {
    return direcao_do_indice(indice_direcao(d) + 2);
}

static LabirintoReal gerar(int largura, int altura, uint32_t semente) {
    LabirintoReal r;
    r.largura = largura;
    r.altura = altura;
    std::mt19937 rng(semente);
    bool visitado[LABIRINTO_MAX_LARGURA][LABIRINTO_MAX_ALTURA] = {};
    int pilha_x[LABIRINTO_MAX_LARGURA * LABIRINTO_MAX_ALTURA];
    int pilha_y[LABIRINTO_MAX_LARGURA * LABIRINTO_MAX_ALTURA];
    int topo = 0;
    pilha_x[topo] = 0;
    pilha_y[topo] = 0;
    topo++;
    visitado[0][0] = true;
    while (topo > 0) {
        int x = pilha_x[topo - 1], y = pilha_y[topo - 1];
        Direcao opcoes[4];
        int n = 0;
        for (int i = 0; i < 4; i++) {
            Direcao d = direcao_do_indice(i);
            int nx = x, ny = y;
            avancar(d, nx, ny);
            if (nx >= 0 && ny >= 0 && nx < largura && ny < altura && !visitado[nx][ny]) {
                opcoes[n++] = d;
            }
        }
        if (n == 0) {
            topo--;
            continue;
        }
        Direcao d = opcoes[rng() % n];
        int nx = x, ny = y;
        avancar(d, nx, ny);
        r.aberto[x][y] |= d;
        r.aberto[nx][ny] |= oposta(d);
        visitado[nx][ny] = true;
        pilha_x[topo] = nx;
        pilha_y[topo] = ny;
        topo++;
    }
    return r;
}

enum Resultado { CHEGOU, SEM_CAMINHO, BATEU, CELULAS_DEMAIS, POSICAO_ERRADA, TELEMETRIA_ERRADA };

struct Corrida {
    Resultado resultado;
    int celulas;
    int curvas;
};

// Roda a exploração. "start_no_canto_esquerdo": o robô começa no canto esquerdo do
// labirinto real (x real 0) e o labirinto continua à direita. "labirinto_a_direita" é a
// configuração gravada no robô (LABIRINTO_A_DIREITA).
static Corrida explorar(const LabirintoReal &real, bool start_no_canto_esquerdo,
                        bool labirinto_a_direita) {
    const int L = real.largura;
    // Robô no labirinto real.
    int rx = start_no_canto_esquerdo ? 0 : L - 1;
    int ry = 0;
    Direcao rdir = NORTE;
    // Mapa e estado do explorar.
    Labirinto mapa(L, real.altura);
    FloodFill floodfill(mapa);
    int x = 0, y = 0;
    Direcao direcao = NORTE;
    Corrida c = {CELULAS_DEMAIS, 0, 0};
    bool espelhado = !labirinto_a_direita;

    for (int passo = 0; passo < 200; passo++) {
        // A posição do mapa, convertida para o real, tem de bater com a do robô.
        if (espelho_x(x, L, espelhado) != rx || y != ry ||
            espelho_direcao(direcao, espelhado) != rdir) {
            c.resultado = POSICAO_ERRADA;
            return c;
        }
        // Lê as paredes (frente, direita, esquerda) e marca no mapa.
        static const int LADOS[] = {0, 1, 3};
        for (int lado : LADOS) {
            if (real.parede(rx, ry, direcao_do_indice(indice_direcao(rdir) + lado))) {
                mapa.colocarParede(x, y, lado_no_mapa(direcao, lado, labirinto_a_direita));
            }
        }
        if (floodfill.objetivo(x, y)) {
            c.resultado = CHEGOU;
            return c;
        }
        Direcao proxima;
        if (!floodfill.proximaDirecao(x, y, direcao, proxima)) {
            c.resultado = SEM_CAMINHO;
            return c;
        }
        int passos = passos_ate(direcao, proxima, labirinto_a_direita);
        if (passos != 0) {
            c.curvas += passos == 2 ? 2 : 1;
        }
        // Curva no robô real (passos para a direita dele) e no mapa.
        rdir = direcao_do_indice(indice_direcao(rdir) + passos);
        direcao = proxima;
        // Anda uma célula.
        if (real.parede(rx, ry, rdir)) {
            c.resultado = BATEU;
            return c;
        }
        avancar(rdir, rx, ry);
        avancar(direcao, x, y);
        c.celulas++;
    }
    return c;
}

// Objetivo do explorar: o canto oposto ao start, no mapa (L-1, A-1).
static void testar_varios(int largura, int altura, bool start_no_canto_esquerdo) {
    int chegou = 0;
    for (uint32_t semente = 1; semente <= 300; semente++) {
        LabirintoReal real = gerar(largura, altura, semente);
        Corrida c = explorar(real, start_no_canto_esquerdo, start_no_canto_esquerdo);
        char msg[96];
        snprintf(msg, sizeof(msg), "%dx%d semente %u resultado %d", largura, altura,
                 (unsigned)semente, c.resultado);
        TEST_ASSERT_EQUAL_MESSAGE(CHEGOU, c.resultado, msg);
        chegou++;
    }
    TEST_ASSERT_EQUAL(300, chegou);
}

void test_start_canto_esquerdo_4x4() { testar_varios(4, 4, true); }
void test_start_canto_direito_4x4() { testar_varios(4, 4, false); }
void test_start_canto_esquerdo_8x4() { testar_varios(8, 4, true); }
void test_start_canto_direito_8x4() { testar_varios(8, 4, false); }
void test_start_canto_esquerdo_12x4() { testar_varios(12, 4, true); }
void test_start_canto_direito_12x4() { testar_varios(12, 4, false); }

// Labirinto e seu espelho: a corrida tem de ter as mesmas células e curvas, só com os
// lados trocados.
void test_espelho_faz_a_mesma_corrida() {
    for (uint32_t semente = 1; semente <= 100; semente++) {
        LabirintoReal real = gerar(4, 4, semente);
        LabirintoReal espelho = real;
        espelho.largura = real.largura;
        for (int x = 0; x < 4; x++) {
            for (int y = 0; y < 4; y++) {
                espelho.aberto[3 - x][y] = espelho_paredes(real.aberto[x][y], true);
            }
        }
        Corrida a = explorar(real, true, true);
        Corrida b = explorar(espelho, false, false);
        TEST_ASSERT_EQUAL(CHEGOU, a.resultado);
        TEST_ASSERT_EQUAL(CHEGOU, b.resultado);
        TEST_ASSERT_EQUAL(a.celulas, b.celulas);
        TEST_ASSERT_EQUAL(a.curvas, b.curvas);
    }
}

// Com a configuração errada (LABIRINTO_A_DIREITA do lado oposto ao real), o mapa sai
// espelhado em relação ao robô: a exploração não chega ao objetivo.
void test_configuracao_errada_nao_resolve() {
    int falhou = 0;
    for (uint32_t semente = 1; semente <= 100; semente++) {
        LabirintoReal real = gerar(4, 4, semente);
        if (explorar(real, true, false).resultado != CHEGOU) {
            falhou++;
        }
    }
    TEST_ASSERT_EQUAL(100, falhou);
}

// Funções usadas pelo explorar.
void test_lado_no_mapa_e_passos() {
    // Labirinto à direita: direita do robô virado ao norte é o leste do mapa.
    TEST_ASSERT_EQUAL(LESTE, lado_no_mapa(NORTE, 1, true));
    TEST_ASSERT_EQUAL(OESTE, lado_no_mapa(NORTE, 3, true));
    // Espelhado: a direita do robô é o oeste do mapa.
    TEST_ASSERT_EQUAL(OESTE, lado_no_mapa(NORTE, 1, false));
    TEST_ASSERT_EQUAL(LESTE, lado_no_mapa(NORTE, 3, false));
    TEST_ASSERT_EQUAL(NORTE, lado_no_mapa(NORTE, 0, false));
    TEST_ASSERT_EQUAL(SUL, lado_no_mapa(NORTE, 2, false));
    // Do norte para o leste do mapa: direita sem espelho, esquerda espelhado.
    TEST_ASSERT_EQUAL(1, passos_ate(NORTE, LESTE, true));
    TEST_ASSERT_EQUAL(3, passos_ate(NORTE, LESTE, false));
    TEST_ASSERT_EQUAL(2, passos_ate(NORTE, SUL, false));
    TEST_ASSERT_EQUAL(0, passos_ate(LESTE, LESTE, false));
}


// ---------------------------------------------------------------------------
// Detecção automática do lado (navegacao/deteccao_lado), como no explorar: até achar a
// primeira abertura lateral da primeira coluna, o robô só anda reto e guarda as paredes;
// em cada célula dessa fase, AMOSTRAS leituras das laterais, cada uma errando (aberto
// virando parede ou o contrário) com probabilidade "ruido".
constexpr int AMOSTRAS = 5;
constexpr int RELEITURAS_SEM_SAIDA = 3;


// Paredes que o explorar manda na telemetria (como em publicar_telemetria): as do mapa e,
// com o lado ainda desconhecido, o leste quando as duas laterais guardadas são parede.
static uint8_t paredes_telemetria(const Labirinto &mapa, const DeteccaoLado &deteccao, int x, int y) {
    uint8_t paredes = 0;
    for (int i = 0; i < 4; i++) {
        Direcao d = direcao_do_indice(i);
        if (mapa.temParede(x, y, d)) {
            paredes |= d;
        }
    }
    if (deteccao.lado() == LADO_DESCONHECIDO && deteccao.parede(y, 1) && deteccao.parede(y, 3)) {
        paredes |= LESTE;
    }
    return paredes;
}

// Paredes reais da célula do robô, nas coordenadas da web (as do mapa do explorar):
// com o start no canto direito, o labirinto real é o espelho do que a web desenha.
static uint8_t paredes_reais_na_web(const LabirintoReal &real, int rx, int ry,
                                    bool start_no_canto_esquerdo) {
    uint8_t reais = 0;
    for (int i = 0; i < 4; i++) {
        Direcao d = direcao_do_indice(i);
        if (real.parede(rx, ry, d)) {
            reais |= d;
        }
    }
    return espelho_paredes(reais, !start_no_canto_esquerdo);
}

struct CorridaAuto {
    Resultado resultado;
    LadoLabirinto lado;
};

// "passa_depois_da_volta": depois de cada meia-volta, o robô real segue reto até a
// parede, mas o explorar conta só uma célula (o erro da corrida de 23/11, em que ele
// voltou de um beco, parou duas células adiante e fechou o caminho no mapa).
// "corrigir": usa Passagens::corrigir, como o explorar, ao parar com parede na frente.
static CorridaAuto explorar_auto(const LabirintoReal &real, bool start_no_canto_esquerdo,
                                 float ruido, uint32_t semente, LadoLabirinto padrao,
                                 bool passa_depois_da_volta = false, bool corrigir = true,
                                 float paredes_falsas = 0.0f, bool esquecer = true) {
    std::mt19937 rng(semente * 7919u + 17u);
    std::uniform_real_distribution<float> sorteio(0.0f, 1.0f);
    const int L = real.largura;
    int rx = start_no_canto_esquerdo ? 0 : L - 1;
    int ry = 0;
    Direcao rdir = NORTE;
    Labirinto mapa(L, real.altura);
    FloodFill floodfill(mapa);
    DeteccaoLado deteccao(real.altura);
    Passagens passagens(L, real.altura);
    int x = 0, y = 0;
    Direcao direcao = NORTE;
    bool a_direita = padrao == LADO_DIREITA;
    CorridaAuto c = {CELULAS_DEMAIS, LADO_DESCONHECIDO};

    for (int passo = 0; passo < 400; passo++) {
        bool frente_real = real.parede(rx, ry, rdir);
        if (deteccao.lado() == LADO_DESCONHECIDO) {
            // Ainda na primeira coluna, virado para o norte: x = 0 nos dois mapas.
            bool d_real = !real.parede(rx, ry, direcao_do_indice(indice_direcao(rdir) + 1));
            bool e_real = !real.parede(rx, ry, direcao_do_indice(indice_direcao(rdir) + 3));
            LadoLabirinto lado = LADO_DESCONHECIDO;
            // Com parede na frente, a abertura tem de estar aqui (ou foi perdida antes):
            // relê até RELEITURAS_SEM_SAIDA vezes antes de escolher.
            int leituras = frente_real ? 1 + RELEITURAS_SEM_SAIDA : 1;
            for (int k = 0; k < leituras && lado == LADO_DESCONHECIDO; k++) {
                bool d[AMOSTRAS], e[AMOSTRAS];
                for (int i = 0; i < AMOSTRAS; i++) {
                    d[i] = sorteio(rng) < ruido ? !d_real : d_real;
                    e[i] = sorteio(rng) < ruido ? !e_real : e_real;
                }
                lado = deteccao.celula(y, d, e, AMOSTRAS);
            }
            if (lado == LADO_DESCONHECIDO && frente_real) {
                lado = deteccao.escolher(padrao);
            }
            if (lado == LADO_DESCONHECIDO) {
                if (frente_real) {
                    mapa.colocarParede(x, y, NORTE);
                }
                // Sem ruído, a telemetria desta célula já tem de mostrar as paredes reais
                // nas coordenadas da web, mesmo sem saber o lado.
                if (ruido == 0.0f && (x != 0 || paredes_telemetria(mapa, deteccao, x, y) !=
                                                     paredes_reais_na_web(real, rx, ry,
                                                                          start_no_canto_esquerdo))) {
                    c.resultado = TELEMETRIA_ERRADA;
                    return c;
                }
                // Só anda reto: é o único caminho da primeira coluna.
                passagens.marcar(x, y, direcao);
                avancar(rdir, rx, ry);
                avancar(direcao, x, y);
                continue;
            }
            a_direita = lado == LADO_DIREITA;
            deteccao.aplicar(mapa);
            c.lado = lado;
        }
        // Parou com parede na frente numa passagem já percorrida: está mais à frente.
        if (corrigir && frente_real) {
            passagens.corrigir(x, y, direcao, mapa);
        }
        bool espelhado = !a_direita;
        if (espelho_x(x, L, espelhado) != rx || y != ry ||
            espelho_direcao(direcao, espelhado) != rdir) {
            c.resultado = POSICAO_ERRADA;
            return c;
        }
        static const int LADOS[] = {0, 1, 3};
        bool lida[3];
        for (int k = 0; k < 3; k++) {
            int lado = LADOS[k];
            // "paredes_falsas": lado aberto lido como parede (quina vista na diagonal, robô
            // fora do centro), o erro que fechava o caminho no mapa.
            lida[k] = real.parede(rx, ry, direcao_do_indice(indice_direcao(rdir) + lado)) ||
                      sorteio(rng) < paredes_falsas;
            if (lida[k]) {
                mapa.colocarParede(x, y, lado_no_mapa(direcao, lado, a_direita));
            }
        }
        // Telemetria: a web recebe a posição do mapa; as paredes da frente e das laterais
        // (a de trás pode faltar, por isso só as três lidas) têm de bater com as reais.
        if (ruido == 0.0f && paredes_falsas == 0.0f) {
            uint8_t lidas = 0;
            for (int lado : LADOS) {
                lidas |= lado_no_mapa(direcao, lado, a_direita);
            }
            if ((paredes_telemetria(mapa, deteccao, x, y) & lidas) !=
                (paredes_reais_na_web(real, rx, ry, start_no_canto_esquerdo) & lidas)) {
                c.resultado = TELEMETRIA_ERRADA;
                return c;
            }
        }
        if (floodfill.objetivo(x, y)) {
            c.resultado = CHEGOU;
            return c;
        }
        Direcao proxima;
        if (!floodfill.proximaDirecao(x, y, direcao, proxima)) {
            if (!esquecer) {
                c.resultado = SEM_CAMINHO;
                return c;
            }
            // Como o explorar (esquecer_mapa): só a borda, mais as paredes lidas aqui.
            mapa = Labirinto(L, real.altura);
            for (int k = 0; k < 3; k++) {
                if (lida[k]) {
                    mapa.colocarParede(x, y, lado_no_mapa(direcao, LADOS[k], a_direita));
                }
            }
            if (!floodfill.proximaDirecao(x, y, direcao, proxima)) {
                // Como o explorar: o lado que mede mais longe. Na simulação, uma saída
                // aberta lida como parede ainda mede mais que uma parede de verdade.
                int melhor = -1;
                for (int k = 0; k < 3 && melhor < 0; k++) {
                    if (!real.parede(rx, ry, direcao_do_indice(indice_direcao(rdir) + LADOS[k]))) {
                        melhor = k;
                    }
                }
                proxima = melhor >= 0 ? lado_no_mapa(direcao, LADOS[melhor], a_direita)
                                      : direcao_do_indice(indice_direcao(direcao) + 2);
            }
        }
        int passos = passos_ate(direcao, proxima, a_direita);
        rdir = direcao_do_indice(indice_direcao(rdir) + passos);
        direcao = proxima;
        if (real.parede(rx, ry, rdir)) {
            c.resultado = BATEU;
            return c;
        }
        passagens.marcar(x, y, direcao);
        avancar(rdir, rx, ry);
        avancar(direcao, x, y);
        if (passa_depois_da_volta && passos == 2) {
            while (!real.parede(rx, ry, rdir)) {
                avancar(rdir, rx, ry);
            }
        }
    }
    return c;
}

struct Taxa {
    int corridas;
    int chegou;
    int lado_certo;
};

static Taxa rodar_auto(int largura, int altura, float ruido, LadoLabirinto padrao) {
    Taxa t = {0, 0, 0};
    for (uint32_t semente = 1; semente <= 300; semente++) {
        LabirintoReal real = gerar(largura, altura, semente);
        for (int canto = 0; canto < 2; canto++) {
            bool esquerdo = canto == 0;
            // O mesmo labirinto, mas com o start no canto direito: espelho do gerado.
            LabirintoReal r = real;
            if (!esquerdo) {
                for (int x = 0; x < largura; x++) {
                    for (int y = 0; y < altura; y++) {
                        r.aberto[largura - 1 - x][y] = espelho_paredes(real.aberto[x][y], true);
                    }
                }
            }
            CorridaAuto c = explorar_auto(r, esquerdo, ruido, semente * 2 + canto, padrao);
            t.corridas++;
            t.chegou += c.resultado == CHEGOU ? 1 : 0;
            t.lado_certo += c.lado == (esquerdo ? LADO_DIREITA : LADO_ESQUERDA) ? 1 : 0;
        }
    }
    printf("  %dx%d ruido %.0f%%: chegou %d/%d, lado certo %d/%d\n", largura, altura,
           ruido * 100, t.chegou, t.corridas, t.lado_certo, t.corridas);
    return t;
}

void test_auto_sem_ruido_resolve_os_dois_lados() {
    const int tamanhos[][2] = {{4, 4}, {8, 4}, {12, 4}};
    for (auto &tam : tamanhos) {
        Taxa t = rodar_auto(tam[0], tam[1], 0.0f, LADO_ESQUERDA);
        TEST_ASSERT_EQUAL(t.corridas, t.chegou);
        TEST_ASSERT_EQUAL(t.corridas, t.lado_certo);
    }
}

void test_auto_com_leitura_falsa() {
    // Cada amostra lateral erra com 5% ou 10% de chance só na fase da detecção.
    Taxa t5 = rodar_auto(4, 4, 0.05f, LADO_ESQUERDA);
    Taxa t10 = rodar_auto(4, 4, 0.10f, LADO_ESQUERDA);
    Taxa t12 = rodar_auto(12, 4, 0.10f, LADO_ESQUERDA);
    rodar_auto(4, 4, 0.20f, LADO_ESQUERDA);
    TEST_ASSERT_EQUAL(t12.corridas, t12.chegou);
    TEST_ASSERT_EQUAL(t5.corridas, t5.lado_certo);
    TEST_ASSERT_EQUAL(t5.corridas, t5.chegou);
    TEST_ASSERT_EQUAL(t10.corridas, t10.lado_certo);
    TEST_ASSERT_EQUAL(t10.corridas, t10.chegou);
}

void test_deteccao_unidade() {
    DeteccaoLado d(4);
    bool fechado[3] = {false, false, false};
    bool aberto[3] = {true, true, true};
    bool misto[3] = {true, false, true};
    bool um[3] = {false, true, false};
    // Duas paredes: ainda não sabe.
    TEST_ASSERT_EQUAL(LADO_DESCONHECIDO, d.celula(0, fechado, fechado, 3));
    // Abertura em só uma das três amostras: não confirma.
    TEST_ASSERT_EQUAL(LADO_DESCONHECIDO, d.celula(1, um, fechado, 3));
    // Aberto nas três, só à esquerda: labirinto à esquerda.
    TEST_ASSERT_EQUAL(LADO_ESQUERDA, d.celula(2, fechado, aberto, 3));
    // As paredes guardadas vão para o mapa espelhado: direita do robô = oeste do mapa.
    Labirinto mapa(4, 4);
    d.aplicar(mapa);
    TEST_ASSERT_TRUE(mapa.temParede(0, 0, OESTE));
    TEST_ASSERT_TRUE(mapa.temParede(0, 0, LESTE));
    TEST_ASSERT_FALSE(mapa.temParede(0, 2, LESTE));
    // Duas de três abertas (uma amostra errada): confirma.
    DeteccaoLado d5(4);
    TEST_ASSERT_EQUAL(LADO_DIREITA, d5.celula(0, misto, fechado, 3));
    // Aberturas dos dois lados ao mesmo tempo: leitura impossível, não confirma.
    DeteccaoLado d2(4);
    TEST_ASSERT_EQUAL(LADO_DESCONHECIDO, d2.celula(0, aberto, aberto, 3));
    // Parede na frente sem confirmar: escolhe pelo que mais apareceu aberto.
    DeteccaoLado d3(4);
    d3.celula(0, um, fechado, 3);
    TEST_ASSERT_EQUAL(LADO_DIREITA, d3.escolher(LADO_ESQUERDA));
    // Sem nenhuma abertura vista: usa o padrão.
    DeteccaoLado d4(4);
    d4.celula(0, fechado, fechado, 3);
    TEST_ASSERT_EQUAL(LADO_ESQUERDA, d4.escolher(LADO_ESQUERDA));
}


// Labirinto da corrida de 23/11 (reconstruído do registro), com o start no canto
// esquerdo: beco em (0,1), corredor (0,0)-(3,0), subida até o beco em (3,2) e o caminho
// certo por (1,0)-(1,3)-(3,3). Ao voltar do beco em (3,2), o robô real passa de (3,1) e
// para em (3,0), contando uma célula só.
static LabirintoReal labirinto_da_corrida() {
    LabirintoReal r;
    r.largura = 4;
    r.altura = 4;
    auto abrir = [&](int x, int y, Direcao d) {
        int nx = x, ny = y;
        avancar(d, nx, ny);
        r.aberto[x][y] |= d;
        r.aberto[nx][ny] |= oposta(d);
    };
    abrir(0, 0, NORTE);  // beco (0,1)
    abrir(0, 0, LESTE);
    abrir(1, 0, LESTE);
    abrir(2, 0, LESTE);
    abrir(3, 0, NORTE);
    abrir(3, 1, NORTE);  // beco (3,2)
    abrir(1, 0, NORTE);
    abrir(1, 1, NORTE);
    abrir(1, 2, NORTE);
    abrir(1, 3, LESTE);
    abrir(2, 3, LESTE);  // objetivo (3,3)
    abrir(1, 2, OESTE);
    abrir(0, 2, NORTE);
    abrir(1, 1, LESTE);
    abrir(2, 1, NORTE);
    return r;
}

// Sem a correção, a posição se perde depois da meia-volta (como na corrida); com
// Passagens::corrigir, o robô chega ao objetivo. Nos dois cantos de start.
void test_corrige_posicao_depois_da_meia_volta() {
    LabirintoReal real = labirinto_da_corrida();
    LabirintoReal espelho = real;
    for (int x = 0; x < 4; x++)
        for (int y = 0; y < 4; y++)
            espelho.aberto[3 - x][y] = espelho_paredes(real.aberto[x][y], true);
    // Conferência: sem o erro de contagem, as duas corridas chegam.
    TEST_ASSERT_EQUAL(CHEGOU, explorar_auto(real, true, 0.0f, 1, LADO_ESQUERDA).resultado);
    TEST_ASSERT_EQUAL(CHEGOU, explorar_auto(espelho, false, 0.0f, 1, LADO_ESQUERDA).resultado);
    // Com o erro e sem a correção: perde a posição.
    TEST_ASSERT_NOT_EQUAL(CHEGOU,
                          explorar_auto(real, true, 0.0f, 1, LADO_ESQUERDA, true, false).resultado);
    // Com o erro e a correção: chega, nos dois cantos.
    TEST_ASSERT_EQUAL(CHEGOU,
                      explorar_auto(real, true, 0.0f, 1, LADO_ESQUERDA, true, true).resultado);
    TEST_ASSERT_EQUAL(CHEGOU,
                      explorar_auto(espelho, false, 0.0f, 1, LADO_ESQUERDA, true, true).resultado);
    // Labirintos sorteados, com o erro em toda meia-volta: a correção não atrapalha.
    for (uint32_t semente = 1; semente <= 300; semente++) {
        LabirintoReal r = gerar(4, 4, semente);
        TEST_ASSERT_EQUAL(CHEGOU,
                          explorar_auto(r, true, 0.0f, semente, LADO_ESQUERDA, true, true).resultado);
    }
}

void test_passagens_unidade() {
    Labirinto mapa(4, 4);
    Passagens p(4, 4);
    p.marcar(3, 0, NORTE);  // (3,0) -> (3,1)
    p.marcar(3, 1, NORTE);  // (3,1) -> (3,2)
    TEST_ASSERT_TRUE(p.aberta(3, 1, SUL));
    TEST_ASSERT_TRUE(p.aberta(3, 2, SUL));
    // Acha que está em (3,2) virado para o sul, mas vê parede: está em (3,0).
    int x = 3, y = 2;
    TEST_ASSERT_EQUAL(2, p.corrigir(x, y, SUL, mapa));
    TEST_ASSERT_EQUAL(3, x);
    TEST_ASSERT_EQUAL(0, y);
    // Sem passagem conhecida à frente: não mexe.
    x = 3; y = 0;
    TEST_ASSERT_EQUAL(0, p.corrigir(x, y, SUL, mapa));
    TEST_ASSERT_EQUAL(0, y);
}


// Paredes falsas durante a corrida toda (lado aberto lido como parede): sem "esquecer o
// mapa" o robô para com "sem caminho"; com ele, sempre chega (o robô não pode desistir no
// dia da apresentação).
void test_nao_desiste_com_paredes_falsas() {
    const float taxas[] = {0.05f, 0.10f};
    for (float taxa : taxas) {
        int sem = 0, com = 0, total = 0;
        for (uint32_t semente = 1; semente <= 300; semente++) {
            for (int canto = 0; canto < 2; canto++) {
                bool esquerdo = canto == 0;
                LabirintoReal r = gerar(4, 4, semente);
                if (!esquerdo) {
                    LabirintoReal base = r;
                    for (int x = 0; x < 4; x++)
                        for (int y = 0; y < 4; y++)
                            r.aberto[3 - x][y] = espelho_paredes(base.aberto[x][y], true);
                }
                total++;
                sem += explorar_auto(r, esquerdo, 0.0f, semente, LADO_ESQUERDA, false, true, taxa,
                                     false).resultado == CHEGOU;
                com += explorar_auto(r, esquerdo, 0.0f, semente, LADO_ESQUERDA, false, true, taxa,
                                     true).resultado == CHEGOU;
            }
        }
        printf("  paredes falsas %.0f%%: sem esquecer o mapa %d/%d, esquecendo %d/%d\n",
               taxa * 100, sem, total, com, total);
        TEST_ASSERT_EQUAL(total, com);
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_lado_no_mapa_e_passos);
    RUN_TEST(test_start_canto_esquerdo_4x4);
    RUN_TEST(test_start_canto_direito_4x4);
    RUN_TEST(test_start_canto_esquerdo_8x4);
    RUN_TEST(test_start_canto_direito_8x4);
    RUN_TEST(test_start_canto_esquerdo_12x4);
    RUN_TEST(test_start_canto_direito_12x4);
    RUN_TEST(test_espelho_faz_a_mesma_corrida);
    RUN_TEST(test_configuracao_errada_nao_resolve);
    RUN_TEST(test_deteccao_unidade);
    RUN_TEST(test_auto_sem_ruido_resolve_os_dois_lados);
    RUN_TEST(test_auto_com_leitura_falsa);
    RUN_TEST(test_passagens_unidade);
    RUN_TEST(test_corrige_posicao_depois_da_meia_volta);
    RUN_TEST(test_nao_desiste_com_paredes_falsas);
    return UNITY_END();
}
