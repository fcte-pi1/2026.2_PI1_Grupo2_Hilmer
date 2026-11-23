// Testes da conversão do mapa espelhado para as coordenadas reais.
// Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/espelho.h"

void setUp() {}
void tearDown() {}

void test_sem_espelho_nao_muda() {
    TEST_ASSERT_EQUAL(1, espelho_x(1, 4, false));
    TEST_ASSERT_EQUAL(LESTE, espelho_direcao(LESTE, false));
    TEST_ASSERT_EQUAL(NORTE | LESTE, espelho_paredes(NORTE | LESTE, false));
}

void test_start_vai_para_o_canto_direito() {
    // Start (0,0) do mapa espelhado num labirinto de 4 de largura: x real 3.
    TEST_ASSERT_EQUAL(3, espelho_x(0, 4, true));
    TEST_ASSERT_EQUAL(0, espelho_x(3, 4, true));
}

void test_leste_e_oeste_trocam() {
    TEST_ASSERT_EQUAL(OESTE, espelho_direcao(LESTE, true));
    TEST_ASSERT_EQUAL(LESTE, espelho_direcao(OESTE, true));
    TEST_ASSERT_EQUAL(NORTE, espelho_direcao(NORTE, true));
    TEST_ASSERT_EQUAL(SUL, espelho_direcao(SUL, true));
}

void test_paredes_laterais_trocam() {
    TEST_ASSERT_EQUAL(NORTE | OESTE, espelho_paredes(NORTE | LESTE, true));
    TEST_ASSERT_EQUAL(SUL | LESTE, espelho_paredes(SUL | OESTE, true));
    TEST_ASSERT_EQUAL(LESTE | OESTE, espelho_paredes(LESTE | OESTE, true));
    TEST_ASSERT_EQUAL(0, espelho_paredes(0, true));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_sem_espelho_nao_muda);
    RUN_TEST(test_start_vai_para_o_canto_direito);
    RUN_TEST(test_leste_e_oeste_trocam);
    RUN_TEST(test_paredes_laterais_trocam);
    return UNITY_END();
}
