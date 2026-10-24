/**
 * @file pinos.h
 * @brief Mapa de pinos do ESP32-DevKit-V1 (WROOM-32) do micromouse.
 *
 * Segue o diagrama elétrico MICROMOUSE_HARDWARE.kicad_sch (rev. A).
 * Qualquer mudança aqui deve ser feita também no esquemático.
 *
 * Pinos que não devem ser usados:
 *   - GPIO6 a GPIO11: ligados à flash interna do módulo.
 *   - GPIO12: strapping da tensão da flash; em nível alto no boot impede a inicialização.
 *   - GPIO0, GPIO1 (TX0) e GPIO3 (RX0): gravação e depuração via USB.
 *
 * Usar o módulo WROOM: no WROVER, GPIO16 e GPIO17 são ocupados pela PSRAM.
 */

#pragma once

#include <cstdint>

namespace Pinos
{

    // Barramento I2C (VL53L0X x3, MPU6050 e INA219)

    constexpr uint8_t I2C_SDA = 21;
    constexpr uint8_t I2C_SCL = 22;

    // Cartão microSD (HW-125) via VSPI

    constexpr uint8_t SD_MOSI = 23;
    constexpr uint8_t SD_MISO = 19;
    constexpr uint8_t SD_SCK = 18;
    constexpr uint8_t SD_CS = 5;

    // XSHUT dos sensores de distância VL53L0X
    // O sensor da direita (U10) não tem XSHUT: fica sempre ligado pelo pull-up
    // do módulo e deve ter o endereço trocado primeiro na inicialização.

    constexpr uint8_t XSHUT_FRENTE = 17;   // U7  (XSHUT1)
    constexpr uint8_t XSHUT_ESQUERDA = 33; // U9  (XSHUT3)

    // Ponte H L298N (tirar os jumpers de ENA e ENB do módulo para usar o PWM).
    // Pull-down de 10k em ENA e ENB no hardware: ponte desligada no boot.

    // Motor esquerdo (canal A, conector J2)
    constexpr uint8_t MOTOR_ESQ_PWM = 25; // ENA
    constexpr uint8_t MOTOR_ESQ_IN1 = 26; // IN1
    constexpr uint8_t MOTOR_ESQ_IN2 = 27; // IN2

    // Motor direito (canal B, conector J3)
    constexpr uint8_t MOTOR_DIR_PWM = 13; // ENB
    constexpr uint8_t MOTOR_DIR_IN1 = 14; // IN3
    constexpr uint8_t MOTOR_DIR_IN2 = 4;  // IN4

    // Encoders (pinos somente de entrada, sem pull-up interno)
    // Ler com o periférico PCNT e manter o filtro de glitch habilitado,
    // principalmente em GPIO36 e GPIO39 com o Wi-Fi ativo.

    constexpr uint8_t ENC_ESQ_A = 34; // ENC_E_A (J2 C1)
    constexpr uint8_t ENC_ESQ_B = 35; // ENC_E_B (J2 C2)
    constexpr uint8_t ENC_DIR_A = 36; // ENC_D_A (J3 C1), VP
    constexpr uint8_t ENC_DIR_B = 39; // ENC_D_B (J3 C2), VN

    // Interface com o usuário

    constexpr uint8_t LED_STATUS = 2;   // D1 via R2 (330 ohm); também é o LED da placa
    constexpr uint8_t BOTAO_START = 15; // SW2 para o GND; usar INPUT_PULLUP (ativo em nível baixo)

} // namespace Pinos