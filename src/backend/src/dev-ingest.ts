/**
 * SÓ PARA TESTE LOCAL: sobe apenas o TelemetryModule, sem HTTP e sem banco de dados.
 * Inicia o servidor ws em ws://localhost:3001/ingest com o log temporário de eventos ligado.
 *
 *   npm run ingest:dev
 *
 * Quando o AppModule/main.ts do backend existir, este arquivo pode ser removido.
 */
import 'reflect-metadata';
import { Module } from '@nestjs/common';
import { NestFactory } from '@nestjs/core';

// Precisa vir antes de carregar o TelemetryModule (ele lê a variável ao ser importado).
process.env.TELEMETRY_DEBUG_EVENTS ??= 'true';

async function main() {
  const { TelemetryModule } = await import('./telemetry/telemetry.module');

  @Module({ imports: [TelemetryModule] })
  class DevModule {}

  const app = await NestFactory.createApplicationContext(DevModule);
  app.enableShutdownHooks();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
