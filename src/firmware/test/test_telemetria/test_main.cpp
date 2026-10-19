// Testes do pacote de telemetria. Rodar com: pio test -e native
// Os textos esperados são os exemplos do contrato (src/docs/arquitetura.md, seção 2).
#include <unity.h>

#include <cstring>

#include "navegacao/telemetria.h"

static char saida[TELEMETRIA_TAMANHO_MAX];

void setUp() {}
void tearDown() {}

static DadosTelemetria dados_do_exemplo() {
    DadosTelemetria dados;
    dados.x = 2;
    dados.y = 1;
    dados.direcao = NORTE;
    dados.velocidade_mm_s = 240.5f;
    dados.odometria_mm = 18320;
    dados.bateria_pct = 76.2f;
    dados.paredes = NORTE | OESTE;
    dados.estado = ROBO_EXPLORANDO;
    return dados;
}

void test_pacote_de_telemetria_segue_o_exemplo_do_contrato() {
    Telemetria telemetria("mm-01", 1532);
    telemetria.pacote(153200, dados_do_exemplo(), saida, sizeof(saida));
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"type\":\"telemetry\",\"robot_id\":\"mm-01\",\"seq\":1532,\"t_ms\":153200,"
        "\"pos\":{\"x\":2,\"y\":1},\"heading\":\"N\",\"speed_mm_s\":240.5,\"odo_mm\":18320,"
        "\"battery_pct\":76.2,\"walls\":{\"n\":true,\"e\":false,\"s\":false,\"w\":true},"
        "\"state\":\"EXPLORING\"}",
        saida);
}

void test_pacote_traduz_direcao_e_estado_para_o_contrato() {
    Telemetria telemetria("mm-01");
    DadosTelemetria dados = dados_do_exemplo();
    dados.direcao = OESTE;
    dados.paredes = LESTE | SUL;
    dados.estado = ROBO_OBJETIVO;
    telemetria.pacote(0, dados, saida, sizeof(saida));
    TEST_ASSERT_NOT_NULL(strstr(saida, "\"heading\":\"W\""));
    TEST_ASSERT_NOT_NULL(strstr(saida, "\"walls\":{\"n\":false,\"e\":true,\"s\":true,\"w\":false}"));
    TEST_ASSERT_NOT_NULL(strstr(saida, "\"state\":\"GOAL_REACHED\""));
}

void test_evento_de_calibracao_segue_o_exemplo_do_contrato() {
    Telemetria telemetria("mm-01");
    telemetria.calibracao(0, true, saida, sizeof(saida));
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"type\":\"event\",\"robot_id\":\"mm-01\",\"seq\":0,\"t_ms\":0,"
        "\"event\":\"CALIBRATION_DONE\",\"ok\":true}",
        saida);
}

void test_evento_de_inicio_segue_o_exemplo_do_contrato() {
    Telemetria telemetria("mm-01", 1);
    telemetria.inicio(0, 98.0f, saida, sizeof(saida));
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"type\":\"event\",\"robot_id\":\"mm-01\",\"seq\":1,\"t_ms\":0,"
        "\"event\":\"RUN_START\",\"battery_pct\":98.0}",
        saida);
}

void test_evento_de_objetivo_segue_o_exemplo_do_contrato() {
    Telemetria telemetria("mm-01", 1843);
    telemetria.objetivo(184250, 3, 3, saida, sizeof(saida));
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"type\":\"event\",\"robot_id\":\"mm-01\",\"seq\":1843,\"t_ms\":184250,"
        "\"event\":\"GOAL_REACHED\",\"pos\":{\"x\":3,\"y\":3}}",
        saida);
}

void test_evento_de_fim_segue_o_exemplo_do_contrato() {
    Telemetria telemetria("mm-01", 1900);
    telemetria.fim(190000, saida, sizeof(saida));
    TEST_ASSERT_EQUAL_STRING(
        "{\"v\":1,\"type\":\"event\",\"robot_id\":\"mm-01\",\"seq\":1900,\"t_ms\":190000,"
        "\"event\":\"RUN_END\"}",
        saida);
}

void test_seq_cresce_a_cada_mensagem_incluindo_eventos() {
    Telemetria telemetria("mm-01");
    telemetria.calibracao(0, true, saida, sizeof(saida));
    telemetria.inicio(0, 98.0f, saida, sizeof(saida));
    telemetria.pacote(100, dados_do_exemplo(), saida, sizeof(saida));
    TEST_ASSERT_NOT_NULL(strstr(saida, "\"seq\":2,"));
    TEST_ASSERT_EQUAL_UINT32(3, telemetria.seq());
}

void test_mensagem_que_nao_cabe_retorna_zero_e_nao_gasta_seq() {
    Telemetria telemetria("mm-01");
    char pequena[20];
    TEST_ASSERT_EQUAL_INT(0, telemetria.pacote(0, dados_do_exemplo(), pequena, sizeof(pequena)));
    TEST_ASSERT_EQUAL_UINT32(0, telemetria.seq());
}

void test_retorna_o_tamanho_do_texto_gerado() {
    Telemetria telemetria("mm-01");
    int tamanho = telemetria.fim(0, saida, sizeof(saida));
    TEST_ASSERT_EQUAL_INT((int)strlen(saida), tamanho);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pacote_de_telemetria_segue_o_exemplo_do_contrato);
    RUN_TEST(test_pacote_traduz_direcao_e_estado_para_o_contrato);
    RUN_TEST(test_evento_de_calibracao_segue_o_exemplo_do_contrato);
    RUN_TEST(test_evento_de_inicio_segue_o_exemplo_do_contrato);
    RUN_TEST(test_evento_de_objetivo_segue_o_exemplo_do_contrato);
    RUN_TEST(test_evento_de_fim_segue_o_exemplo_do_contrato);
    RUN_TEST(test_seq_cresce_a_cada_mensagem_incluindo_eventos);
    RUN_TEST(test_mensagem_que_nao_cabe_retorna_zero_e_nao_gasta_seq);
    RUN_TEST(test_retorna_o_tamanho_do_texto_gerado);
    return UNITY_END();
}
