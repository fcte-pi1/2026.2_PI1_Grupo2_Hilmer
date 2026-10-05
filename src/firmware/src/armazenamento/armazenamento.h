// Cartão microSD (HW-125, FAT32), para guardar o mapa e a telemetria no robô.
#pragma once

// Retorna false se o cartão não for encontrado.
bool armazenamento_iniciar();

// Acrescenta uma linha ao arquivo de telemetria.
bool armazenamento_gravar_linha(const char *linha);
