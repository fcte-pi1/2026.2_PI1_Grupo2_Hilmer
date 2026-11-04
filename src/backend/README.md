# _Backend_

Esta pasta deverá armazenar arquivos referentes a:

- Código-fonte da API REST: rotas, controladores, modelos e lógica de negócio.
- Arquivos de configuração do servidor: `app.py`, `server.js`, `main.go` etc., dependendo da linguagem/framework utilizado ([Flask](https://flask.palletsprojects.com/), [FastAPI](https://fastapi.tiangolo.com/), [Express](https://expressjs.com/), [Django](https://www.djangoproject.com/) etc.).
- Arquivos de definição de dependências: `requirements.txt` ou `pyproject.toml` (Python), `package.json` (Node.js), `pom.xml` (Java/Maven) etc.
- Scripts de migração e esquemas de banco de dados: arquivos `.sql`, scripts de migração ([Alembic](https://alembic.sqlalchemy.org/), [Sequelize](https://sequelize.org/) etc.) e seeds de dados para desenvolvimento.
- Arquivos de configuração de ambiente: `.env.example` com as variáveis de ambiente necessárias (nunca o `.env` real).
- Arquivos de containerização: `Dockerfile` e `docker-compose.yml`, caso o serviço seja executado em contêiner.

Evite incluir:

- Credenciais e segredos: arquivos `.env`, chaves de API, senhas, tokens de acesso ou qualquer dado sensível **nunca** devem ser versionados.
- Artefatos de build: diretórios como `__pycache__/`, `dist/`, `build/`, `.eggs/` devem ser gerados localmente e ignorados via `.gitignore`.
- Dependências instaladas: pastas como `node_modules/` ou ambientes virtuais Python (`venv/`, `.env/`) não devem ser incluídos no repositório.
- Arquivos temporários/específicos do sistema operacional: arquivos gerados automaticamente pelo sistema ou pelo gerenciador de arquivos (ex.: `*~`, `.DS_Store`, `Thumbs.db`).

> [!WARNING]
> **Não acrescente arquivos referentes ao _frontend_ nesta pasta.** Eles deverão ser armazenados na pasta [frontend](https://github.com/fcte-pi1/template/tree/main/src/frontend) deste repositório.

## Como rodar o Backend (NestJS)

```bash
cd src/backend
cp .env.example .env
npm install
npm run dev
# ou para subir apenas a aplicação NestJS em modo watch:
npm run start:dev
```

## Ingestão de telemetria (`/ingest`)

O `TelemetryModule` (`src/backend/src/telemetry/`) é a porta de entrada da telemetria. Ele **recebe, valida e publica eventos internos**; não grava no banco nem fala com o navegador.

### Endpoint

| Item | Valor |
| --- | --- |
| URL | `ws://<host>:3001/ingest` (porta configurável por `TELEMETRY_WS_PORT`) |
| Protocolo | WebSocket puro (`ws`), mensagens de texto em JSON |
| Direção | **somente robô → backend** (RNF13): o servidor nunca envia dados ao robô |
| Contrato | `src/docs/arquitetura.md`, seção 2 (DTOs em `dto/telemetry.dto.ts`) |

Como o navegador usa Socket.IO, o `/ingest` é um servidor `ws` próprio (`TelemetryIngestService`), e não o adaptador WebSocket global do NestJS.

### Fluxo de uma mensagem

1. `TelemetryIngestService` recebe o texto e o entrega, em ordem, ao `TelemetryValidationService`.
2. O `TelemetryValidationService` converte em JSON, valida contra o DTO e descarta o que estiver fora do contrato (log + contador, sem encerrar a conexão).
3. O `SequenceTrackerService` ignora `seq` repetido e registra lacunas no log.
4. O evento interno correspondente é emitido (envelope `{ data, recebido_em }`).
5. O `ConnectionMonitorService` escuta `telemetry.received` para vigiar a conexão e medir a taxa.

### Eventos internos emitidos

| Evento | Quando | Payload |
| --- | --- | --- |
| `telemetry.received` | cada pacote de telemetria válido | `{ data, recebido_em }` |
| `run.calibrated` | `CALIBRATION_DONE` | `{ data, recebido_em }` |
| `run.started` | `RUN_START` | `{ data, recebido_em }` |
| `run.goal_reached` | `GOAL_REACHED` | `{ data, recebido_em }` |
| `run.finished` | `RUN_END` (e também `GOAL_REACHED`) | `{ data, recebido_em }` |
| `robot.disconnected` | mais de 1 s sem telemetria | `{ robotId, lastSeenAt, at }` |
| `robot.reconnected` | os dados voltam após uma queda | `{ robotId, downtimeMs, at }` |
| `telemetry.rate_low` | taxa abaixo de 10 Hz por mais de 2 s | `{ robotId, hz, lowForMs, at }` |

A taxa é medida em janelas de 1 s (`ConnectionMonitorService.getRate(robotId)`). Para evitar alarme falso por jitter, só é considerada baixa abaixo de 9,5 Hz (`RATE_TOLERANCE_HZ`). Depois de `RUN_END`, o silêncio não conta como queda.

### Como testar

```bash
cd src/backend
npm install

# testes automatizados (115 testes; npm run test:cov mostra a cobertura)
npm test

# servidor completo NestJS (Ingestão WebSocket + Prisma + Regras de Corridas)
npm run start:dev

# servidor de ingestão isolado (só o TelemetryModule, sem banco)
npm run ingest:dev

# emulador do robô físico (Node.js/TypeScript) disparando telemetria a 10 Hz
npm run robot:emulator

# emulador com opções customizadas (loop contínuo, frequência, passos)
npm run robot:emulator -- --hz 10 --steps 120 --loop
```

O log mostra os eventos `run.*`, o registro das corridas no banco (`[RunsService] Corrida iniciada/encerrada`), um `telemetry.received` a cada 10 pacotes e a taxa medida a cada segundo (`... Hz`, nível `debug`).

## Docker e healthcheck

O `Dockerfile` é multi-stage: o estágio `build` instala tudo, roda `prisma generate` e compila o Nest; o `runtime` leva só `dist/`, as dependências de produção (com o Prisma Client gerado) e as migrações, e roda como o usuário `node` (sem root). Ao iniciar, o container aplica as migrações pendentes (`prisma migrate deploy`) e sobe o `node dist/main`.

```bash
cd src/backend

# banco + backend + frontend, na ordem certa (backend espera o Postgres e frontend espera o backend ficar healthy)
npm run docker:up        # = docker compose -f infra/compose.yaml up -d --build
docker compose -f infra/compose.yaml ps   # STATUS mostra (healthy)/(unhealthy)

# só o banco, para rodar o backend fora do Docker com npm run start:dev
npm run services:up
```

> Não rode `npm run start:dev` com o container do backend de pé: os dois usam as portas 3002 e 3001.

### `GET /health`

Faz um `SELECT 1` no banco via Prisma, com timeout de 2 s, e devolve o status de cada dependência:

| Situação | HTTP | Corpo |
| --- | --- | --- |
| Banco respondendo | `200` | `{ "status": "ok", "timestamp": "...", "dependencies": { "database": { "status": "up", "latencyMs": 2 } } }` |
| Banco fora ou lento | `503` | `{ "status": "error", ..., "dependencies": { "database": { "status": "down", "latencyMs": 3 } } }` |

O motivo da falha vai só para o log (`[AppService] Healthcheck: banco indisponível: ...`), porque a mensagem do Prisma pode trazer o endereço do banco e o `/health` é público.

O `healthcheck` do compose (e o `HEALTHCHECK` da imagem) roda `infra/scripts/healthcheck.js` a cada 10 s. Com o Postgres parado, o container fica `unhealthy` em cerca de 30 s (3 falhas) e continua rodando; quando o banco volta, o Prisma reconecta sozinho e o container volta a `healthy` no próximo ciclo, sem reiniciar (RNF-18). O `restart: unless-stopped` cobre o caso de o processo cair.

### Render

No serviço do backend na Render (#219), configure **Settings → Health Check Path = `/health`**. Assim a Render só manda tráfego para uma instância nova depois que ela responder 200, e marca o serviço como fora do ar quando o banco cair.

## Desempenho do histórico (`GET /runs`, `GET /runs/best`)

Índices criados na migração `add_indices_historico`:

| Tabela | Índice | Para quê |
| --- | --- | --- |
| `telemetrias` | `(corrida_id, timestamp)` | primeira e última amostra de cada corrida, amostras em ordem |
| `corridas` | `(labirinto_id, iniciada_em DESC)` | histórico filtrado por labirinto, do mais recente ao mais antigo |
| `corridas` | `(status)` | melhor resultado e limpeza das corridas órfãs (`EM_ANDAMENTO`) |

O `GET /runs` não traz mais as telemetrias para a aplicação: busca só a primeira e a última amostra de cada corrida. A bateria inicial e final vem dessas amostras, e a **velocidade média passa a ser distância/tempo** (`Δodo_mm / Δt_ms`, em m/s) em vez da média de todos os `speed_mm_s`. Sem `odo_mm` (campo opcional), a velocidade fica 0. O `GET /runs/best` virou uma consulta só (`DISTINCT ON` por labirinto), sem o N+1.

Medição com o banco populado pelo emulador (3 corridas reais multiplicadas: 3.006 corridas, 448 mil telemetrias, ~1.000 corridas por labirinto):

| Rota | Antes | Depois |
| --- | --- | --- |
| `GET /runs?mazeId=4x4` | ~4,5 s | ~80 ms |
| `GET /runs` (todas) | ~12,5 s | ~195 ms |
| `GET /runs/best` | ~50 ms | ~15–30 ms |

No `EXPLAIN ANALYZE`, a busca da primeira/última amostra usa `Index Scan` / `Index Scan Backward` em `telemetrias_corrida_id_timestamp_idx` (35 ms; sem o índice, 113 s). Em `corridas`, com só ~3 mil linhas, o Postgres ainda prefere `Seq Scan` quando o filtro devolve 1/3 da tabela; os índices entram nas consultas seletivas (`status = 'EM_ANDAMENTO'`, histórico paginado com `LIMIT`) e conforme a tabela cresce.

Para reproduzir (use um banco à parte para não misturar com os dados de desenvolvimento):

```bash
cd src/backend
docker exec microMouse-db-dev psql -U localUser -d postgres -c 'CREATE DATABASE "microMouse_bench"'
DATABASE_URL="postgresql://localUser:localPassword@localhost:5432/microMouse_bench?schema=public" npx prisma migrate deploy

# suba o backend apontando para esse banco e rode algumas corridas com o emulador
# (npm run robot:emulator -- --maze 8x4 ...); depois multiplique e meça:
docker exec -i microMouse-db-dev psql -U localUser -d microMouse_bench -v copias=1000 < infra/scripts/bench-popular.sql
docker cp infra/scripts/. microMouse-db-dev:/tmp/bench/
docker exec microMouse-db-dev psql -U localUser -d microMouse_bench -v maze=4x4 -f /tmp/bench/bench-historico.sql
```
