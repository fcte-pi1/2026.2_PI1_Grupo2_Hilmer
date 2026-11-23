// Conversão do mapa espelhado do explorar para as coordenadas reais do labirinto.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
//
// Com o labirinto à esquerda do start, o explorar usa um mapa espelhado: o x cresce
// para a esquerda do robô e "leste" do mapa é o oeste real. Estas funções convertem do
// mapa para o labirinto real (usadas na simulação, que confere o robô no real). A web
// usa as coordenadas do mapa, sem conversão: start em (0,0) e objetivo no canto oposto.
#pragma once

#include "labirinto.h"

int espelho_x(int x, int largura, bool espelhado);
Direcao espelho_direcao(Direcao direcao, bool espelhado);
// "paredes" em bits de Direcao.
uint8_t espelho_paredes(uint8_t paredes, bool espelhado);

// Índice da direção: N=0, L=1, S=2, O=3 (virar à direita soma 1).
int indice_direcao(Direcao direcao);
Direcao direcao_do_indice(int indice);

// Direção, no mapa do explorar, de um lado do robô virado para "atual".
// "passos_direita": 0 frente, 1 direita, 2 trás, 3 esquerda. Com o labirinto à esquerda
// do start (labirinto_a_direita = false) o mapa é espelhado: o leste do mapa é a
// esquerda do robô.
Direcao lado_no_mapa(Direcao atual, int passos_direita, bool labirinto_a_direita);

// Quantas vezes 90° para a direita (do robô) ele vira para sair de "atual" e ficar
// virado para "proxima" no mapa: 0 segue, 1 direita, 2 volta, 3 esquerda.
int passos_ate(Direcao atual, Direcao proxima, bool labirinto_a_direita);
