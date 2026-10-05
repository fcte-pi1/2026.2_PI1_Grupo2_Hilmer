// Testes do mapa de paredes. Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/labirinto.h"

void setUp() {}
void tearDown() {}

void test_labirinto_novo_tem_paredes_na_borda() {
    Labirinto labirinto(4, 4);

    TEST_ASSERT_TRUE(labirinto.temParede(0, 0, SUL));
    TEST_ASSERT_TRUE(labirinto.temParede(0, 0, OESTE));
    TEST_ASSERT_TRUE(labirinto.temParede(3, 3, NORTE));
    TEST_ASSERT_TRUE(labirinto.temParede(3, 3, LESTE));
}

void test_labirinto_novo_nao_tem_paredes_internas() {
    Labirinto labirinto(4, 4);

    TEST_ASSERT_FALSE(labirinto.temParede(1, 1, NORTE));
    TEST_ASSERT_FALSE(labirinto.temParede(1, 1, LESTE));
    TEST_ASSERT_FALSE(labirinto.temParede(1, 1, SUL));
    TEST_ASSERT_FALSE(labirinto.temParede(1, 1, OESTE));
}

void test_colocar_parede_marca_tambem_a_celula_vizinha() {
    Labirinto labirinto(4, 4);

    labirinto.colocarParede(1, 1, LESTE);

    TEST_ASSERT_TRUE(labirinto.temParede(1, 1, LESTE));
    TEST_ASSERT_TRUE(labirinto.temParede(2, 1, OESTE));
}

void test_fora_do_mapa_conta_como_parede() {
    Labirinto labirinto(4, 4);

    TEST_ASSERT_FALSE(labirinto.dentro(4, 0));
    TEST_ASSERT_TRUE(labirinto.temParede(4, 0, OESTE));
    TEST_ASSERT_TRUE(labirinto.temParede(-1, 2, NORTE));
}

void test_labirinto_12x4_e_o_maior_aceito() {
    Labirinto grande(12, 4);
    Labirinto exagerado(20, 10);

    TEST_ASSERT_EQUAL(12, grande.largura());
    TEST_ASSERT_EQUAL(4, grande.altura());
    TEST_ASSERT_EQUAL(12, exagerado.largura());
    TEST_ASSERT_EQUAL(4, exagerado.altura());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_labirinto_novo_tem_paredes_na_borda);
    RUN_TEST(test_labirinto_novo_nao_tem_paredes_internas);
    RUN_TEST(test_colocar_parede_marca_tambem_a_celula_vizinha);
    RUN_TEST(test_fora_do_mapa_conta_como_parede);
    RUN_TEST(test_labirinto_12x4_e_o_maior_aceito);
    return UNITY_END();
}
