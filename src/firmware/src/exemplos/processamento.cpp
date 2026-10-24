// Exemplo do processamento básico dos dados dos sensores (teste 3.13 do doc 7.3).
//
// Gravar:  pio run -e exemplo_processamento -t upload && pio device monitor
//
// Mostra cada dado bruto ao lado do dado processado, a cada 250 ms:
//   - distâncias: leitura de cada VL53L0X e "S" (parede) ou "N" (sem parede),
//     comparando com LIMIAR_PAREDE_MM (thresholding);
//   - bateria: tensão e corrente de uma leitura do INA219 e a média móvel das
//     últimas 10 leituras;
//   - encoders: contagem das rodas paradas com o rádio Wi-Fi ligado ou desligado,
//     para conferir o filtro de glitch do PCNT.
// Conferir:
//   - com uma parede mais perto que o limiar, o sensor mostra "S"; mais longe, "N";
//   - a média do INA219 varia menos que a leitura bruta;
//   - com as rodas paradas, a contagem não muda, mesmo com o Wi-Fi ligado.
// Enviar "w" pelo monitor serial liga e desliga o rádio Wi-Fi. "z" zera os encoders.
// A remoção do offset do giroscópio é conferida no exemplo_imu.
//
// Antes de usar, trocar LIMIAR_PAREDE_MM pelo valor definido no teste 3.4.

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>

#include "config/pinos.h"
#include "encoders/encoders.h"
#include "sensores/distancia.h"
#include "sensores/energia.h"

constexpr uint32_t I2C_FREQUENCIA_HZ = 400000;

// Abaixo dessa distância, há parede. Valor provisório: definir no teste 3.4.
constexpr uint16_t LIMIAR_PAREDE_MM = 100;

constexpr int TAMANHO_MEDIA = 10;
constexpr uint32_t PERIODO_LEITURA_MS = 50;
constexpr uint32_t PERIODO_IMPRESSAO_MS = 250;

static const PinosEncoder ENCODER_ESQUERDO = {Pinos::ENC_ESQ_A, Pinos::ENC_ESQ_B, false};
static const PinosEncoder ENCODER_DIREITO = {Pinos::ENC_DIR_A, Pinos::ENC_DIR_B, true};
static const PinosDistancia PINOS_DISTANCIA = {Pinos::XSHUT_FRENTE, Pinos::XSHUT_ESQUERDA};

static Distancias distancias = {DISTANCIA_INVALIDA, DISTANCIA_INVALIDA, DISTANCIA_INVALIDA};

// Média móvel: guarda as últimas TAMANHO_MEDIA leituras e troca a mais antiga pela nova.
static float tensoes[TAMANHO_MEDIA] = {};
static float correntes[TAMANHO_MEDIA] = {};
static int proxima = 0;
static int preenchidas = 0;
static float tensao_bruta = 0.0f;
static float corrente_bruta = 0.0f;

static bool wifi_ligado = false;

static void ler_bateria() {
    if (!energia_ler(tensao_bruta, corrente_bruta)) {
        return;
    }
    tensoes[proxima] = tensao_bruta;
    correntes[proxima] = corrente_bruta;
    proxima = (proxima + 1) % TAMANHO_MEDIA;
    if (preenchidas < TAMANHO_MEDIA) {
        preenchidas++;
    }
}

static float media(const float *valores) {
    if (preenchidas == 0) {
        return 0.0f;
    }
    float soma = 0.0f;
    for (int i = 0; i < preenchidas; i++) {
        soma += valores[i];
    }
    return soma / preenchidas;
}

static char parede(uint16_t distancia) {
    return distancia < LIMIAR_PAREDE_MM ? 'S' : 'N';
}

// Com o rádio ligado e procurando redes, ele transmite de tempos em tempos,
// que é quando os pulsos espúrios podem aparecer nos encoders.
static void alternar_wifi() {
    wifi_ligado = !wifi_ligado;
    if (wifi_ligado) {
        WiFi.mode(WIFI_STA);
        WiFi.scanNetworks(true);
        Serial.println("Wi-Fi ligado");
    } else {
        WiFi.scanDelete();
        WiFi.mode(WIFI_OFF);
        Serial.println("Wi-Fi desligado");
    }
}

void setup() {
    // Mantém a ponte H desligada: as rodas ficam paradas ou giram livres à mão.
    pinMode(Pinos::MOTOR_ESQ_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_ESQ_PWM, LOW);
    pinMode(Pinos::MOTOR_DIR_PWM, OUTPUT);
    digitalWrite(Pinos::MOTOR_DIR_PWM, LOW);

    Serial.begin(115200);
    delay(2000);

    Wire.begin(Pinos::I2C_SDA, Pinos::I2C_SCL, I2C_FREQUENCIA_HZ);
    if (!distancia_iniciar(Wire, PINOS_DISTANCIA)) {
        Serial.println("Aviso: algum sensor de distância não iniciou");
    }
    if (!energia_iniciar(Wire)) {
        Serial.println("Aviso: INA219 não respondeu");
    }
    if (!encoders_iniciar(ENCODER_ESQUERDO, ENCODER_DIREITO)) {
        Serial.println("Erro: falha ao configurar o PCNT dos encoders");
    }

    Serial.printf("Limiar de parede: %u mm. Envie \"w\" para ligar o Wi-Fi e \"z\" para zerar.\n",
                  LIMIAR_PAREDE_MM);
}

void loop() {
    static uint32_t ultima_impressao = 0;

    if (Serial.available() > 0) {
        char comando = Serial.read();
        if (comando == 'w') {
            alternar_wifi();
        } else if (comando == 'z') {
            encoders_zerar();
            Serial.println("Contagens zeradas");
        }
    }

    // Quando uma busca de redes termina, começa outra, para o rádio continuar ativo.
    if (wifi_ligado && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
    }

    distancia_ler(distancias);
    ler_bateria();

    if (millis() - ultima_impressao >= PERIODO_IMPRESSAO_MS) {
        ultima_impressao = millis();
        Serial.printf("dist f %5u %c  e %5u %c  d %5u %c | "
                      "bruto %.3f V %6.1f mA  média %.3f V %6.1f mA | "
                      "enc %ld %ld (Wi-Fi %s)\n",
                      distancias.frente, parede(distancias.frente), distancias.esquerda,
                      parede(distancias.esquerda), distancias.direita, parede(distancias.direita),
                      tensao_bruta,
                      1000.0f * corrente_bruta, media(tensoes), 1000.0f * media(correntes),
                      (long)encoders_contagem_esquerda(), (long)encoders_contagem_direita(),
                      wifi_ligado ? "ligado" : "desligado");
    }

    delay(PERIODO_LEITURA_MS);
}
