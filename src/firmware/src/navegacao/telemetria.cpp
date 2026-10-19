#include "telemetria.h"

#include <cstdio>

static const char* texto_direcao(Direcao direcao) {
    switch (direcao) {
        case NORTE: return "N";
        case LESTE: return "E";
        case SUL: return "S";
        default: return "W";
    }
}

static const char* texto_estado(EstadoRobo estado) {
    switch (estado) {
        case ROBO_PARADO: return "IDLE";
        case ROBO_EXPLORANDO: return "EXPLORING";
        case ROBO_RETORNANDO: return "RETURNING";
        case ROBO_CORRIDA: return "SPEED_RUN";
        case ROBO_OBJETIVO: return "GOAL_REACHED";
        default: return "ERROR";
    }
}

static const char* texto_bool(bool valor) {
    return valor ? "true" : "false";
}

// snprintf retorna o tamanho que o texto teria; se for maior que o espaço, não coube.
static bool coube(int escritos, size_t tamanho) {
    return escritos >= 0 && (size_t)escritos < tamanho;
}

Telemetria::Telemetria(const char* robot_id, uint32_t seq_inicial)
    : robot_id_(robot_id), seq_(seq_inicial) {}

int Telemetria::montar(const char* tipo, uint32_t t_ms, const char* campos, char* saida, size_t tamanho) {
    int escritos = snprintf(saida, tamanho,
                            "{\"v\":1,\"type\":\"%s\",\"robot_id\":\"%s\",\"seq\":%lu,\"t_ms\":%lu,%s}",
                            tipo, robot_id_, (unsigned long)seq_, (unsigned long)t_ms, campos);
    if (!coube(escritos, tamanho)) {
        return 0;
    }
    seq_++;
    return escritos;
}

int Telemetria::pacote(uint32_t t_ms, const DadosTelemetria& dados, char* saida, size_t tamanho) {
    char campos[TELEMETRIA_TAMANHO_MAX];
    int escritos = snprintf(campos, sizeof(campos),
                            "\"pos\":{\"x\":%d,\"y\":%d},\"heading\":\"%s\",\"speed_mm_s\":%.1f,"
                            "\"odo_mm\":%ld,\"battery_pct\":%.1f,"
                            "\"walls\":{\"n\":%s,\"e\":%s,\"s\":%s,\"w\":%s},\"state\":\"%s\"",
                            dados.x, dados.y, texto_direcao(dados.direcao), dados.velocidade_mm_s,
                            (long)dados.odometria_mm, dados.bateria_pct,
                            texto_bool(dados.paredes & NORTE), texto_bool(dados.paredes & LESTE),
                            texto_bool(dados.paredes & SUL), texto_bool(dados.paredes & OESTE),
                            texto_estado(dados.estado));
    if (!coube(escritos, sizeof(campos))) {
        return 0;
    }
    return montar("telemetry", t_ms, campos, saida, tamanho);
}

int Telemetria::calibracao(uint32_t t_ms, bool ok, char* saida, size_t tamanho) {
    char campos[64];
    snprintf(campos, sizeof(campos), "\"event\":\"CALIBRATION_DONE\",\"ok\":%s", texto_bool(ok));
    return montar("event", t_ms, campos, saida, tamanho);
}

int Telemetria::inicio(uint32_t t_ms, float bateria_pct, char* saida, size_t tamanho) {
    char campos[64];
    snprintf(campos, sizeof(campos), "\"event\":\"RUN_START\",\"battery_pct\":%.1f", bateria_pct);
    return montar("event", t_ms, campos, saida, tamanho);
}

int Telemetria::objetivo(uint32_t t_ms, int x, int y, char* saida, size_t tamanho) {
    char campos[64];
    snprintf(campos, sizeof(campos), "\"event\":\"GOAL_REACHED\",\"pos\":{\"x\":%d,\"y\":%d}", x, y);
    return montar("event", t_ms, campos, saida, tamanho);
}

int Telemetria::fim(uint32_t t_ms, char* saida, size_t tamanho) {
    return montar("event", t_ms, "\"event\":\"RUN_END\"", saida, tamanho);
}
