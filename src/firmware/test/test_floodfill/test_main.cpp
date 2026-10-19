// Testes do Flood Fill. Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/floodfill.h"

void setUp() {}
void tearDown() {}

static const Direcao DIRECOES[] = {NORTE, LESTE, SUL, OESTE};

// Labirinto em zigue-zague: cada linha só se liga à de cima por uma ponta,
// alternando entre a leste e a oeste. Assim o caminho mais curto passa por todas as linhas.
static void montar_zigue_zague(Labirinto &labirinto) {
    for (int y = 0; y < labirinto.altura() - 1; y++) {
        int abertura = (y % 2 == 0) ? labirinto.largura() - 1 : 0;
        for (int x = 0; x < labirinto.largura(); x++) {
            if (x != abertura) {
                labirinto.colocarParede(x, y, NORTE);
            }
        }
    }
}

// Simula a corrida: o robô só conhece as paredes das células por onde passou.
// Retorna quantas células ele andou até o objetivo, ou -1 se não chegou.
static int explorar(const Labirinto &real, int limite_passos) {
    Labirinto mapa(real.largura(), real.altura());
    FloodFill floodfill(mapa);
    int x = 0;
    int y = 0;
    Direcao direcao = NORTE;

    for (int passos = 0; passos < limite_passos; passos++) {
        if (floodfill.objetivo(x, y)) {
            return passos;
        }
        for (Direcao d : DIRECOES) {
            if (real.temParede(x, y, d)) {
                mapa.colocarParede(x, y, d);
            }
        }
        Direcao proxima;
        if (!floodfill.proximaDirecao(x, y, direcao, proxima)) {
            return -1;
        }
        // O robô nunca pode mandar andar contra uma parede de verdade.
        TEST_ASSERT_FALSE(real.temParede(x, y, proxima));
        avancar(proxima, x, y);
        direcao = proxima;
    }
    return -1;
}

void test_objetivo_e_o_canto_oposto_a_saida() {
    Labirinto labirinto(8, 4);
    FloodFill floodfill(labirinto);

    TEST_ASSERT_TRUE(floodfill.objetivo(7, 3));
    TEST_ASSERT_FALSE(floodfill.objetivo(0, 0));
    TEST_ASSERT_FALSE(floodfill.objetivo(7, 0));
}

void test_sem_paredes_internas_a_distancia_e_a_de_manhattan() {
    Labirinto labirinto(4, 4);
    FloodFill floodfill(labirinto);
    floodfill.calcular();

    TEST_ASSERT_EQUAL(0, floodfill.distancia(3, 3));
    TEST_ASSERT_EQUAL(1, floodfill.distancia(2, 3));
    TEST_ASSERT_EQUAL(6, floodfill.distancia(0, 0));
}

void test_parede_aumenta_a_distancia() {
    Labirinto labirinto(4, 4);
    labirinto.colocarParede(2, 3, LESTE);
    labirinto.colocarParede(3, 2, NORTE);
    FloodFill floodfill(labirinto);
    floodfill.calcular();

    // As duas entradas do objetivo estão fechadas.
    TEST_ASSERT_EQUAL(DISTANCIA_INFINITA, floodfill.distancia(2, 3));
}

void test_prefere_seguir_reto_quando_empata() {
    Labirinto labirinto(4, 4);
    FloodFill floodfill(labirinto);
    Direcao proxima;

    // De (0, 0), ir ao norte ou ao leste dá a mesma distância.
    TEST_ASSERT_TRUE(floodfill.proximaDirecao(0, 0, LESTE, proxima));
    TEST_ASSERT_EQUAL(LESTE, proxima);
    TEST_ASSERT_TRUE(floodfill.proximaDirecao(0, 0, NORTE, proxima));
    TEST_ASSERT_EQUAL(NORTE, proxima);
}

void test_recalcula_o_caminho_quando_descobre_uma_parede() {
    Labirinto labirinto(4, 4);
    FloodFill floodfill(labirinto);
    Direcao proxima;

    TEST_ASSERT_TRUE(floodfill.proximaDirecao(0, 0, NORTE, proxima));
    TEST_ASSERT_EQUAL(NORTE, proxima);

    labirinto.colocarParede(0, 0, NORTE);

    TEST_ASSERT_TRUE(floodfill.proximaDirecao(0, 0, NORTE, proxima));
    TEST_ASSERT_EQUAL(LESTE, proxima);
}

void test_no_objetivo_nao_ha_proxima_direcao() {
    Labirinto labirinto(4, 4);
    FloodFill floodfill(labirinto);
    Direcao proxima;

    TEST_ASSERT_FALSE(floodfill.proximaDirecao(3, 3, NORTE, proxima));
}

void test_sem_caminho_ate_o_objetivo_nao_ha_proxima_direcao() {
    Labirinto labirinto(4, 4);
    labirinto.colocarParede(0, 0, NORTE);
    labirinto.colocarParede(0, 0, LESTE);
    FloodFill floodfill(labirinto);
    Direcao proxima;

    TEST_ASSERT_FALSE(floodfill.proximaDirecao(0, 0, NORTE, proxima));
}

void test_com_o_mapa_conhecido_segue_o_caminho_mais_curto() {
    Labirinto labirinto(4, 4);
    montar_zigue_zague(labirinto);
    FloodFill floodfill(labirinto);
    floodfill.calcular();

    // 3 linhas de 3 células para o lado, mais 3 subidas.
    TEST_ASSERT_EQUAL(12, floodfill.distancia(0, 0));
}

void test_explorando_chega_ao_objetivo_no_4x4() {
    Labirinto real(4, 4);
    montar_zigue_zague(real);
    TEST_ASSERT_NOT_EQUAL(-1, explorar(real, 200));
}

void test_explorando_chega_ao_objetivo_no_8x4() {
    Labirinto real(8, 4);
    montar_zigue_zague(real);
    TEST_ASSERT_NOT_EQUAL(-1, explorar(real, 200));
}

void test_explorando_chega_ao_objetivo_no_12x4() {
    Labirinto real(12, 4);
    montar_zigue_zague(real);
    TEST_ASSERT_NOT_EQUAL(-1, explorar(real, 200));
}

void test_explorando_sem_paredes_internas_anda_o_minimo() {
    Labirinto real(12, 4);
    TEST_ASSERT_EQUAL(14, explorar(real, 200));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_objetivo_e_o_canto_oposto_a_saida);
    RUN_TEST(test_sem_paredes_internas_a_distancia_e_a_de_manhattan);
    RUN_TEST(test_parede_aumenta_a_distancia);
    RUN_TEST(test_prefere_seguir_reto_quando_empata);
    RUN_TEST(test_recalcula_o_caminho_quando_descobre_uma_parede);
    RUN_TEST(test_no_objetivo_nao_ha_proxima_direcao);
    RUN_TEST(test_sem_caminho_ate_o_objetivo_nao_ha_proxima_direcao);
    RUN_TEST(test_com_o_mapa_conhecido_segue_o_caminho_mais_curto);
    RUN_TEST(test_explorando_chega_ao_objetivo_no_4x4);
    RUN_TEST(test_explorando_chega_ao_objetivo_no_8x4);
    RUN_TEST(test_explorando_chega_ao_objetivo_no_12x4);
    RUN_TEST(test_explorando_sem_paredes_internas_anda_o_minimo);
    return UNITY_END();
}
