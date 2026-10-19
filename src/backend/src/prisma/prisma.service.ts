import {
  Injectable,
  Logger,
  OnModuleDestroy,
  OnModuleInit,
} from '@nestjs/common';
import { PrismaClient } from '@prisma/client';

@Injectable()
export class PrismaService
  extends PrismaClient
  implements OnModuleInit, OnModuleDestroy
{
  private readonly logger = new Logger(PrismaService.name);

  async onModuleInit(): Promise<void> {
    try {
      await this.$connect();
      this.logger.log(
        'Conexão com PostgreSQL via Prisma estabelecida com sucesso.',
      );
    } catch (error: any) {
      this.logger.warn(
        `Não foi possível conectar ao banco de dados no startup: ${error.message}. O backend continuará operando.`,
      );
    }
  }

  async onModuleDestroy(): Promise<void> {
    await this.$disconnect();
    this.logger.log('Conexão com PostgreSQL encerrada.');
  }
}
