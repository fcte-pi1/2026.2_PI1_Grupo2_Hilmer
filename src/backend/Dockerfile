# Imagem do backend (NestJS + Prisma) usada pelo compose.yaml e pela Render.
#   docker build -t micromouse-backend src/backend

# ---------- build: dependências completas, Prisma Client e compilação ----------
FROM node:24-alpine AS build
WORKDIR /app

# O engine do Prisma precisa do OpenSSL no Alpine
RUN apk add --no-cache openssl

COPY package.json package-lock.json ./
RUN npm ci

COPY prisma ./prisma
RUN npx prisma generate

COPY tsconfig.json tsconfig.build.json nest-cli.json ./
COPY src ./src
RUN npm run build

# Só as dependências de produção seguem para o runtime; o Prisma Client
# gerado (node_modules/.prisma) é mantido.
RUN npm prune --omit=dev

# ---------- runtime: só o necessário para rodar ----------
FROM node:24-alpine AS runtime
WORKDIR /app

RUN apk add --no-cache openssl

ENV NODE_ENV=production \
    PORT=3002 \
    TELEMETRY_WS_PORT=3001

COPY --from=build /app/package.json ./
COPY --from=build /app/node_modules ./node_modules
COPY --from=build /app/dist ./dist
COPY --from=build /app/prisma ./prisma
COPY infra/scripts/healthcheck.js ./infra/scripts/healthcheck.js

# Usuário sem privilégios que já vem na imagem oficial do Node
USER node

# 3002: API REST + Socket.IO; 3001: WebSocket de ingestão (/ingest)
EXPOSE 3002 3001

HEALTHCHECK --interval=10s --timeout=5s --start-period=20s --retries=3 \
  CMD ["node", "infra/scripts/healthcheck.js"]

# Aplica as migrações pendentes antes de subir. Se o banco não responder, o
# container sai e a política de restart tenta de novo.
CMD ["sh", "-c", "./node_modules/.bin/prisma migrate deploy && exec node dist/main"]
