import { Injectable, Logger } from '@nestjs/common';
import { PrismaService } from './prisma/prisma.service';

/** Tempo máximo do `SELECT 1` antes de considerar o banco fora do ar. */
export const DATABASE_HEALTH_TIMEOUT_MS = 2000;

export interface DependencyHealth {
  status: 'up' | 'down';
  latencyMs: number;
}

export interface HealthReport {
  status: 'ok' | 'error';
  timestamp: string;
  dependencies: { database: DependencyHealth };
}

@Injectable()
export class AppService {
  private readonly logger = new Logger(AppService.name);

  constructor(private readonly prisma: PrismaService) {}

  /**
   * Healthcheck profundo: testa cada dependência e só responde `ok` se todas
   * estiverem de pé. Usado pelo Docker e pela Render (RNF-18).
   */
  async checkHealth(): Promise<HealthReport> {
    const database = await this.checkDatabase();

    return {
      status: database.status === 'up' ? 'ok' : 'error',
      timestamp: new Date().toISOString(),
      dependencies: { database },
    };
  }

  /**
   * `SELECT 1` via Prisma com timeout, para o healthcheck não ficar preso
   * esperando a conexão quando o banco some da rede.
   */
  private async checkDatabase(): Promise<DependencyHealth> {
    const inicio = Date.now();
    let timer: NodeJS.Timeout | undefined;

    const timeout = new Promise<never>((_, reject) => {
      timer = setTimeout(
        () =>
          reject(
            new Error(
              `Sem resposta do banco em ${DATABASE_HEALTH_TIMEOUT_MS} ms`,
            ),
          ),
        DATABASE_HEALTH_TIMEOUT_MS,
      );
    });

    try {
      await Promise.race([this.prisma.$queryRaw`SELECT 1`, timeout]);
      return { status: 'up', latencyMs: Date.now() - inicio };
    } catch (err: unknown) {
      // O motivo vai só para o log: a mensagem do Prisma pode trazer o host
      // do banco, e o /health é público na Render.
      const msg = err instanceof Error ? err.message : String(err);
      // O Prisma põe o motivo na última linha ("Invalid `prisma.$queryRaw()`
      // invocation: ... Server has closed the connection.")
      const motivo = msg.trim().split('\n').pop();
      this.logger.warn(`Healthcheck: banco indisponível: ${motivo}`);
      return { status: 'down', latencyMs: Date.now() - inicio };
    } finally {
      clearTimeout(timer);
    }
  }
}
