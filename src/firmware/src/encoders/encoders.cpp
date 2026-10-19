#include "encoders.h"

#include <driver/pulse_cnt.h>

// O contador do PCNT tem 16 bits. Ao chegar num limite ele volta a zero, e com
// accum_count o driver soma o valor perdido, então a contagem lida tem 32 bits.
constexpr int LIMITE_ALTO = 32767;
constexpr int LIMITE_BAIXO = -32768;

// Pulsos mais curtos que isso são descartados. Os pulsos espúrios do rádio em
// IO36/IO39 duram poucos microssegundos, e o intervalo entre bordas do encoder
// na rotação máxima do motor fica na casa de dezenas de microssegundos.
// No ESP32 o máximo é 1023 ciclos do APB (cerca de 12,7 us).
constexpr uint32_t FILTRO_GLITCH_NS = 1000;

struct Encoder {
    pcnt_unit_handle_t unidade = nullptr;
    bool invertido = false;
};

static Encoder encoder_esquerdo;
static Encoder encoder_direito;

// ESP_ERR_INVALID_STATE indica que o ponto já existe, o que também serve.
static bool adicionar_limite(pcnt_unit_handle_t unidade, int limite) {
    esp_err_t erro = pcnt_unit_add_watch_point(unidade, limite);
    return erro == ESP_OK || erro == ESP_ERR_INVALID_STATE;
}

static bool iniciar_encoder(const PinosEncoder &pinos, Encoder &encoder) {
    encoder.invertido = pinos.invertido;

    pcnt_unit_config_t config_unidade = {};
    config_unidade.low_limit = LIMITE_BAIXO;
    config_unidade.high_limit = LIMITE_ALTO;
    config_unidade.flags.accum_count = 1;
    if (pcnt_new_unit(&config_unidade, &encoder.unidade) != ESP_OK) {
        return false;
    }

    pcnt_glitch_filter_config_t config_filtro = {};
    config_filtro.max_glitch_ns = FILTRO_GLITCH_NS;
    if (pcnt_unit_set_glitch_filter(encoder.unidade, &config_filtro) != ESP_OK) {
        return false;
    }

    // Quadratura com 4 contagens por pulso: cada canal conta as duas bordas de um
    // sinal, e o nível do outro sinal define se soma ou subtrai.
    pcnt_chan_config_t config_canal_a = {};
    config_canal_a.edge_gpio_num = pinos.a;
    config_canal_a.level_gpio_num = pinos.b;
    pcnt_channel_handle_t canal_a = nullptr;
    if (pcnt_new_channel(encoder.unidade, &config_canal_a, &canal_a) != ESP_OK) {
        return false;
    }

    pcnt_chan_config_t config_canal_b = {};
    config_canal_b.edge_gpio_num = pinos.b;
    config_canal_b.level_gpio_num = pinos.a;
    pcnt_channel_handle_t canal_b = nullptr;
    if (pcnt_new_channel(encoder.unidade, &config_canal_b, &canal_b) != ESP_OK) {
        return false;
    }

    pcnt_channel_set_edge_action(canal_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(canal_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    pcnt_channel_set_edge_action(canal_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(canal_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // O accum_count só funciona com os limites registrados como pontos de observação.
    if (!adicionar_limite(encoder.unidade, LIMITE_ALTO) ||
        !adicionar_limite(encoder.unidade, LIMITE_BAIXO)) {
        return false;
    }

    return pcnt_unit_enable(encoder.unidade) == ESP_OK &&
           pcnt_unit_clear_count(encoder.unidade) == ESP_OK &&
           pcnt_unit_start(encoder.unidade) == ESP_OK;
}

static int32_t contagem(const Encoder &encoder) {
    if (encoder.unidade == nullptr) {
        return 0;
    }
    int valor = 0;
    pcnt_unit_get_count(encoder.unidade, &valor);
    return encoder.invertido ? -valor : valor;
}

bool encoders_iniciar(const PinosEncoder &esquerdo, const PinosEncoder &direito) {
    bool ok_esquerdo = iniciar_encoder(esquerdo, encoder_esquerdo);
    bool ok_direito = iniciar_encoder(direito, encoder_direito);
    return ok_esquerdo && ok_direito;
}

int32_t encoders_contagem_esquerda() {
    return contagem(encoder_esquerdo);
}

int32_t encoders_contagem_direita() {
    return contagem(encoder_direito);
}

void encoders_zerar() {
    // Zera também o valor acumulado pelo driver.
    if (encoder_esquerdo.unidade != nullptr) {
        pcnt_unit_clear_count(encoder_esquerdo.unidade);
    }
    if (encoder_direito.unidade != nullptr) {
        pcnt_unit_clear_count(encoder_direito.unidade);
    }
}
