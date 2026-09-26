# Micromouse: Software (Aplicação Web de Telemetria)

Aplicação web que recebe a telemetria do robô micromouse em tempo real, exibe a corrida em um dashboard e guarda os resultados no PostgreSQL para consultas posteriores.

Enquanto o robô físico não está pronto, um **mock em Python** simula o robô e envia os mesmos dados. Para o backend, os dois são indistinguíveis.

> 📐 A arquitetura completa (visões 4+1, requisitos e decisões) está em [`docs/arquitetura.md`](docs/arquitetura.md). Este README é o guia prático para quem vai mexer no código.

---

## Visão geral em 30 segundos

```
 Robô (firmware)  ─┐
                   ├─ WebSocket /ingest ─▶  backend (NestJS) ──▶ PostgreSQL
 mock (Python)   ──┘                          │
                                              └─ Socket.IO + REST ─▶ frontend (Next.js) ─▶ navegador
```

- **Fluxo em tempo real (event-driven):** cada pacote recebido vira um evento interno (`telemetry.received`). Esse evento é consumido de forma independente por três partes: o broadcast para o navegador, a gravação no banco e as regras de alerta.
- **Consultas (MVC):** o histórico, o melhor tempo e os labirintos seguem o padrão Controller → Service → Prisma, e o Next.js faz o papel de View.
- **Comunicação só de ida:** a web **nunca** envia comandos, código ou mapa ao robô (RNF13).

| Parte        | Tecnologia                   | Porta                                   |
| ------------ | ---------------------------- | --------------------------------------- |
| Frontend     | Next.js (React + TypeScript) | `3000`                                  |
| Backend      | NestJS (TypeScript) + Prisma | `3001`                                  |
| Banco        | PostgreSQL 16                | `5432`                                  |
| Mock do robô | Python 3.11+                 | cliente de `ws://localhost:3001/ingest` |

---

## Estrutura do repositório

```
micromouse/
├── frontend/                  # Next.js: dashboard ao vivo e histórico
│   ├── app/
│   │   ├── page.tsx           # Dashboard (tempo real)
│   │   └── historico/page.tsx # Histórico e melhor tempo por labirinto
│   ├── components/            # MazeMap, Speedometer, BatteryBar, RunTimer,
│   │                          # ChallengeFlag, ConnectionStatus, AlertPanel...
│   ├── hooks/useTelemetry.ts  # conexão Socket.IO + reconexão + snapshot
│   └── lib/api.ts             # chamadas REST
│
├── backend/                   # NestJS: ingestão, eventos, REST e persistência
│   ├── src/
│   │   ├── telemetry/         # WebSocket /ingest, validação, seq, watchdog, taxa (Hz)
│   │   ├── runs/              # ciclo de vida da corrida, métricas, REST /runs
│   │   ├── mazes/             # labirintos 4x4, 8x4, 12x4 e labirinto ativo
│   │   ├── alerts/            # regras de alerta (bateria, conexão, taxa, erro)
│   │   ├── realtime/          # Socket.IO para o navegador
│   │   └── persistence/       # gravação em lote no banco
│   └── prisma/
│       ├── schema.prisma      # modelo do banco
│       └── seed.ts            # cadastra os três labirintos
│
├── mocks/                     # Python: robô virtual
│   ├── robot_mock.py
│   ├── mazes/                 # labirintos em arquivo que o mock percorre
│   └── requirements.txt
│
├── shared/
│   └── telemetry.schema.json  # ⭐ contrato de telemetria (fonte da verdade)
│
├── docs/
│   ├── arquitetura.md         # documento de arquitetura (4+1)
│   └── diagramas/
│
├── docker-compose.yml         # sobe db + backend + frontend
└── README.md
```

### Onde mexer para…

| Quero…                                      | Vá em                                                                                                              |
| ------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ |
| mudar um gráfico ou indicador do dashboard  | `frontend/components/`                                                                                             |
| mudar como o front recebe dados ao vivo     | `frontend/hooks/useTelemetry.ts`                                                                                   |
| adicionar ou alterar um campo da telemetria | `shared/telemetry.schema.json` primeiro, depois backend, frontend e mock (ver [Contrato](#contrato-de-telemetria)) |
| criar uma regra de alerta                   | `backend/src/alerts/` (basta ouvir `telemetry.received`)                                                           |
| criar ou alterar uma rota de consulta       | `backend/src/runs/` (controller + service)                                                                         |
| mudar o banco                               | `backend/prisma/schema.prisma` + nova migração                                                                     |
| simular uma situação de teste               | parâmetros do `mocks/robot_mock.py`                                                                                |

---

## Como rodar

### Pré-requisitos

- Docker e Docker Compose
- Node.js 20+ e npm (para rodar fora do Docker)
- Python 3.11+ (para o mock)

### Opção A: tudo com Docker (recomendado)

```bash
git clone <url-do-repositorio> micromouse
cd micromouse
cp .env.example .env
docker compose up --build
```

Na primeira execução, aplique as migrações e cadastre os labirintos:

```bash
docker compose exec backend npx prisma migrate deploy
docker compose exec backend npx prisma db seed
```

Depois:

- Dashboard: http://localhost:3000
- API: http://localhost:3001

### Opção B: desenvolvimento local (hot reload)

```bash
# 1. Banco
docker compose up -d db

# 2. Backend
cd backend
npm install
npx prisma migrate dev
npx prisma db seed
npm run start:dev

# 3. Frontend (outro terminal)
cd frontend
npm install
npm run dev
```

### Rodando o mock

```bash
cd mocks
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt

python robot_mock.py --maze 4x4
```

Antes de iniciar o mock, selecione o mesmo labirinto no dashboard.

| Parâmetro                              | Padrão                       | Para que serve                                                      |
| -------------------------------------- | ---------------------------- | ------------------------------------------------------------------- |
| `--maze 4x4\|8x4\|12x4`                | `4x4`                        | labirinto percorrido                                                |
| `--url`                                | `ws://localhost:3001/ingest` | endereço do backend                                                 |
| `--rate`                               | `10`                         | pacotes por segundo (RNF15 exige ≥ 10)                              |
| `--battery-drain`                      | `0.05`                       | queda de bateria por pacote (%), para testar os alertas de 20% e 5% |
| `--drop-rate`                          | `0`                          | fração de pacotes perdidos (0 a 1)                                  |
| `--disconnect-at` / `--disconnect-for` | —                            | derruba a conexão no segundo X por Y segundos (RNF18)               |
| `--calibration-fail`                   | desligado                    | envia a calibração com falha (RF12)                                 |
| `--no-goal`                            | desligado                    | corrida sem chegada ao objetivo (desafio = N)                       |

Exemplo de teste de reconexão:

```bash
python robot_mock.py --maze 8x4 --disconnect-at 15 --disconnect-for 5
```

O dashboard deve mostrar "Reconectando…" e, depois, retomar a atualização sozinho, sem recarregar a página e sem abrir uma nova corrida.

### Variáveis de ambiente (`.env`)

| Variável                                              | Exemplo                                                        | Usada por |
| ----------------------------------------------------- | -------------------------------------------------------------- | --------- |
| `POSTGRES_USER` / `POSTGRES_PASSWORD` / `POSTGRES_DB` | `micromouse`                                                   | db        |
| `DATABASE_URL`                                        | `postgresql://micromouse:micromouse@localhost:5432/micromouse` | backend   |
| `BACKEND_PORT`                                        | `3001`                                                         | backend   |
| `NEXT_PUBLIC_API_URL`                                 | `http://localhost:3001`                                        | frontend  |

Nunca faça commit do `.env`, apenas do `.env.example`.

---

## Contrato de telemetria

O arquivo [`shared/telemetry.schema.json`](shared/telemetry.schema.json) define o formato que **robô, mock, backend e frontend** seguem.

**Telemetria**, enviada a cada 100 ms:

```json
{
  "v": 1,
  "type": "telemetry",
  "robot_id": "mm-01",
  "seq": 1532,
  "t_ms": 153200,
  "pos": { "x": 2, "y": 1 },
  "heading": "N",
  "speed_mm_s": 240.5,
  "odo_mm": 18320,
  "battery_pct": 76.2,
  "walls": { "n": true, "e": false, "s": false, "w": true },
  "state": "EXPLORING"
}
```

**Eventos**, enviados quando ocorrem: `CALIBRATION_DONE` → `RUN_START` → … → `GOAL_REACHED` → `RUN_END`.

Regras importantes:

- **`seq` sempre cresce** e vale para todas as mensagens. O backend descarta valores repetidos, o que permite ao robô reenviar pacotes com segurança após uma reconexão.
- **`t_ms` é o relógio do robô**, contado desde o início da corrida. O tempo de conclusão vem dele.
- **Para mudar o contrato:**
  1. altere o schema;
  2. se a mudança quebrar a compatibilidade, incremente `v`;
  3. atualize o backend (DTO), o frontend (tipos) e o mock **no mesmo PR**;
  4. avise a equipe embarcada.

Os detalhes de cada campo e das métricas derivadas estão nas seções 2.1 a 2.3 de [`docs/arquitetura.md`](docs/arquitetura.md).

---

## API de relance

**WebSocket de ingestão** (robô e mock): `ws://<host>:3001/ingest`

**Socket.IO** (navegador), eventos recebidos:

| Evento       | Conteúdo                                      |
| ------------ | --------------------------------------------- |
| `telemetry`  | pacote de telemetria + métricas calculadas    |
| `run`        | início, calibração, objetivo e fim da corrida |
| `alert`      | alertas (bateria, conexão, taxa, erro)        |
| `connection` | estado da conexão com o robô, taxa em Hz      |

**REST:**

| Rota                               | Descrição                                                     |
| ---------------------------------- | ------------------------------------------------------------- |
| `GET /runs`                        | histórico geral (paginado)                                    |
| `GET /runs?mazeId=<id>`            | histórico de um labirinto                                     |
| `GET /runs/best`                   | melhor tempo de cada labirinto                                |
| `GET /runs/:id`                    | detalhes de uma corrida                                       |
| `GET /runs/:id/samples`            | amostras (para reproduzir o trajeto)                          |
| `GET /runs/active/snapshot`        | estado atual da corrida (usado na reconexão)                  |
| `GET /mazes` / `PUT /mazes/active` | listar labirintos / selecionar o labirinto da próxima corrida |

---

## Banco de dados

| Tabela                | Guarda                                                              |
| --------------------- | ------------------------------------------------------------------- |
| `labirintos`          | os três labirintos (dimensões e célula objetivo)                    |
| `corridas`            | cada corrida: tempo, status, desafio S/N, bateria, velocidade média |
| `amostras_telemetria` | todos os pacotes (único por `corrida_id + seq`)                     |
| `celulas_mapeadas`    | paredes descobertas por célula                                      |
| `alertas`             | alertas emitidos durante a corrida                                  |

Comandos úteis (dentro de `backend/`):

```bash
npx prisma migrate dev --name <descricao>   # cria uma migração após editar o schema
npx prisma studio                           # abre uma interface para navegar nos dados
npx prisma migrate reset                    # ⚠️ apaga tudo e recria (só em dev)
```

---

## Testes e qualidade

```bash
# backend
cd backend && npm run lint && npm test

# frontend
cd frontend && npm run lint

# mock
cd mocks && ruff check . && pytest
```

O roteiro de testes usa o mock para reproduzir cada cenário:

| Cenário              | Comando do mock                         |
| -------------------- | --------------------------------------- |
| corrida normal       | `--maze 4x4`                            |
| bateria baixa        | `--battery-drain 0.5`                   |
| perda de pacotes     | `--drop-rate 0.1`                       |
| queda de conexão     | `--disconnect-at 10 --disconnect-for 5` |
| falha de calibração  | `--calibration-fail`                    |
| desafio não cumprido | `--no-goal`                             |

---

## Como contribuir

1. Pegue uma issue no **GitHub Projects**. Cada HU ou RNF é uma issue.
2. Crie uma branch a partir da `main`:
   - `feat/<descricao-curta>` para funcionalidades;
   - `fix/<descricao-curta>` para correções;
   - `docs/<descricao-curta>` para documentação.
3. Faça commits pequenos e com mensagens claras, por exemplo: `feat(alerts): alerta de taxa abaixo de 10 Hz`.
4. Abra um Pull Request para a `main`:
   - referencie a issue (`Closes #12`);
   - peça **pelo menos um revisor**;
   - confirme que o lint e os testes passam.
5. A `main` é protegida: nada entra sem PR aprovado.

---

## Problemas comuns

| Sintoma                                         | Causa provável                                                                                               |
| ----------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| O mock conecta, mas o dashboard não mostra nada | nenhum labirinto foi selecionado no dashboard antes do `RUN_START`                                           |
| O backend falha ao iniciar com erro do Prisma   | as migrações não foram aplicadas; rode `npx prisma migrate dev`                                              |
| O robô físico não conecta ao backend            | robô e notebook em redes diferentes, ou rede com isolamento de clientes; use um roteador ou hotspot dedicado |
| A taxa aparece abaixo de 10 Hz                  | Wi-Fi fraco ou `--rate` menor que 10 no mock                                                                 |

---

## Documentação

- [`docs/arquitetura.md`](docs/arquitetura.md): arquitetura 4+1, contrato, banco e rastreabilidade de requisitos
- `docs/diagramas/`: diagramas exportados
