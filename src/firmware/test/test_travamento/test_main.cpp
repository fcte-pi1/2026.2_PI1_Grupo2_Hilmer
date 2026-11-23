// Testes do detector de travamento na reta.
// Rodar com: pio test -e native -f test_travamento
#include <unity.h>

#include "navegacao/travamento.h"

void setUp() {}
void tearDown() {}

// Janela de 400 ms, 60 contagens mínimas, carência de 300 ms (como no explorar).
static DetectorTravamento novo() { return DetectorTravamento(400, 60, 300); }

void test_andando_normal_nao_trava() {
    DetectorTravamento d = novo();
    d.reiniciar(0, 0);
    // ~1500 contagens/s, ciclos de 10 ms.
    for (uint32_t t = 0; t <= 3000; t += 10) {
        TEST_ASSERT_FALSE(d.atualizar(t, (int32_t)(t * 3 / 2)));
    }
}

void test_parado_contra_a_parede_trava() {
    DetectorTravamento d = novo();
    d.reiniciar(0, 0);
    bool travou = false;
    uint32_t quando = 0;
    for (uint32_t t = 0; t <= 2000 && !travou; t += 10) {
        // Andou até 500 ms e encostou: a contagem para em 750.
        int32_t c = t < 500 ? (int32_t)(t * 3 / 2) : 750;
        if (d.atualizar(t, c)) {
            travou = true;
            quando = t;
        }
    }
    TEST_ASSERT_TRUE(travou);
    // Trava cerca de 400 ms depois de parar de andar.
    TEST_ASSERT_UINT32_WITHIN(60, 900, quando);
}

void test_carencia_no_arranque() {
    DetectorTravamento d = novo();
    d.reiniciar(1000, 0);
    // Parado nos primeiros 300 ms (a roda ainda vencendo a inércia): não conta.
    for (uint32_t t = 1000; t < 1300; t += 10) {
        TEST_ASSERT_FALSE(d.atualizar(t, 0));
    }
}

void test_quase_parado_tambem_trava() {
    DetectorTravamento d = novo();
    d.reiniciar(0, 0);
    bool travou = false;
    // Arrastando: 50 contagens/s (menos de 60 em 400 ms).
    for (uint32_t t = 0; t <= 2000 && !travou; t += 10) {
        travou = d.atualizar(t, (int32_t)(t / 20));
    }
    TEST_ASSERT_TRUE(travou);
}

void test_depois_de_travar_recomeca() {
    DetectorTravamento d = novo();
    d.reiniciar(0, 0);
    uint32_t t = 0;
    while (!d.atualizar(t, 0)) {
        t += 10;
    }
    // Logo depois não avisa de novo; só depois de outra janela sem progresso.
    TEST_ASSERT_FALSE(d.atualizar(t + 10, 0));
    bool de_novo = false;
    for (uint32_t k = t + 20; k <= t + 500 && !de_novo; k += 10) {
        de_novo = d.atualizar(k, 0);
    }
    TEST_ASSERT_TRUE(de_novo);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_andando_normal_nao_trava);
    RUN_TEST(test_parado_contra_a_parede_trava);
    RUN_TEST(test_carencia_no_arranque);
    RUN_TEST(test_quase_parado_tambem_trava);
    RUN_TEST(test_depois_de_travar_recomeca);
    return UNITY_END();
}
