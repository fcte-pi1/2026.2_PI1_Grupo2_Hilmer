# MicroMouse: Grupo 2 (PI1 2026.2)

Robô micromouse autônomo desenvolvido na disciplina Projeto Integrador de Engenharia 1 (FCTE/UnB). O robô percorre sozinho os três labirintos propostos (4×4, 8×4 e 12×4 células): identifica as paredes, acompanha a própria posição, mapeia o percurso e reconhece a chegada. Durante a corrida, ele envia telemetria para uma aplicação web, que mostra os dados em tempo real e guarda os resultados no banco para consulta depois.

Orientador: **Hilmer Rodrigues Neri**. Período: 02/09/2026 a 04/12/2026.

![Calendário 2026/2](https://github.com/fcte-pi1/template/blob/main/docs/figs/Calendario.png?raw=true)

## Visão geral

```
 Robô (ESP32 + firmware) ──WebSocket──▶ backend (NestJS) ──▶ PostgreSQL
                                             │
                                             └──WebSocket + REST──▶ frontend (Next.js) ──▶ navegador
```

O projeto junta quatro frentes:

| Frente | O que faz |
| --- | --- |
| **Estruturas** | Chassi, rodas, fixação dos motores e peças impressas em 3D |
| **Energia** | Bateria, alimentação e autonomia do robô |
| **Hardware** | Sensores, motores, ESP32 e placa de integração |
| **Software** | Firmware de navegação e aplicação web de telemetria (backend, frontend e banco) |

## Estrutura do repositório

| Pasta | Conteúdo |
| --- | --- |
| [`docs/`](docs/) | Documentação do projeto: TAP, requisitos, EAP, projetos conceituais, cronograma, orçamento, testes e riscos |
| [`hw/`](hw/) | Diagrama elétrico, simulação de circuitos e placa de integração (KiCad) |
| [`mec/`](mec/) | Projeto estrutural e mecânico |
| [`src/firmware/`](src/firmware/) | Código do ESP32 (PlatformIO): navegação, sensores e envio da telemetria |
| [`src/backend/`](src/backend/) | API NestJS: recebe a telemetria, grava no PostgreSQL e serve o histórico |
| [`src/frontend/`](src/frontend/) | Dashboard Next.js: corrida ao vivo, histórico e melhores tempos |

> [!IMPORTANT]
> A estrutura de pastas não reflete a divisão de equipes. Qualquer membro pode trabalhar em qualquer pasta, conforme a necessidade do projeto.

## Documentação

| Documento | |
| --- | --- |
| [1 - Termo de Abertura do Projeto (TAP)](docs/1%20-%20TAP.md) | Objetivos, problema, equipe e orçamento estimado |
| [2 - Requisitos](docs/2%20-%20Requisitos.md) | Requisitos funcionais e não funcionais |
| [3 - EAP](docs/3%20-%20EAP.md) | Estrutura analítica do projeto |
| [4.1 - Estruturas](docs/4.1%20-%20Projeto%20conceitual%20de%20estruturas.md) · [4.2 - Energia](docs/4.2%20-%20Projeto%20conceitual%20de%20energia.md) · [4.3 - Hardware](docs/4.3%20-%20Projeto%20conceitual%20de%20hardware.md) · [4.4 - Software](docs/4.4%20-%20Projeto%20conceitual%20de%20software.md) | Projetos conceituais de cada frente |
| [5 - Cronograma](docs/5%20-%20Cronograma.md) | Planejamento e andamento das issues |
| [6 - Orçamento](docs/6%20-%20Or%C3%A7amento.md) | Custos do projeto |
| [7.1](docs/7.1%20-%20Testes%20de%20estrutura.md) · [7.2](docs/7.2%20-%20Testes%20de%20energia.md) · [7.3](docs/7.3%20-%20Testes%20de%20hardware.md) · [7.4](docs/7.4%20-%20Testes%20de%20software.md) · [7.5](docs/7.5%20-%20Testes%20de%20integra%C3%A7%C3%A3o.md) | Testes de estrutura, energia, hardware, software e integração |
| [8 - Avaliação de desempenho](docs/8%20-%20Avalia%C3%A7%C3%A3o%20de%20desempenho.md) | Resultados do robô |
| [9 - Relatório de encerramento](docs/9%20-%20Relat%C3%B3rio%20de%20encerramento%20do%20projeto.md) | Encerramento e lições aprendidas |
| [Gerenciamento de riscos](docs/Gerenciamento%20de%20riscos/) | Matrizes de risco por sprint |

## Como rodar

### Aplicação web (banco + backend + frontend)

Só precisa do Docker e do Docker Compose:

```bash
cd src/backend
npm run docker:up   # ou: docker compose -f infra/compose.yaml up -d --build
```

Os containers sobem na ordem certa (banco → backend → frontend) e aplicam as migrações sozinhos. Depois:

- Dashboard: <http://localhost:3000>
- API REST: <http://localhost:3002> (saúde do sistema em `/health`)
- Ingestão da telemetria do robô: `ws://<ip-do-computador>:3001/ingest`

Sem o robô físico, dá para simular uma corrida com o emulador (precisa do Node.js): `npm install` e `npm run robot:emulator` dentro de `src/backend`.

Desenvolvimento sem Docker, testes e o contrato de telemetria estão em [`src/README.md`](src/README.md).

### Firmware do robô

Instalação do PlatformIO, configuração do Wi-Fi e comandos de compilação e gravação estão em [`src/firmware/README.md`](src/firmware/README.md).

## Equipe

| Frente | Membros |
| --- | --- |
| Gerência de projeto | João Felipe Veloso Rosa, Lorena Rege de Sousa Alves |
| Estruturas | João Felipe Veloso Rosa, Lorena Rege de Sousa Alves, Maria Eduarda Dantas, Jean David Bento dos Santos, Gabrielly Carvalho de Freitas |
| Energia | Júlia Pêgo de Meneses, Pedro Henrique Conceição de Souza, Kevin Ferreira Nunes de Sousa, Vinícius de Jesus Bessa Fernandes |
| Hardware | Artur Handow Krauspenhar, Vitor Coelho Costa, Laura de Carvalho Feitosa, Edilson Ribeiro da Cruz Junior |
| Software | João Pedro de Sousa Silva, Daniel Moura Peres, Eduardo Henrique Coutinho Gurjão, Felipe Fernandes Brandim |

Matrículas, cursos e contatos estão no [TAP](docs/1%20-%20TAP.md#membros-da-equipe).

## Boas práticas do repositório

- Use branches, Pull Requests com revisão e issues para organizar o trabalho.
- Para fechar a issue automaticamente no merge, escreva `Resolves #<número>` na descrição do PR.
- Evite enviar arquivos muito grandes (mais de 5 MB). Vídeos e outros arquivos pesados devem ficar em outra plataforma, com só o link aqui.
- Nunca envie segredos (`.env`, senhas do Wi-Fi, `credenciais.h`): o `.gitignore` já cobre os arquivos conhecidos.
