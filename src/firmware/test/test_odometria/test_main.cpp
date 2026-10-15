// Testes da odometria. Rodar com: pio test -e native
#include <unity.h>

#include "navegacao/odometria.h"

static const float PI_F = 3.14159265f;

void setUp() {}
void tearDown() {}

void test_uma_volta_da_roda_anda_o_perimetro() {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, PI_F * 22.0f, odometria_distancia_mm(1000, 22.0f, 1000));
}

void test_meia_volta_anda_metade_do_perimetro() {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, PI_F * 22.0f / 2.0f, odometria_distancia_mm(500, 22.0f, 1000));
}

void test_contagem_negativa_e_distancia_para_tras() {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -PI_F * 22.0f, odometria_distancia_mm(-1000, 22.0f, 1000));
}

void test_distancia_e_zero_sem_contagens_por_volta() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, odometria_distancia_mm(1000, 22.0f, 0));
}

void test_rodas_iguais_nao_giram() {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, odometria_angulo_graus(100.0f, 100.0f, 80.0f));
}

void test_roda_direita_andando_mais_gira_para_a_esquerda() {
    float arco_90_graus = PI_F * 80.0f / 2.0f;

    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 90.0f, odometria_angulo_graus(0.0f, arco_90_graus, 80.0f));
}

void test_giro_no_lugar_soma_as_duas_rodas() {
    float meio_arco = PI_F * 80.0f / 4.0f;

    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 90.0f, odometria_angulo_graus(-meio_arco, meio_arco, 80.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-2f, -90.0f, odometria_angulo_graus(meio_arco, -meio_arco, 80.0f));
}

void test_angulo_e_zero_sem_distancia_entre_rodas() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, odometria_angulo_graus(0.0f, 50.0f, 0.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_uma_volta_da_roda_anda_o_perimetro);
    RUN_TEST(test_meia_volta_anda_metade_do_perimetro);
    RUN_TEST(test_contagem_negativa_e_distancia_para_tras);
    RUN_TEST(test_distancia_e_zero_sem_contagens_por_volta);
    RUN_TEST(test_rodas_iguais_nao_giram);
    RUN_TEST(test_roda_direita_andando_mais_gira_para_a_esquerda);
    RUN_TEST(test_giro_no_lugar_soma_as_duas_rodas);
    RUN_TEST(test_angulo_e_zero_sem_distancia_entre_rodas);
    return UNITY_END();
}
