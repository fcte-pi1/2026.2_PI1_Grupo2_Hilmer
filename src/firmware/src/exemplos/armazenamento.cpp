// Exemplo mínimo do módulo do cartão microSD (#238), para testar na placa.
//
// Gravar:  pio run -e exemplo_armazenamento -t upload && pio device monitor
//
// Grava 1000 linhas parecidas com a telemetria em /teste_sd.txt, mede o tempo
// de escrita e lê o arquivo de volta para conferir.
// Conferir:
//   - o monitor mostra "1000 linhas conferidas, nenhuma diferente";
//   - no PC, o arquivo teste_sd.txt tem 1000 linhas, de "seq":0 a "seq":999;
//   - os tempos de escrita (total, pior linha e sincronização);
//   - sem cartão, o exemplo avisa e continua rodando (o LED pisca), sem travar.
// Enviar "r" pelo monitor serial repete o teste.

#include <Arduino.h>
#include <SD.h>

#include "armazenamento/armazenamento.h"
#include "config/pinos.h"

constexpr const char *ARQUIVO_TESTE = "/teste_sd.txt";
constexpr int LINHAS = 1000;

static const PinosSD PINOS_SD = {Pinos::SD_SCK, Pinos::SD_MISO, Pinos::SD_MOSI, Pinos::SD_CS};

static void montar_linha(char *texto, size_t tamanho, int seq) {
    snprintf(texto, tamanho,
             "{\"v\":1,\"type\":\"telemetry\",\"robot_id\":\"mm-01\",\"seq\":%d,\"t_ms\":%d,"
             "\"pos\":{\"x\":3,\"y\":1},\"heading\":\"N\",\"speed_mps\":0.42,\"battery_pct\":87.5}",
             seq, seq * 100);
}

static void testar() {
    uint32_t inicio = millis();
    bool ok = armazenamento_iniciar(PINOS_SD, ARQUIVO_TESTE);
    Serial.printf("Montagem: %s em %lu ms\n", ok ? "ok" : "falhou",
                  (unsigned long)(millis() - inicio));
    if (!ok) {
        Serial.println("Cartão não encontrado. Confira o cartão (FAT32) e a ligação do HW-125.");
        // Confere que as outras funções também retornam erro sem travar.
        Serial.printf("gravar_linha sem cartão: %s\n",
                      armazenamento_gravar_linha("teste") ? "true (errado)" : "false (certo)");
        return;
    }

    // Começa de um arquivo vazio.
    armazenamento_fechar();
    SD.remove(ARQUIVO_TESTE);
    armazenamento_iniciar(PINOS_SD, ARQUIVO_TESTE);

    char linha[200];
    uint32_t pior_linha_us = 0;
    uint32_t inicio_escrita_us = micros();
    for (int seq = 0; seq < LINHAS; seq++) {
        montar_linha(linha, sizeof(linha), seq);
        uint32_t t0 = micros();
        if (!armazenamento_gravar_linha(linha)) {
            Serial.printf("Falha ao gravar a linha %d\n", seq);
            return;
        }
        pior_linha_us = max(pior_linha_us, micros() - t0);
    }
    uint32_t t0 = micros();
    bool sincronizou = armazenamento_sincronizar();
    uint32_t sincronizacao_us = micros() - t0;
    uint32_t total_us = micros() - inicio_escrita_us;
    armazenamento_fechar();

    Serial.printf("%d linhas de %u bytes em %.1f ms (%.0f linhas/s)\n", LINHAS,
                  (unsigned)strlen(linha) + 1, total_us / 1000.0f, LINHAS * 1e6f / total_us);
    Serial.printf("Pior linha: %lu us   sincronização final: %lu us (%s)\n",
                  (unsigned long)pior_linha_us, (unsigned long)sincronizacao_us,
                  sincronizou ? "ok" : "falhou");

    // Lê de volta e compara cada linha com a esperada.
    File arquivo = SD.open(ARQUIVO_TESTE, FILE_READ);
    if (!arquivo) {
        Serial.println("Não foi possível abrir o arquivo para leitura");
        return;
    }
    int lidas = 0;
    int diferentes = 0;
    char esperada[200];
    while (arquivo.available()) {
        String lida = arquivo.readStringUntil('\n');
        montar_linha(esperada, sizeof(esperada), lidas);
        if (lida != esperada) {
            diferentes++;
        }
        lidas++;
    }
    Serial.printf("Tamanho do arquivo: %lu bytes\n", (unsigned long)arquivo.size());
    arquivo.close();
    Serial.printf("%d linhas conferidas, %s\n", lidas,
                  diferentes == 0 && lidas == LINHAS ? "nenhuma diferente" : "COM DIFERENÇAS");
    if (diferentes > 0) {
        Serial.printf("%d linhas diferentes do esperado\n", diferentes);
    }
}

void setup() {
    // Mantém a ponte H desligada.
    pinMode(Pinos::MOTOR_STBY, OUTPUT);
    digitalWrite(Pinos::MOTOR_STBY, LOW);
    pinMode(Pinos::LED_STATUS, OUTPUT);

    Serial.begin(115200);
    delay(2000);

    testar();
    Serial.println("Envie \"r\" para repetir o teste.");
}

void loop() {
    // O LED piscando mostra que o firmware não travou.
    digitalWrite(Pinos::LED_STATUS, (millis() / 500) % 2);

    if (Serial.available() > 0 && Serial.read() == 'r') {
        testar();
    }
}
