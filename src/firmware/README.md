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
6. Copie `src/config/credenciais.exemplo.h` para `src/config/credenciais.h` e preencha o nome e a senha do Wi-Fi e o IP do notebook com o backend. A rede deve ser comum, com senha, em 2,4 GHz (por exemplo, o ponto de acesso do celular); redes WPA2-Enterprise não são suportadas. Esse arquivo está no `.gitignore` e não vai para o repositório. Sem ele, `pio run` falha com uma mensagem pedindo o arquivo.

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
│   ├── motores/          # ponte H L298N (PWM por LEDC)
│   ├── encoders/         # encoders dos N20 (PCNT)
│   ├── sensores/         # I2C: VL53L0X, MPU6050 e INA219
│   ├── armazenamento/    # cartão microSD (SPI)
│   ├── comunicacao/      # Wi-Fi e WebSocket até o backend
│   ├── config/           # pinos e credenciais (credenciais.h fica fora do git)
│   ├── navegacao/        # lógica pura: mapa do labirinto, Flood Fill, PID, bateria, odometria, telemetria
│   └── tarefas/          # tarefas do FreeRTOS
└── test/                 # testes Unity da lógica pura
```

### Lógica pura e _drivers_

A pasta `navegacao/` não pode incluir `Arduino.h` nem nada do ESP-IDF. Só ela é compilada no ambiente `native`, e por isso dá para testar o mapa e o Flood Fill no PC, sem placa. As outras pastas são _drivers_ que falam com o _hardware_ e só são testadas na placa.

Regra prática: se o código precisa de um pino, de um sensor ou de um `delay`, ele fica num _driver_. Se é cálculo, fica em `navegacao/` (ou em outra pasta de lógica pura incluída no `build_src_filter` do ambiente `native`) e ganha um teste em `test/`.

### Exemplos dos _drivers_

Cada _driver_ tem um exemplo mínimo em `src/exemplos/`, com o próprio `setup()` e `loop()`, para testar na placa sem o resto do _firmware_. Cada exemplo tem um ambiente `exemplo_<nome>` no `platformio.ini`, e o comentário no topo do arquivo explica o que conferir:

```
pio run -e exemplo_<nome> -t upload
pio device monitor
```

## Navegação (Flood Fill)

O módulo `navegacao/floodfill.*` decide para onde o robô vai, usando o mapa de `navegacao/labirinto.*`. O robô sai do canto (0, 0), que é o sudoeste, e o objetivo é o canto oposto (`largura - 1`, `altura - 1`).

1. A partir do objetivo, cada célula recebe o número de passos que faltam até ele, considerando só as paredes já descobertas. Uma célula ainda não visitada conta como sem paredes internas.
2. A cada célula, o robô anda para a vizinha sem parede com o menor número. Se houver empate, segue reto, para fazer menos curvas.
3. `proximaDirecao` recalcula os números toda vez que é chamada. Assim, quando uma parede nova entra no mapa, o caminho já muda na próxima decisão. No 12x4 são só 48 células, então o cálculo é rápido.

```cpp
Labirinto mapa(4, 4);
FloodFill floodfill(mapa);
Direcao proxima;
// A cada célula: marcar no mapa as paredes lidas pelos sensores e então decidir.
if (floodfill.proximaDirecao(x, y, direcao_atual, proxima)) {
    // virar para "proxima" e andar uma célula
}
```

`proximaDirecao` retorna `false` quando o robô já está no objetivo ou quando não existe caminho até ele.

## Telemetria

O módulo `navegacao/telemetria.*` monta o texto JSON que o robô envia ao sistema web. O formato é o contrato combinado com a equipe de Software, descrito na seção 2 de [`src/docs/arquitetura.md`](../docs/arquitetura.md) e validado pelo backend em `src/backend/src/telemetry/dto/telemetry.dto.ts`.

| Função       | Mensagem                          | Quando enviar                       |
| ------------ | --------------------------------- | ----------------------------------- |
| `pacote`     | `telemetry`                       | a cada 100 ms (10 Hz) durante a corrida |
| `calibracao` | evento `CALIBRATION_DONE` (`ok`)  | ao terminar a calibração dos sensores |
| `inicio`     | evento `RUN_START` (`battery_pct`) | no início da corrida               |
| `objetivo`   | evento `GOAL_REACHED` (`pos`)     | ao chegar na célula objetivo        |
| `fim`        | evento `RUN_END`                  | no fim da corrida                   |

A classe `Telemetria` guarda o `seq`, que cresce a cada mensagem, inclusive nos eventos. Cada função escreve num _buffer_ de `TELEMETRIA_TAMANHO_MAX` caracteres e retorna o tamanho do texto. Se ele não couber, a função retorna 0 e o `seq` não avança. O envio pelo WebSocket fica em `comunicacao/`, descrito a seguir.

```cpp
Telemetria telemetria("mm-01");
char texto[TELEMETRIA_TAMANHO_MAX];
int tamanho = telemetria.inicio(0, 98.0f, texto, sizeof(texto));
// texto: {"v":1,"type":"event","robot_id":"mm-01","seq":0,"t_ms":0,"event":"RUN_START","battery_pct":98.0}
```

## Comunicação (Wi-Fi e WebSocket)

O módulo `comunicacao/` conecta o ESP32 ao gateway do backend (`ws://<ip-do-notebook>:3001/ingest`) com a biblioteca [arduinoWebSockets](https://github.com/Links2004/arduinoWebSockets) (`links2004/WebSockets`). O endereço e a senha da rede ficam em `src/config/credenciais.h` (veja a Instalação).

| Função                  | O que faz                                                         |
| ----------------------- | ----------------------------------------------------------------- |
| `comunicacao_iniciar`   | começa a conectar no Wi-Fi, sem esperar                           |
| `comunicacao_atualizar` | abre o WebSocket quando o Wi-Fi conecta e mantém a conexão viva   |
| `comunicacao_conectada` | `true` se o Wi-Fi e o WebSocket estão conectados                  |
| `comunicacao_enviar`    | envia o texto; sem conexão, retorna `false` na hora               |

- **Reconexão:** o Wi-Fi volta sozinho (`WiFi.setAutoReconnect`). O WebSocket tenta de novo a cada 2 s. Um _ping_ a cada 2 s detecta o servidor fora do ar mesmo quando o Wi-Fi continua de pé.
- **Sem bloqueio:** com o Wi-Fi caído, nenhuma tentativa de conexão é feita. Com o Wi-Fi de pé e o servidor fora, cada tentativa espera no máximo o tempo limite da conexão TCP. Como isso roda no núcleo 0, o controle no núcleo 1 não é afetado.
- **Canal só de ida:** mensagens do servidor são ignoradas (`arquitetura.md`, seção 2.4).
- **Pacotes na queda:** por enquanto, o pacote enviado sem conexão é descartado, e o backend vê a lacuna no `seq`. O _buffer_ para reenviar os pacotes depois da queda (`arquitetura.md`, seção 5.5) ainda não foi feito.

Hoje a tarefa `comunicacao` já envia um pacote de telemetria a 10 Hz, com a bateria lida pelo INA219 e os outros campos fixos. Para conferir na placa, rode `npm run ingest:dev` em `src/backend/` e veja os pacotes chegando no log.

## Tarefas do FreeRTOS

O ESP32 tem dois núcleos. O núcleo 1 fica com o que precisa de tempo certo, e o núcleo 0 com o Wi-Fi e o cartão SD, para que a comunicação não atrase o controle.

| Tarefa        | Núcleo | Prioridade | Período        | O que faz                                                       |
| ------------- | :----: | :--------: | -------------- | --------------------------------------------------------------- |
| `sensores`    |   1    |     3      | 20 ms (50 Hz)  | Lê os sensores I2C e publica a leitura mais recente             |
| `controle`    |   1    |     2      | 10 ms (100 Hz) | Atualiza o mapa, roda o Flood Fill e o PID e comanda os motores |
| `comunicacao` |   0    |     1      | 100 ms (10 Hz) | Envia a telemetria por WebSocket e grava no cartão SD           |

As tarefas trocam dados por uma fila do FreeRTOS de tamanho 1. A tarefa `sensores` sobrescreve a leitura (`xQueueOverwrite`), e as outras leem a mais recente sem removê-la (`xQueuePeek`).

Os períodos são valores iniciais e devem ser ajustados nos testes de _hardware_.
