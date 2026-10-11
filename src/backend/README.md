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

# testes automatizados
npm test

# servidor de ingestão (só o TelemetryModule, sem banco) com o log temporário dos eventos
npm run ingest:dev

# em outro terminal, a partir de src/backend: corrida completa com o mock
pip install -r ../mocks/requirements.txt
python ../mocks/robot_mock.py --maze 4x4
```

O log mostra os eventos `run.*`, um `telemetry.received` a cada 10 pacotes e a taxa medida a cada segundo (`... Hz`, nível `debug`). Para testar a queda e a retomada, suspenda o mock no meio da corrida (Ctrl+Z) e retome (`fg`); no Windows, use uma pausa dentro do mock.

O `ingest:dev` liga o listener temporário de log (`telemetry-events.logger.ts`), que deve ser removido quando os módulos Realtime, Persistence e Alerts estiverem consumindo os eventos. Reinicie o servidor entre corridas: o `SequenceTrackerService` não reinicia o `seq` e descartaria o início da corrida seguinte.
