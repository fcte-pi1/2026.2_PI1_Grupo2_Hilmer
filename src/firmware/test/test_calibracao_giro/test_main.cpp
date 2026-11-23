// Testes da calibração da escala do giroscópio. Rodar com: pio test -e native
#include <unity.h>

#include <cmath>
#include <cstdlib>
#include <vector>

#include "navegacao/calibracao_giro.h"

void setUp() {}
void tearDown() {}

static const float PI_F = 3.14159265f;

// Robô girando no centro de uma célula de 180 mm com paredes nos lados indicados (graus
// reais em que o robô fica de frente para cada parede). O sensor fica "deslocamento" mm
// à frente do centro de giro e enxerga um cone de "meio_cone" graus. O giroscópio mede
// "escala" vezes o giro real e começa em "inicio" graus reais.
static std::vector<AmostraGiro> girar(float escala, float voltas_reais, int sentido,
                                      const std::vector<float> &paredes, float ruido_mm,
                                      float inicio, unsigned semente) {
    srand(semente);
    const float centro_parede = 90.0f;  // centro da célula até a face da parede
    const float deslocamento = 40.0f;
    const float meio_cone = 12.0f;
    std::vector<AmostraGiro> amostras;
    for (float real = 0; real <= voltas_reais * 360.0f; real += 1.2f) {
        float direcao = inicio + sentido * real;  // graus reais
        float menor = 65535.0f;
        for (float parede : paredes) {
            // Diferença de ângulo entre -180 e 180 (fmodf de negativo dá resto negativo).
            float delta = fmodf(direcao - parede + 180.0f, 360.0f);
            if (delta < 0.0f) {
                delta += 360.0f;
            }
            delta -= 180.0f;
            // O cone achata o fundo do mínimo: dentro dele, a menor distância é a da normal.
            float efetivo = fabsf(delta) <= meio_cone ? 0.0f : fabsf(delta) - meio_cone;
            if (efetivo >= 60.0f) {
                continue;
            }
            float d = centro_parede / cosf(efetivo * PI_F / 180.0f) - deslocamento;
            if (d < menor) {
                menor = d;
            }
        }
        float ruido = ruido_mm * ((rand() / (float)RAND_MAX) * 2.0f - 1.0f);
        uint16_t frente = menor >= 2000.0f ? 65535 : (uint16_t)lroundf(menor + ruido);
        amostras.push_back({sentido * real * escala, frente});
    }
    return amostras;
}

void test_parede_unica_sem_ruido_mede_a_escala() {
    auto a = girar(0.944f, 2.5f, 1, {0.0f}, 0.0f, 30.0f, 1);
    ResultadoCalibracaoGiro r = calibracao_giro_analisar(a.data(), a.size());
    TEST_ASSERT_GREATER_OR_EQUAL(1, r.voltas);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.944f * 360.0f, r.graus_por_volta);
}

// Beco sem saída: três paredes a 90° uma da outra, com ruído de ±3 mm.
void test_beco_com_ruido_mede_a_escala() {
    auto a = girar(0.944f, 2.5f, 1, {0.0f, 90.0f, 270.0f}, 3.0f, 10.0f, 2);
    ResultadoCalibracaoGiro r = calibracao_giro_analisar(a.data(), a.size());
    TEST_ASSERT_GREATER_OR_EQUAL(2, r.voltas);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.944f * 360.0f, r.graus_por_volta);
}

// Girando para a direita o ângulo cai: a análise tem que funcionar igual.
void test_giro_para_a_direita() {
    auto a = girar(1.03f, 2.5f, -1, {0.0f, 90.0f, 270.0f}, 3.0f, 200.0f, 3);
    ResultadoCalibracaoGiro r = calibracao_giro_analisar(a.data(), a.size());
    // As voltas a mais de 3° da mediana (ruído no fundo do mínimo) ficam de fora.
    TEST_ASSERT_GREATER_OR_EQUAL(2, r.voltas);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 1.03f * 360.0f, r.graus_por_volta);
}

// Paredes a 90° uma da outra não podem ser confundidas com uma volta.
void test_nao_confunde_paredes_vizinhas_com_volta() {
    auto a = girar(1.0f, 0.9f, 1, {0.0f, 90.0f, 180.0f, 270.0f}, 0.0f, 20.0f, 4);
    ResultadoCalibracaoGiro r = calibracao_giro_analisar(a.data(), a.size());
    TEST_ASSERT_GREATER_OR_EQUAL(3, r.minimos);
    TEST_ASSERT_EQUAL(0, r.voltas);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.graus_por_volta);
}

void test_sem_parede_nao_mede() {
    auto a = girar(1.0f, 2.0f, 1, {}, 0.0f, 0.0f, 5);
    ResultadoCalibracaoGiro r = calibracao_giro_analisar(a.data(), a.size());
    TEST_ASSERT_EQUAL(0, r.minimos);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.graus_por_volta);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_parede_unica_sem_ruido_mede_a_escala);
    RUN_TEST(test_beco_com_ruido_mede_a_escala);
    RUN_TEST(test_giro_para_a_direita);
    RUN_TEST(test_nao_confunde_paredes_vizinhas_com_volta);
    RUN_TEST(test_sem_parede_nao_mede);
    return UNITY_END();
}
