// Pacote de telemetria enviado ao sistema web por WebSocket.
// O formato é o contrato com a equipe de Software (src/docs/arquitetura.md, seção 2).
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
#pragma once

#include <cstddef>
#include <cstdint>

#include "labirinto.h"

// Folga para o maior pacote (telemetria com paredes), que tem cerca de 260 caracteres.
constexpr size_t TELEMETRIA_TAMANHO_MAX = 320;

// Campo "state" do contrato.
enum EstadoRobo {
    ROBO_PARADO,      // IDLE
    ROBO_EXPLORANDO,  // EXPLORING
    ROBO_RETORNANDO,  // RETURNING
    ROBO_CORRIDA,     // SPEED_RUN
    ROBO_OBJETIVO,    // GOAL_REACHED
    ROBO_ERRO,        // ERROR
};

struct DadosTelemetria {
    int x;                  // célula atual
    int y;
    Direcao direcao;        // para onde o robô está virado
    float velocidade_mm_s;
    int32_t odometria_mm;   // distância desde o RUN_START
    float bateria_pct;
    uint8_t paredes;        // paredes da célula atual, bits de Direcao
    EstadoRobo estado;
};

// Monta o texto JSON de cada mensagem e numera as mensagens com o "seq".
// Cada função escreve em "saida" e retorna o tamanho do texto. Se o texto
// não couber, retorna 0 e o "seq" não avança, para não deixar lacuna.
class Telemetria {
public:
    explicit Telemetria(const char* robot_id, uint32_t seq_inicial = 0);

    // Próximo "seq" que será usado.
    uint32_t seq() const { return seq_; }

    int pacote(uint32_t t_ms, const DadosTelemetria& dados, char* saida, size_t tamanho);

    // Eventos de estado.
    int calibracao(uint32_t t_ms, bool ok, char* saida, size_t tamanho);
    int inicio(uint32_t t_ms, float bateria_pct, char* saida, size_t tamanho);
    int objetivo(uint32_t t_ms, int x, int y, char* saida, size_t tamanho);
    int fim(uint32_t t_ms, char* saida, size_t tamanho);

private:
    // Escreve o começo comum a todas as mensagens e o resto que vier em "campos".
    int montar(const char* tipo, uint32_t t_ms, const char* campos, char* saida, size_t tamanho);

    const char* robot_id_;
    uint32_t seq_;
};
