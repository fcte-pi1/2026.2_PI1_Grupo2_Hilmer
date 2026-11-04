# Imagem do frontend (Next.js 16 + React 19) usada pelo compose.yaml.
#   docker build -t micromouse-frontend src/frontend

# ---------- build: dependências completas e compilação Next.js ----------
FROM node:24-alpine AS build
WORKDIR /app

ENV NEXT_TELEMETRY_DISABLED=1

# Argumentos opcionais caso a imagem seja compilada apontando para URLs externas
ARG NEXT_PUBLIC_API_URL=http://localhost:3002
ARG NEXT_PUBLIC_USE_MOCK=false
ARG NEXT_PUBLIC_LIVE_URL=

ENV NEXT_PUBLIC_API_URL=${NEXT_PUBLIC_API_URL} \
    NEXT_PUBLIC_USE_MOCK=${NEXT_PUBLIC_USE_MOCK} \
    NEXT_PUBLIC_LIVE_URL=${NEXT_PUBLIC_LIVE_URL}

COPY package.json package-lock.json ./
RUN npm ci

COPY tsconfig.json next.config.ts ./
COPY public ./public
COPY src ./src

RUN npm run build || npm run build

# Mantém apenas as dependências de produção para enxugar a imagem final
RUN npm prune --omit=dev

# ---------- runtime: apenas os artefatos necessários para servir ----------
FROM node:24-alpine AS runtime
WORKDIR /app

ENV NODE_ENV=production \
    PORT=3000 \
    NEXT_TELEMETRY_DISABLED=1 \
    API_URL=http://backend:3002

COPY --from=build --chown=node:node /app/package.json ./
COPY --from=build --chown=node:node /app/next.config.ts ./
COPY --from=build --chown=node:node /app/node_modules ./node_modules
COPY --from=build --chown=node:node /app/.next ./.next
COPY --from=build --chown=node:node /app/public ./public

USER node

EXPOSE 3000

HEALTHCHECK --interval=10s --timeout=5s --start-period=15s --retries=3 \
  CMD ["node", "-e", "fetch('http://127.0.0.1:' + (process.env.PORT || 3000)).then(r => process.exit(r.ok ? 0 : 1)).catch(() => process.exit(1))"]

CMD ["./node_modules/.bin/next", "start", "-p", "3000"]
