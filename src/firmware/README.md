# _Firmware_

Código do ESP32-DevKit-V1 do Micromouse. Usa o [PlatformIO](https://platformio.org/) com o _framework_ Arduino, na plataforma [pioarduino](https://github.com/pioarduino/platform-espressif32) (Arduino core 3.x sobre ESP-IDF 5.x). Assim dá para usar a API do Arduino (`digitalWrite`, `Wire`, `Serial`) e, quando precisar, as APIs do ESP-IDF e do FreeRTOS.

## Instalação

1. **Windows:** habilite caminhos longos antes de tudo. Sem isso, a instalação da plataforma falha com `FileNotFoundError`, porque alguns arquivos do pioarduino passam do limite de 260 caracteres do Windows. Abra o PowerShell **como administrador**, rode o comando abaixo e reinicie o computador:

   ```powershell
   New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
   ```

2. Instale o [VS Code](https://code.visualstudio.com/).
3. Instale a extensão **PlatformIO IDE**.
4. No VS Code, abra a pasta `src/firmware/` (não a raiz do repositório). A primeira abertura baixa a plataforma e demora alguns minutos.
5. Para os testes no PC, é preciso ter um compilador C++ (`g++`) no PATH. No Windows, use o [MinGW-w64](https://www.mingw-w64.org/) ou o [MSYS2](https://www.msys2.org/).

## Comandos

Pela barra inferior do PlatformIO no VS Code, ou pelo terminal dentro de `src/firmware/`:

| Ação                   | Comando              |
| ---------------------- | -------------------- |
| Compilar               | `pio run`            |
| Gravar no ESP32 (USB)  | `pio run -t upload`  |
| Abrir o monitor serial | `pio device monitor` |
| Rodar os testes no PC  | `pio test -e native` |

Nos testes, o `-e native` é obrigatório. Sem ele, o PlatformIO usa o ambiente padrão (`esp32dev`), que ignora os testes e mostra `0 test cases`. No VS Code, rode pela barra lateral do PlatformIO: **Project Tasks → native → Advanced → Test**.

## Estrutura

```
src/firmware/
├── platformio.ini        # ambientes: esp32dev (robô) e native (testes no PC)
├── src/
│   ├── main.cpp          # inicializa os módulos e cria as tarefas
│   ├── motores/          # ponte H TB6612FNG (PWM por LEDC)
│   ├── encoders/         # encoders dos N20 (PCNT)
│   ├── sensores/         # I2C: VL53L0X, BMI088 e INA219
│   ├── armazenamento/    # cartão microSD (SPI)
│   ├── navegacao/        # lógica pura: mapa do labirinto, Flood Fill
│   └── tarefas/          # tarefas do FreeRTOS
└── test/                 # testes Unity da lógica pura
```

### Lógica pura e _drivers_

A pasta `navegacao/` não pode incluir `Arduino.h` nem nada do ESP-IDF. Só ela é compilada no ambiente `native`, e por isso dá para testar o mapa e o Flood Fill no PC, sem placa. As outras pastas são _drivers_ que falam com o _hardware_ e só são testadas na placa.

Regra prática: se o código precisa de um pino, de um sensor ou de um `delay`, ele fica num _driver_. Se é cálculo, fica em `navegacao/` (ou em outra pasta de lógica pura incluída no `build_src_filter` do ambiente `native`) e ganha um teste em `test/`.

## Tarefas do FreeRTOS

O ESP32 tem dois núcleos. O núcleo 1 fica com o que precisa de tempo certo, e o núcleo 0 com o Wi-Fi e o cartão SD, para que a comunicação não atrase o controle.

| Tarefa        | Núcleo | Prioridade | Período        | O que faz                                                       |
| ------------- | :----: | :--------: | -------------- | --------------------------------------------------------------- |
| `sensores`    |   1    |     3      | 20 ms (50 Hz)  | Lê os sensores I2C e publica a leitura mais recente             |
| `controle`    |   1    |     2      | 10 ms (100 Hz) | Atualiza o mapa, roda o Flood Fill e o PID e comanda os motores |
| `comunicacao` |   0    |     1      | 100 ms (10 Hz) | Envia a telemetria por WebSocket e grava no cartão SD           |

As tarefas trocam dados por uma fila do FreeRTOS de tamanho 1. A tarefa `sensores` sobrescreve a leitura (`xQueueOverwrite`), e as outras leem a mais recente sem removê-la (`xQueuePeek`).

Os períodos são valores iniciais e devem ser ajustados nos testes de _hardware_.
