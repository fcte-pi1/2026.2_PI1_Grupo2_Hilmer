#include "armazenamento.h"

bool armazenamento_iniciar() {
    // TODO(#164): iniciar o SPI e montar o cartão com SD.begin().
    return false;
}

bool armazenamento_gravar_linha(const char *linha) {
    // TODO(#164): abrir o arquivo em modo append e gravar a linha.
    (void)linha;
    return false;
}
