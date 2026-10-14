// Testes das regras da bateria. Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/bateria.h"

void setUp() {}
void tearDown() {}

void test_duty_maximo_e_tensao_do_motor_sobre_a_da_bateria() {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f / 8.4f, bateria_duty_maximo(6.0f, 8.4f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 6.0f / 7.4f, bateria_duty_maximo(6.0f, 7.4f));
}

void test_duty_maximo_fica_em_100_por_cento_com_bateria_abaixo_do_motor() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, bateria_duty_maximo(6.0f, 5.0f));
}

void test_duty_maximo_e_zero_sem_leitura_de_tensao() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, bateria_duty_maximo(6.0f, 0.0f));
}

void test_bateria_acima_do_aviso_esta_normal() {
    TEST_ASSERT_EQUAL(BATERIA_NORMAL, bateria_estado(7.4f));
    TEST_ASSERT_EQUAL(BATERIA_NORMAL, bateria_estado(7.0f));
}

void test_bateria_abaixo_do_aviso_esta_baixa() {
    TEST_ASSERT_EQUAL(BATERIA_BAIXA, bateria_estado(6.9f));
    TEST_ASSERT_EQUAL(BATERIA_BAIXA, bateria_estado(6.6f));
}

void test_bateria_abaixo_de_6_6_volts_bloqueia() {
    TEST_ASSERT_EQUAL(BATERIA_BLOQUEADA, bateria_estado(6.59f));
    TEST_ASSERT_EQUAL(BATERIA_BLOQUEADA, bateria_estado(0.0f));
}

void test_carga_e_100_com_bateria_cheia_e_0_no_bloqueio() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bateria_carga(8.4f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, bateria_carga(6.6f));
}

void test_carga_fica_entre_0_e_100() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bateria_carga(9.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, bateria_carga(6.0f));
}

void test_carga_segue_a_tabela_da_lipo() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, bateria_carga(7.60f));
}

void test_carga_interpola_entre_pontos_da_tabela() {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, bateria_carga(7.67f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_duty_maximo_e_tensao_do_motor_sobre_a_da_bateria);
    RUN_TEST(test_duty_maximo_fica_em_100_por_cento_com_bateria_abaixo_do_motor);
    RUN_TEST(test_duty_maximo_e_zero_sem_leitura_de_tensao);
    RUN_TEST(test_bateria_acima_do_aviso_esta_normal);
    RUN_TEST(test_bateria_abaixo_do_aviso_esta_baixa);
    RUN_TEST(test_bateria_abaixo_de_6_6_volts_bloqueia);
    RUN_TEST(test_carga_e_100_com_bateria_cheia_e_0_no_bloqueio);
    RUN_TEST(test_carga_fica_entre_0_e_100);
    RUN_TEST(test_carga_segue_a_tabela_da_lipo);
    RUN_TEST(test_carga_interpola_entre_pontos_da_tabela);
    return UNITY_END();
}
