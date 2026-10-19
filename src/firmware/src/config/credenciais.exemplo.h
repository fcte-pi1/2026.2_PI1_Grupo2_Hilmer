// Exemplo das credenciais da rede e do endereço do backend.
// Copie este arquivo para credenciais.h (na mesma pasta) e preencha os valores.
// O credenciais.h está no .gitignore: a senha do Wi-Fi não vai para o repositório.
#pragma once

#include <cstdint>

namespace Credenciais
{

    constexpr const char *WIFI_SSID = "nome-da-rede";
    constexpr const char *WIFI_SENHA = "senha-da-rede";

    // Notebook com o backend, na mesma rede do robô: ws://<ip>:3001/ingest
    constexpr const char *BACKEND_IP = "192.168.0.10";
    constexpr uint16_t BACKEND_PORTA = 3001;
    constexpr const char *BACKEND_CAMINHO = "/ingest";

} // namespace Credenciais
