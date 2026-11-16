// Testes da centralização pelas paredes laterais. Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/centralizacao.h"

void setUp() {}
void tearDown() {}

// Valores do robô: no centro, a esquerda lê 23 mm e a direita 44 mm.
static const CalibracaoCentro CAL = {23.0f, 44.0f, 100, 0.5f, 10.0f};

void test_no_centro_nao_inclina_com_duas_paredes() {
    float desvio;
    TEST_ASSERT_TRUE(centralizacao_desvio(23, 44, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, desvio);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, centralizacao_inclinacao(23, 44, CAL));
}

// Com a regra antiga (esquerda = direita), o centro de verdade dava 21 mm de erro.
void test_leituras_diferentes_no_centro_nao_sao_erro() {
    float desvio;
    centralizacao_desvio(23, 44, CAL, desvio);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, desvio);
}

void test_robo_a_direita_inclina_para_a_esquerda() {
    float desvio;
    // 10 mm para a direita: esquerda lê 33, direita 34.
    TEST_ASSERT_TRUE(centralizacao_desvio(33, 34, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(10.0f, desvio);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, centralizacao_inclinacao(33, 34, CAL));
}

void test_robo_a_esquerda_inclina_para_a_direita() {
    TEST_ASSERT_EQUAL_FLOAT(-4.0f, centralizacao_inclinacao(15, 52, CAL));
}

void test_so_parede_esquerda_usa_o_centro_da_esquerda() {
    float desvio;
    TEST_ASSERT_TRUE(centralizacao_desvio(23, 65535, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, desvio);
    TEST_ASSERT_TRUE(centralizacao_desvio(30, 65535, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, desvio);
}

void test_so_parede_direita_usa_o_centro_da_direita() {
    float desvio;
    TEST_ASSERT_TRUE(centralizacao_desvio(250, 44, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, desvio);
    TEST_ASSERT_TRUE(centralizacao_desvio(250, 50, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(-6.0f, desvio);
}

void test_sem_parede_nao_inclina() {
    float desvio = 99.0f;
    TEST_ASSERT_FALSE(centralizacao_desvio(65535, 300, CAL, desvio));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, desvio);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, centralizacao_inclinacao(65535, 300, CAL));
}

void test_inclinacao_limitada() {
    TEST_ASSERT_EQUAL_FLOAT(10.0f, centralizacao_inclinacao(90, 10, CAL));
    TEST_ASSERT_EQUAL_FLOAT(-10.0f, centralizacao_inclinacao(0, 95, CAL));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_no_centro_nao_inclina_com_duas_paredes);
    RUN_TEST(test_leituras_diferentes_no_centro_nao_sao_erro);
    RUN_TEST(test_robo_a_direita_inclina_para_a_esquerda);
    RUN_TEST(test_robo_a_esquerda_inclina_para_a_direita);
    RUN_TEST(test_so_parede_esquerda_usa_o_centro_da_esquerda);
    RUN_TEST(test_so_parede_direita_usa_o_centro_da_direita);
    RUN_TEST(test_sem_parede_nao_inclina);
    RUN_TEST(test_inclinacao_limitada);
    return UNITY_END();
}
