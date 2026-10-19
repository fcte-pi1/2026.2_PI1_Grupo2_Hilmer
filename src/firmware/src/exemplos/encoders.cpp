// Exemplo mínimo do driver dos encoders (#234), para testar na placa.
//
// Gravar:  pio run -e exemplo_encoders -t upload && pio device monitor
//
// Com a ponte H desligada, girar cada roda à mão e acompanhar a contagem.
// Conferir:
//   - uma volta da roda gera pulsos do encoder x redução x 4 contagens
//     (doc 4.3, seção 6.1);
//   - girar no sentido que leva o robô para a frente aumenta a contagem, e o
//     sentido inverso diminui (se não, trocar o campo invertido em main.cpp e aqui);
//   - com o Wi-Fi ligado no firmware completo, a roda parada não conta sozinha.
// Enviar "z" pelo monitor serial zera as contagens.

#include <Arduino.h>

#include "config/pinos.h"
#include "encoders/encoders.h"

static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, true};

void setup() {
    // Mantém a ponte H desligada para as rodas girarem livres à mão.
    pinMode(Pinos::MOTOR_STBY, OUTPUT);
    digitalWrite(Pinos::MOTOR_STBY, LOW);

    Serial.begin(115200);
    delay(2000);

    if (!encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO)) {
        Serial.println("Erro: falha ao configurar o PCNT dos encoders");
    }
    Serial.println("Gire as rodas à mão. Envie \"z\" para zerar.");
}

void loop() {
    if (Serial.available() > 0 && Serial.read() == 'z') {
        encoders_zerar();
        Serial.println("Contagens zeradas");
    }

    Serial.printf("esquerda: %8ld   direita: %8ld\n", (long)encoders_contagem_esquerda(),
                  (long)encoders_contagem_direita());
    delay(200);
}
