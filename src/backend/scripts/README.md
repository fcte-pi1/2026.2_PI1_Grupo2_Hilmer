# 🤖 Emulador do Micromouse — Documentação

## 1. Sobre o emulador

O emulador do Micromouse simula o comportamento do robô físico, conectando-se ao Gateway WebSocket e enviando eventos e dados de telemetria durante uma corrida em um labirinto.

Por meio de argumentos de linha de comando, é possível personalizar a execução sem modificar o código-fonte.

**Funcionalidades:**

- Comunicação com o Gateway WebSocket.
- Simulação de labirintos de dimensões `4x4`, `8x4` e `12x4`.
- Configuração da frequência de envio da telemetria.
- Definição da quantidade de passos por corrida.
- Personalização do identificador do robô.
- Configuração da bateria inicial e do consumo por passo.
- Execução contínua de corridas em modo loop.
- Configuração do endereço do Gateway.

## 2. Pré-requisitos

Antes de executar o emulador, verifique se:

1. O Node.js e o npm estão instalados.
2. As dependências do projeto foram instaladas.
3. O Gateway WebSocket está em execução e acessível.
4. O script está configurado no `package.json` com o comando `robot:emulator`.

Se necessário, instale as dependências na raiz do projeto:

```bash
npm install
```

O endereço padrão utilizado pelo emulador é:

```text
ws://localhost:3001/ingest
```

Esse endereço pode ser alterado por argumento de linha de comando ou pela variável de ambiente `TELEMETRY_WS_URL`.

## 3. Execução básica

Para executar o emulador com as configurações padrão:

```bash
npm run robot:emulator
```

Por padrão, a execução utiliza:

| Parâmetro | Valor padrão |
|---|---|
| Gateway WebSocket | `ws://localhost:3001/ingest` |
| Frequência | `10 Hz` |
| Identificador do robô | `PIH_02_MOUSE` |
| Labirinto | `4x4` |
| Passos por corrida | `100` |
| Bateria inicial | `100%` |
| Consumo por passo | `0.05` ponto percentual |
| Modo loop | Desativado |

A uma frequência de 10 Hz, o emulador tenta enviar aproximadamente 10 pacotes de telemetria por segundo, com intervalo de 100 ms entre eles.

**Importante:** a frequência é aproximada, pois depende do agendamento do Node.js e do processamento da máquina.

## 4. Como passar argumentos ao emulador

A estrutura geral do comando é:

```bash
npm run robot:emulator -- [argumentos]
```

O separador `--` informa ao npm que os argumentos seguintes devem ser repassados ao script.

É possível combinar vários parâmetros no mesmo comando:

```bash
npm run robot:emulator -- --maze 8x4 --battery 75 --steps 120
```

Nesse exemplo, o robô executará uma corrida no labirinto `8x4`, começando com 75% de bateria e enviando 120 passos de telemetria.

## 5. Parâmetros disponíveis

| Argumento | Descrição | Padrão | Restrições |
|---|---|---|---|
| `--url` | Endereço do Gateway WebSocket | `ws://localhost:3001/ingest` | URL WebSocket válida |
| `--hz` | Frequência de envio, em hertz | `10` | De 1 a 50 |
| `--robot-id` | Identificador do robô | `PIH_02_MOUSE` | Texto identificador |
| `--steps` | Quantidade de passos por corrida | `100` | Inteiro maior que zero |
| `--maze` | Dimensão do labirinto | `4x4` | `4x4`, `8x4` ou `12x4` |
| `--battery` | Percentual inicial da bateria | `100` | De 0 a 100 |
| `--battery-drain` | Consumo por passo, em pontos percentuais | `0.05` | Número maior ou igual a zero |
| `--loop` | Repete as corridas continuamente | Desativado | Flag, sem valor |

Os argumentos numéricos devem receber valores válidos. O script valida os limites definidos para frequência, passos e bateria inicial, além de impedir consumo negativo.

## 6. Exemplos didáticos de uso

### 6.1. Alterar a dimensão do labirinto

O argumento `--maze` seleciona a trajetória simulada.

**Labirinto 4x4:**

```bash
npm run robot:emulator -- --maze 4x4
```

**Labirinto 8x4:**

```bash
npm run robot:emulator -- --maze 8x4
```

**Labirinto 12x4:**

```bash
npm run robot:emulator -- --maze 12x4
```

O emulador seleciona os pontos de trajetória associados à dimensão informada. A trajetória é predefinida no código e não representa um algoritmo de exploração autônoma.

### 6.2. Configurar a bateria inicial

Use `--battery` para definir o percentual da bateria no início de cada corrida.

```bash
npm run robot:emulator -- --battery 75
```

Nesse exemplo, a bateria começa em 75%.

Outros exemplos:

```bash
# Bateria em 100%
npm run robot:emulator -- --battery 100

# Bateria em 50%
npm run robot:emulator -- --battery 50

# Bateria em 10%
npm run robot:emulator -- --battery 10

# Bateria em 0%
npm run robot:emulator -- --battery 0
```

O valor configurado é enviado nos eventos iniciais, como `CALIBRATION_DONE` e `RUN_START`. Depois, o percentual é atualizado a cada passo de telemetria.

### 6.3. Configurar o consumo da bateria

Use `--battery-drain` para definir quanto da bateria será consumido por passo.

```bash
npm run robot:emulator -- --battery-drain 0.2
```

Nesse exemplo, o consumo será de 0,2 ponto percentual por passo.

| Consumo por passo | Após 10 passos, partindo de 100% |
|---|---:|
| `0` | 100% |
| `0.05` | 99,5% |
| `0.2` | 98% |
| `0.5` | 95% |
| `1` | 90% |

A bateria nunca fica abaixo de zero, pois o cálculo limita o valor mínimo a 0%.

**Atenção:** o consumo é aplicado por pacote de telemetria, não por segundo. Se a frequência aumentar, haverá mais passos por segundo e, consequentemente, maior consumo por unidade de tempo.

### 6.4. Combinar bateria inicial e consumo

```bash
npm run robot:emulator -- --battery 50 --battery-drain 0.2 --steps 10
```

Resultado esperado:

- Bateria inicial: 50%.
- Consumo por passo: 0,2 ponto percentual.
- Quantidade de passos: 10.
- Bateria após os 10 passos: 48%.

O último pacote de telemetria contém o estado `GOAL_REACHED`, seguido dos eventos de chegada e encerramento da corrida.

### 6.5. Alterar a frequência de telemetria

Use `--hz` para definir quantos pacotes de telemetria são programados por segundo.

```bash
# 1 pacote por segundo
npm run robot:emulator -- --hz 1

# 10 pacotes por segundo
npm run robot:emulator -- --hz 10

# 20 pacotes por segundo
npm run robot:emulator -- --hz 20

# 50 pacotes por segundo
npm run robot:emulator -- --hz 50
```

O intervalo aproximado é calculado por:

\[
\text{intervalo (ms)} = \frac{1000}{\text{frequência (Hz)}}
\]

Por exemplo, em 20 Hz:

\[
\frac{1000}{20} = 50\text{ ms}
\]

### 6.6. Definir a quantidade de passos

Use `--steps` para determinar quantos passos de telemetria serão enviados em cada corrida.

```bash
# 10 passos
npm run robot:emulator -- --steps 10

# 120 passos
npm run robot:emulator -- --steps 120

# 500 passos
npm run robot:emulator -- --steps 500
```

Em condições ideais, uma corrida com 120 passos a 10 Hz dura aproximadamente 12 segundos.

### 6.7. Alterar o identificador do robô

Use `--robot-id` para identificar diferentes robôs simulados.

```bash
npm run robot:emulator -- --robot-id MOUSE_01
```

Outro exemplo:

```bash
npm run robot:emulator -- --robot-id MOUSE_TESTE_02 --maze 8x4
```

O identificador é incluído nos eventos e pacotes de telemetria enviados ao Gateway.

### 6.8. Alterar o endereço do Gateway

Use `--url` para apontar o emulador para outro endpoint WebSocket.

```bash
npm run robot:emulator -- --url ws://localhost:3001/ingest
```

Para utilizar outro host ou porta:

```bash
npm run robot:emulator -- --url ws://192.168.1.100:3001/ingest
```

O host e a porta devem corresponder ao Gateway em execução, e o endpoint precisa aceitar a conexão WebSocket.

Também é possível definir o endereço por variável de ambiente:

```bash
export TELEMETRY_WS_URL="ws://localhost:3001/ingest"
npm run robot:emulator
```

O argumento `--url`, quando informado, tem prioridade sobre a variável de ambiente.

### 6.9. Executar corridas continuamente

Use `--loop` para repetir as corridas. Entre uma execução e outra, o emulador aguarda três segundos.

```bash
npm run robot:emulator -- --loop
```

É possível combinar o loop com outros parâmetros:

```bash
npm run robot:emulator -- --maze 8x4 --battery 80 --battery-drain 0.1 --steps 50 --loop
```

Cada nova corrida começa com o percentual definido em `--battery`, pois a bateria é reinicializada no início de cada ciclo.

Para solicitar a interrupção pelo terminal, pressione `Ctrl+C`.

## 7. Cenários de teste recomendados

### Cenário A — Corrida padrão

```bash
npm run robot:emulator
```

Objetivo: verificar a conexão com o Gateway e o fluxo básico de eventos e telemetria.

### Cenário B — Labirinto maior

```bash
npm run robot:emulator -- --maze 12x4 --steps 120
```

Objetivo: verificar o processamento de coordenadas e dimensões diferentes de labirinto.

### Cenário C — Bateria baixa

```bash
npm run robot:emulator -- --battery 10 --battery-drain 1 --steps 10
```

Objetivo: observar a queda da bateria e validar o tratamento de percentuais baixos.

### Cenário D — Bateria esgotada durante a corrida

```bash
npm run robot:emulator -- --battery 2 --battery-drain 0.5 --steps 10
```

Objetivo: verificar se a telemetria mantém a bateria em zero após o esgotamento.

### Cenário E — Alta frequência

```bash
npm run robot:emulator -- --hz 50 --steps 100
```

Objetivo: testar o processamento de telemetria em frequência elevada, respeitando o limite de 50 Hz configurado no script.

### Cenário F — Corridas repetidas

```bash
npm run robot:emulator -- --maze 8x4 --battery 75 --steps 30 --loop
```

Objetivo: verificar o comportamento do Gateway ao receber múltiplas corridas consecutivas do mesmo robô.

## 8. Eventos e estrutura da telemetria

Durante a execução, o emulador envia eventos e pacotes JSON pelo WebSocket.

### Eventos simulados

| Evento | Finalidade |
|---|---|
| `CALIBRATION_DONE` | Indica que a calibração foi concluída |
| `RUN_START` | Indica o início da corrida |
| `GOAL_REACHED` | Indica a chegada ao objetivo |
| `RUN_END` | Indica o encerramento da corrida |

### Exemplo de evento de início

```json
{
  "v": 1,
  "type": "event",
  "robot_id": "PIH_02_MOUSE",
  "seq": 2,
  "t_ms": 50,
  "event": "RUN_START",
  "maze": "8x4",
  "battery_pct": 75,
  "pos": {
    "x": 0,
    "y": 0
  }
}
```

### Exemplo de pacote de telemetria

```json
{
  "v": 1,
  "type": "telemetry",
  "robot_id": "PIH_02_MOUSE",
  "seq": 3,
  "t_ms": 100,
  "pos": {
    "x": 0,
    "y": 1
  },
  "heading": "N",
  "speed_mm_s": 310,
  "odo_mm": 31,
  "battery_pct": 74.95,
  "state": "EXPLORING",
  "walls": {
    "w": true,
    "e": true,
    "s": false,
    "n": false
  }
}
```

Os valores de velocidade, distância e posição variam conforme a simulação. Os exemplos ilustram a estrutura dos dados e não garantem valores exatos em uma execução real.

O campo `seq` identifica sequencialmente as mensagens enviadas durante cada conexão, enquanto `t_ms` representa o tempo decorrido desde o início da corrida.

## 9. Encerramento e solução de problemas

**Erro de conexão WebSocket**

Verifique se o Gateway está ativo, se o endereço está correto e se a rota `/ingest` aceita conexões WebSocket.

**Argumento numérico inválido**

Confira os limites dos parâmetros. A bateria inicial deve estar entre 0 e 100, a frequência entre 1 e 50 Hz, o número de passos deve ser um inteiro positivo e o consumo não pode ser negativo.

**O emulador utiliza o labirinto 4x4**

Confira se o argumento foi passado após o separador do npm:

```bash
npm run robot:emulator -- --maze 8x4
```

**A bateria volta ao valor inicial em cada corrida**

Esse é o comportamento esperado do modo loop. Cada ciclo cria uma nova corrida e reinicializa a bateria conforme o argumento `--battery`.

**O Gateway não registra os eventos esperados**

Verifique os logs do servidor e confirme se os nomes dos eventos, os campos do payload e a sequência de mensagens são compatíveis com o contrato implementado no backend.

## 10. Resumo rápido

```bash
# Execução padrão
npm run robot:emulator

# Labirinto 8x4
npm run robot:emulator -- --maze 8x4

# Bateria inicial em 75%
npm run robot:emulator -- --battery 75

# Bateria com consumo de 0,2 por passo
npm run robot:emulator -- --battery 75 --battery-drain 0.2

# Corrida personalizada
npm run robot:emulator -- --maze 8x4 --battery 50 --battery-drain 0.1 --hz 20 --steps 120

# Corridas contínuas
npm run robot:emulator -- --maze 8x4 --battery 80 --steps 50 --loop
```

---

**Nota de implementação:** esta documentação descreve os argumentos e o comportamento do script apresentado. Antes de utilizar o README como referência definitiva do repositório, confirme se a versão do arquivo contém os parâmetros `--battery` e `--battery-drain` e se o contrato de eventos corresponde à implementação atual do Gateway.