// Testes da contagem de células por trecho e do equilíbrio das rodas.
// Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/contagem_celulas.h"
#include "navegacao/equilibrio_rodas.h"

void setUp() {}
void tearDown() {}

static const float CELULA = 180.0f;

// O caso da corrida: uma célula pelos encoders, frente de 383 a 53 mm (leitura errada de
// longe). A contagem antiga pela frente dava 2 células; agora vale o encoder.
void test_leitura_longa_errada_nao_conta_duas_celulas() {
    LeituraTrecho l = {1429, 1429, 383, 53};
    TEST_ASSERT_EQUAL(1, contagem_celulas(l, CELULA));
}

void test_uma_celula_pelo_encoder_sem_parede() {
    LeituraTrecho l = {1400, 1429, CONTAGEM_SEM_LEITURA, CONTAGEM_SEM_LEITURA};
    TEST_ASSERT_EQUAL(1, contagem_celulas(l, CELULA));
}

void test_parado_pela_parede_logo_na_frente_e_zero() {
    LeituraTrecho l = {60, 1429, 70, 52};
    TEST_ASSERT_EQUAL(0, contagem_celulas(l, CELULA));
}

void test_encoder_solto_com_a_parede_perto_vale_a_frente() {
    // Encoder não contou, mas a frente foi de 234 a 54 mm: andou uma célula.
    LeituraTrecho l = {0, 1429, 234, 54};
    TEST_ASSERT_EQUAL(1, contagem_celulas(l, CELULA));
}

void test_encoder_solto_sem_parede_perto_fica_zero() {
    LeituraTrecho l = {0, 1429, 600, 420};
    TEST_ASSERT_EQUAL(0, contagem_celulas(l, CELULA));
}

void test_contagem_negativa_conta_igual() {
    LeituraTrecho l = {-1429, 1429, CONTAGEM_SEM_LEITURA, CONTAGEM_SEM_LEITURA};
    TEST_ASSERT_EQUAL(1, contagem_celulas(l, CELULA));
}

void test_equilibrio_reduz_a_roda_adiantada() {
    // Esquerda andou 20 contagens a mais (em módulo): c > 0 reduz a esquerda.
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, equilibrio_rodas(-120, 100, 0.01f, 0.3f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.2f, equilibrio_rodas(100, -120, 0.01f, 0.3f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, equilibrio_rodas(-100, 100, 0.01f, 0.3f));
}

void test_equilibrio_limitado() {
    TEST_ASSERT_EQUAL_FLOAT(0.3f, equilibrio_rodas(500, 0, 0.01f, 0.3f));
    TEST_ASSERT_EQUAL_FLOAT(-0.3f, equilibrio_rodas(0, -500, 0.01f, 0.3f));
}

void test_fatores_so_aceleram_a_roda_atrasada() {
    float fe, fd;
    equilibrio_fatores(0.1f, fe, fd);  // esquerda adiantada: acelera a direita
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fe);
    TEST_ASSERT_EQUAL_FLOAT(1.1f, fd);
    equilibrio_fatores(-0.1f, fe, fd);  // direita adiantada: acelera a esquerda
    TEST_ASSERT_EQUAL_FLOAT(1.1f, fe);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fd);
    equilibrio_fatores(0.0f, fe, fd);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fe);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fd);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_leitura_longa_errada_nao_conta_duas_celulas);
    RUN_TEST(test_uma_celula_pelo_encoder_sem_parede);
    RUN_TEST(test_parado_pela_parede_logo_na_frente_e_zero);
    RUN_TEST(test_encoder_solto_com_a_parede_perto_vale_a_frente);
    RUN_TEST(test_encoder_solto_sem_parede_perto_fica_zero);
    RUN_TEST(test_contagem_negativa_conta_igual);
    RUN_TEST(test_equilibrio_reduz_a_roda_adiantada);
    RUN_TEST(test_equilibrio_limitado);
    RUN_TEST(test_fatores_so_aceleram_a_roda_atrasada);
    return UNITY_END();
}
