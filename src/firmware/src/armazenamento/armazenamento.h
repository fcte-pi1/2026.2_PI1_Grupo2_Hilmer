// Cartão microSD (HW-125, FAT32), para guardar o mapa e a telemetria no robô.
#pragma once

#include <cstdint>

// Pinos do SPI do cartão. O barramento é exclusivo do cartão (doc 4.3, seção 5).
struct PinosSD {
    uint8_t sck;
    uint8_t miso;
    uint8_t mosi;
    uint8_t cs;
};

constexpr const char *ARMAZENAMENTO_ARQUIVO_PADRAO = "/telemetria.txt";

// Monta o cartão e abre o arquivo para acrescentar linhas (cria se não existir).
// Retorna false se o cartão não for encontrado; as outras funções então só
// retornam false, sem travar o firmware. Pode ser chamada de novo para tentar
// outra vez ou trocar de arquivo.
bool armazenamento_iniciar(const PinosSD &pinos, const char *caminho = ARMAZENAMENTO_ARQUIVO_PADRAO);

// Acrescenta uma linha ao arquivo (o '\n' é adicionado aqui).
// A gravação no cartão é feita em blocos: os dados vão para o cartão a cada
// segundo ou em armazenamento_sincronizar(). Se o cartão sair, retorna false.
bool armazenamento_gravar_linha(const char *linha);

// Grava no cartão o que ainda está em memória (por exemplo, no fim da corrida).
bool armazenamento_sincronizar();

// Sincroniza e fecha o arquivo. O cartão continua montado.
void armazenamento_fechar();
