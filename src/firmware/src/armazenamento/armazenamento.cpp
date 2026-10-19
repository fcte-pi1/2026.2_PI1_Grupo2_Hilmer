#include "armazenamento.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include <cstdio>
#include <unistd.h>

// 4 MHz é o padrão da biblioteca e funciona com o buffer de nível do HW-125.
// A telemetria gera poucos kB/s, bem abaixo do que o SPI nessa taxa permite.
constexpr uint32_t SPI_FREQUENCIA_HZ = 4000000;

// O cartão aparece no sistema de arquivos do ESP-IDF a partir deste caminho.
constexpr const char *PONTO_DE_MONTAGEM = "/sd";

// Intervalo máximo entre gravações no cartão. Se a energia cair, perde-se no
// máximo esse tempo de dados.
constexpr uint32_t PERIODO_SINCRONIZACAO_MS = 1000;

// As linhas se acumulam neste buffer e vão para o cartão em blocos.
constexpr size_t TAMANHO_BUFFER = 4096;

static SPIClass spi_sd(VSPI);
// Usa o stdio direto em vez do File do Arduino, porque o File não informa
// erro no flush(): com o cartão removido, as gravações pareceriam funcionar.
static FILE *arquivo = nullptr;
static uint32_t ultima_sincronizacao_ms = 0;

static void fechar_arquivo() {
    if (arquivo != nullptr) {
        fclose(arquivo);
        arquivo = nullptr;
    }
}

// Depois de um erro de escrita o cartão provavelmente saiu: fecha tudo para
// que as próximas chamadas retornem false na hora.
static void desmontar() {
    fechar_arquivo();
    SD.end();
}

bool armazenamento_iniciar(const PinosSD &pinos, const char *caminho) {
    fechar_arquivo();

    // O SD.begin() chama spi.begin() sem pinos, o que é ignorado se o
    // barramento já foi iniciado aqui com os pinos certos.
    spi_sd.begin(pinos.sck, pinos.miso, pinos.mosi, pinos.cs);
    if (!SD.begin(pinos.cs, spi_sd, SPI_FREQUENCIA_HZ, PONTO_DE_MONTAGEM)) {
        return false;
    }
    if (SD.cardType() == CARD_NONE) {
        desmontar();
        return false;
    }

    char caminho_completo[64];
    snprintf(caminho_completo, sizeof(caminho_completo), "%s%s", PONTO_DE_MONTAGEM, caminho);
    arquivo = fopen(caminho_completo, "a");
    if (arquivo == nullptr) {
        desmontar();
        return false;
    }
    setvbuf(arquivo, nullptr, _IOFBF, TAMANHO_BUFFER);
    ultima_sincronizacao_ms = millis();
    return true;
}

bool armazenamento_gravar_linha(const char *linha) {
    if (arquivo == nullptr) {
        return false;
    }

    if (fputs(linha, arquivo) < 0 || fputc('\n', arquivo) == EOF) {
        desmontar();
        return false;
    }

    if (millis() - ultima_sincronizacao_ms >= PERIODO_SINCRONIZACAO_MS) {
        return armazenamento_sincronizar();
    }
    return true;
}

bool armazenamento_sincronizar() {
    if (arquivo == nullptr) {
        return false;
    }
    ultima_sincronizacao_ms = millis();
    // fflush passa o buffer para o FAT, e fsync grava os setores e o tamanho
    // do arquivo no cartão.
    if (fflush(arquivo) != 0 || fsync(fileno(arquivo)) != 0) {
        desmontar();
        return false;
    }
    return true;
}

void armazenamento_fechar() {
    if (arquivo != nullptr) {
        armazenamento_sincronizar();
        fechar_arquivo();
    }
}
