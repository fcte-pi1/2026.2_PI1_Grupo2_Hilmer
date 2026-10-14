// Testes do controlador PID. Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/pid.h"

void setUp() {}
void tearDown() {}

void test_termo_p_e_o_ganho_vezes_o_erro() {
    Pid pid(2.0f, 0.0f, 0.0f, 0.01f, 100.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 6.0f, pid.atualizar(10.0f, 7.0f));
}

void test_termo_i_acumula_o_erro_a_cada_periodo() {
    Pid pid(0.0f, 5.0f, 0.0f, 0.1f, 100.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, pid.atualizar(2.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, pid.atualizar(2.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 3.0f, pid.atualizar(2.0f, 0.0f));
}

void test_termo_d_usa_a_variacao_do_erro() {
    Pid pid(0.0f, 0.0f, 0.5f, 0.1f, 100.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, pid.atualizar(1.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f, pid.atualizar(3.0f, 0.0f));
}

void test_saida_fica_dentro_do_limite() {
    Pid pid(10.0f, 0.0f, 0.0f, 0.01f, 1.0f);

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, pid.atualizar(5.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, pid.atualizar(-5.0f, 0.0f));
}

void test_integral_nao_cresce_com_a_saida_saturada() {
    Pid pid(0.0f, 1.0f, 0.0f, 1.0f, 1.0f);

    for (int i = 0; i < 5; i++) {
        pid.atualizar(10.0f, 0.0f);
    }

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, pid.atualizar(-1.0f, 0.0f));
}

void test_zerar_volta_ao_estado_inicial() {
    Pid pid(0.0f, 1.0f, 1.0f, 0.1f, 100.0f);
    pid.atualizar(5.0f, 0.0f);
    pid.atualizar(5.0f, 0.0f);

    pid.zerar();

    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f, pid.atualizar(2.0f, 0.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_termo_p_e_o_ganho_vezes_o_erro);
    RUN_TEST(test_termo_i_acumula_o_erro_a_cada_periodo);
    RUN_TEST(test_termo_d_usa_a_variacao_do_erro);
    RUN_TEST(test_saida_fica_dentro_do_limite);
    RUN_TEST(test_integral_nao_cresce_com_a_saida_saturada);
    RUN_TEST(test_zerar_volta_ao_estado_inicial);
    return UNITY_END();
}
