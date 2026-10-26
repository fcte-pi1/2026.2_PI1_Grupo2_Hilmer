# Micromouse: Software (Aplicação Web de Telemetria)

Aplicação web que recebe a telemetria do robô micromouse em tempo real, exibe a corrida em um dashboard e guarda os resultados no PostgreSQL para consultas posteriores.

Enquanto o robô físico não está pronto, um **emulador em TypeScript** (`backend/scripts/robot-emulator.ts`) simula o robô e envia os mesmos dados. Para o backend, os dois são indistinguíveis.

> 📐 A arquitetura completa (visões 4+1, requisitos e decisões) está em [`docs/arquitetura.md`](http://docs/arquitetura.md). Este README é o guia prático para quem vai mexer no código.

---

## Visão geral em 30 segundos

 Robô (firmware)  ─┐

                   ├─ WebSocket /ingest ─▶  backend (NestJS) ──▶ PostgreSQL

 emulador (TS)   ──┘                          │

                                              └─ WebSocket /live \+ REST ─▶ frontend (Next.js) ─▶ navegador

- **Fluxo em tempo real (event-driven):** cada pacote recebido vira um evento interno (`telemetry.received`). Esse evento é consumido de forma independente por três partes: o broadcast para o navegador, a gravação no banco e as regras de alerta.  
- **Consultas (MVC):** o histórico, o melhor tempo e os labirintos seguem o padrão Controller → Service → Prisma, e o Next.js faz o papel de View.  
- **Comunicação só de ida:** a web **nunca** envia comandos, código ou mapa ao robô (RNF13).

| Parte | Tecnologia | Porta |
| :---- | :---- | :---- |
| Frontend | Next.js (React \+ TypeScript) | `3000` |
| Backend | NestJS (TypeScript) \+ Prisma | `3002` (REST e WebSocket `/live`) e `3001` (WebSocket `/ingest`) |
| Banco | PostgreSQL 16 | `5432` |
| Emulador do robô | TypeScript (ts-node) | cliente de `ws://localhost:3001/ingest` |

---

## Estrutura do repositório

src/

├── backend/                   \# NestJS: ingestão, eventos, REST e persistência

│   ├── infra/                 \# Docker Compose (PostgreSQL) e scripts de automação

│   │   ├── compose.yaml       \# Container Postgres com volume nomeado

│   │   └── scripts/           \# wait-for-postgres.js e create-test-db.js

│   ├── prisma/

│   │   ├── schema.prisma      \# Modelo de dados e definições de tabelas

│   │   ├── migrations/        \# Histórico de migrações gerenciadas pelo Prisma

│   │   └── seed.ts            \# Cadastro dos três labirintos (4x4, 8x4, 12x4)

│   ├── src/

│   │   ├── telemetry/         \# WebSocket /ingest, validação, seq, taxa (Hz)

│   │   ├── app.module.ts      \# Módulo principal da aplicação

│   │   └── main.ts            \# Ponto de entrada do NestJS

│   └── tests/                 \# Suíte de testes isolados (unit, integration, e2e)

│       ├── integration/       \# Testes de integração (ex: disponibilidade do banco)

│       └── setup.ts           \# Inicialização e limpeza do banco de testes

│

├── frontend/                  \# Next.js: dashboard ao vivo e histórico

│   ├── src/app/               \# Páginas e rotas do App Router

│   └── src/components/        \# Componentes do dashboard (mapa, bateria, velocidade)

│

├── firmware/                  \# Firmware C++ / PlatformIO para o robô físico

│

├── docs/                      \# Documentação técnica e visões de arquitetura

│   └── arquitetura.md

│

└── README.md

### Onde mexer para…

| Quero… | Vá em |
| :---- | :---- |
| mudar um gráfico ou indicador do dashboard | `frontend/src/components/` |
| mudar regras do backend ou ingestão | `backend/src/telemetry/` |
| alterar infraestrutura ou container do banco | `backend/infra/compose.yaml` |
| mudar o modelo de dados do banco | `backend/prisma/schema.prisma` \+ nova migração |
| ajustar testes de integração do banco | `backend/tests/integration/` |
| simular ou parametrizar o robô virtual | `backend/scripts/robot-emulator.ts` |

---

## Como rodar

### Pré-requisitos

- Docker e Docker Compose  
- Node.js 20+ e npm

### Desenvolvimento local

#### 1\. Backend & Banco de Dados

Dentro da pasta `backend`, os scripts já orquestram o container do Docker, a prontidão do PostgreSQL, as migrações e o seed de dados:

cd backend

\# Instale as dependências

npm install

\# Sobe os containers, aguarda o Postgres, roda as migrações e o seed

npm run dev

\# Inicia o servidor em modo de desenvolvimento (hot-reload)

npm run start:dev

*Comandos auxiliares de banco disponíveis no `package.json`:*

- `npm run services:up` / `npm run services:stop`: gerencia o container do PostgreSQL.  
- `npm run db:migrate`: aplica as migrações no banco de desenvolvimento (`microMouse`).  
- `npm run db:seed`: executa o cadastro inicial dos labirintos.  
- `npm run db:studio:dev`: interface web do Prisma no banco de desenvolvimento.

#### 2\. Frontend

Em outro terminal:

cd frontend

npm install

npm run dev

Acesse:

- **Dashboard:** [http\://localhost:3000](http://localhost:3000)  
- **API REST:** [http\://localhost:3002](http://localhost:3002)  
- **Ingestão do robô:** `ws://localhost:3001/ingest`  
- **Telemetria ao vivo (navegador):** `ws://localhost:3002/live`

---

## Rodando o Emulador do Robô

O emulador (`backend/scripts/robot-emulator.ts`) simula o ciclo completo de corrida (calibração, partida, percurso e meta) e envia os pacotes para o backend como o robô real:

cd backend

\# Execução padrão (conecta em ws://localhost:3001/ingest, 10 Hz, labirinto 4x4)

npm run robot:emulator

\# Labirinto maior, com bateria inicial de 75%

npm run robot:emulator \-- \--maze 8x4 \--battery 75

### Parâmetros principais

| Parâmetro | Padrão | Para que serve |
| :---- | :---- | :---- |
| `--maze 4x4|8x4|12x4` | `4x4` | Labirinto a ser percorrido |
| `--hz` | `10` | Taxa de envio em Hz (1 a 50) |
| `--steps` | `100` | Quantidade de pacotes de telemetria por corrida |
| `--battery` | `100` | Bateria inicial, em % |
| `--battery-drain` | `0.05` | Consumo de bateria por pacote, em pontos percentuais |
| `--loop` | desligado | Repete as corridas com 3 s de pausa entre elas |
| `--url` | `ws://localhost:3001/ingest` | URL do WebSocket de ingestão |

A lista completa e exemplos de cenários estão em [`backend/scripts/README.md`](backend/scripts/README.md).

---

## Banco de dados e Testes

O projeto utiliza dois bancos de dados lógicos isolados no mesmo cluster PostgreSQL:

- **`microMouse`**: banco de desenvolvimento.  
- **`microMouse_test`**: banco isolado para execução de testes automatizados (criado e sincronizado dinamicamente via `create-test-db.js`).

### Executando Testes

cd backend

\# Suíte geral de testes (o pretest prepara o container e o banco de testes automaticamente)

npm test

\# Executar exclusivamente os testes de integração do banco de dados

npm run test:integration

\# Monitoramento contínuo dos testes de integração

npm run test:integration:watch

\# Inspecionar dados gerados pelos testes via Prisma Studio (porta 5556\)

npm run db:studio:test

---

## Contrato de telemetria

O contrato entre robô, emulador, backend e frontend é padronizado e validado pelo DTO no backend:

**Amostra de Telemetria (`type: "telemetry"`):**

{

  "v": 1,

  "type": "telemetry",

  "robot\_id": "mm-01",

  "seq": 1532,

  "t\_ms": 153200,

  "pos": { "x": 2, "y": 1 },

  "heading": "N",

  "speed\_mm\_s": 240.5,

  "odo\_mm": 18320,

  "battery\_pct": 76.2,

  "state": "EXPLORING"

}

**Ciclo de Eventos (`type: "event"`):**

1. `CALIBRATION_DONE`: calibração dos sensores e encoders finalizada.  
2. `RUN_START`: largada do robô.  
3. `GOAL_REACHED`: robô alcançou o centro/objetivo do labirinto (`pos: { x, y }`).  
4. `RUN_END`: encerramento da corrida.

---

## Problemas comuns

| Sintoma | Causa provável | Solução |
| :---- | :---- | :---- |
| Erro de conexão com banco de testes | Container parado ou porta ocupada | Execute `npm run services:up` e depois `npm test` |
| Emulador não conecta via WebSocket | Backend não está rodando na porta 3001 | Inicie o backend com `npm run start:dev` antes de rodar `npm run robot:emulator` |
| Schema desatualizado no teste | Migrações pendentes no banco de testes | O comando `npm run pretest` sincroniza o schema automaticamente via `prisma db push` |
