# _Frontend_

Esta pasta deverá armazenar arquivos referentes a:

- Código-fonte da interface: componentes, páginas, estilos e lógica de apresentação, organizados conforme o framework utilizado ([React](https://react.dev/), [Vue](https://vuejs.org/), [Angular](https://angular.io/) etc.).
- Arquivos de marcação e estilo estáticos: `index.html`, arquivos `.css`, `.scss` ou `.sass` de estilização global.
- Arquivos de definição de dependências: `package.json` e `package-lock.json` (ou `yarn.lock`) com todas as bibliotecas utilizadas.
- Arquivos de configuração do bundler/toolchain: `vite.config.js`, `webpack.config.js`, `tsconfig.json` etc.
- Arquivos de configuração de ambiente: `.env.example` com as variáveis de ambiente públicas necessárias (ex.: URL base da API).
- Arquivos de containerização: `Dockerfile` e `docker-compose.yml`, caso a aplicação seja servida via contêiner.

Evite incluir:

- Dependências instaladas: a pasta `node_modules/` deve ser gerada localmente via `npm install` ou equivalente e nunca incluída no repositório.
- Artefatos de build: diretórios como `dist/`, `build/` ou `.next/` são gerados pelo processo de compilação e não devem ser versionados.
- Arquivos de configuração pessoal: arquivos como `.directory` (Linux/KDE) ou configurações locais de editor (ex.: `.vscode/settings.json`), salvo configurações compartilhadas como `.editorconfig`.
- Credenciais e segredos: arquivos `.env` com valores reais nunca devem ser versionados.

> [!WARNING]
> **Não acrescente arquivos referentes ao _backend_ nesta pasta.** Eles deverão ser armazenados na pasta [backend](https://github.com/fcte-pi1/template/tree/main/src/backend) deste repositório.

Como rodar:

### Frontend (Next.js)

Pré-requisito: Node.js 20+ e npm.

```bash
cd src/frontend
cp .env.example .env.local
npm install
npm install socket.io-client
npm run dev
```

Abra [http://localhost:3000](http://localhost:3000).

- Dashboard: `/`
- Histórico: `/historico`

Com as variáveis padrão do `.env.example` não é preciso backend: o mapa usa uma corrida simulada e o histórico usa dados mock.

## Variáveis de ambiente (`.env.local`)

| Variável                       | Padrão                  | Para que serve                                                              |
| :----------------------------- | :---------------------- | :-------------------------------------------------------------------------- |
| `NEXT_PUBLIC_API_URL`          | `http://localhost:3001` | Endereço do backend.                                                        |
| `NEXT_PUBLIC_USE_MOCK`         | `true`                  | Histórico: `true` usa dados mock; `false` busca no backend.                 |
| `NEXT_PUBLIC_TELEMETRY_SOURCE` | `simulated`             | Mapa ao vivo: `simulated` (sem backend) ou `socket` (Socket.IO do backend). |

Depois de mudar o `.env.local`, reinicie o `npm run dev`.
