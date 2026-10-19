// Cliente Wi-Fi e WebSocket que envia a telemetria ao backend (gateway /ingest).
// O canal é só de ida (arquitetura.md, seção 2.4): mensagens do servidor são ignoradas.
#pragma once

#include <cstddef>

// Começa a conectar no Wi-Fi e no WebSocket, sem esperar a conexão terminar.
void comunicacao_iniciar();

// Mantém a conexão e reconecta depois de uma queda. Chamar sempre, na tarefa de comunicação.
void comunicacao_atualizar();

bool comunicacao_conectada();

// Envia um texto (o JSON da telemetria). Retorna false se não houver conexão,
// sem esperar por ela: a mensagem é descartada e a tarefa segue.
bool comunicacao_enviar(const char *texto, size_t tamanho);
