import { ServiceUnavailableException } from '@nestjs/common';
import { Test, TestingModule } from '@nestjs/testing';
import { AppController } from './app.controller';
import { AppService, DATABASE_HEALTH_TIMEOUT_MS } from './app.service';
import { PrismaService } from './prisma/prisma.service';

describe('AppController', () => {
  let appController: AppController;
  let prisma: { $queryRaw: jest.Mock };

  beforeEach(async () => {
    prisma = { $queryRaw: jest.fn().mockResolvedValue([{ '?column?': 1 }]) };

    const app: TestingModule = await Test.createTestingModule({
      controllers: [AppController],
      providers: [AppService, { provide: PrismaService, useValue: prisma }],
    }).compile();

    appController = app.get<AppController>(AppController);
  });

  afterEach(() => {
    jest.useRealTimers();
  });

  describe('health', () => {
    it('deve responder ok com o banco de pé', async () => {
      const result = await appController.health();

      expect(result.status).toBe('ok');
      expect(typeof result.timestamp).toBe('string');
      expect(result.dependencies.database).toEqual({
        status: 'up',
        latencyMs: expect.any(Number),
      });
      expect(prisma.$queryRaw).toHaveBeenCalledTimes(1);
    });

    it('deve responder 503 com o status do banco quando a consulta falhar', async () => {
      prisma.$queryRaw.mockRejectedValueOnce(
        new Error("Can't reach database server"),
      );

      const erro = await appController.health().catch((e: unknown) => e);

      expect(erro).toBeInstanceOf(ServiceUnavailableException);
      const exception = erro as ServiceUnavailableException;
      expect(exception.getStatus()).toBe(503);
      expect(exception.getResponse()).toMatchObject({
        status: 'error',
        dependencies: {
          database: { status: 'down', latencyMs: expect.any(Number) },
        },
      });
    });

    it('deve responder 503 quando o banco não responder dentro do timeout', async () => {
      jest.useFakeTimers();
      prisma.$queryRaw.mockReturnValueOnce(new Promise(() => {}));

      const resposta = appController.health().catch((e: unknown) => e);
      await jest.advanceTimersByTimeAsync(DATABASE_HEALTH_TIMEOUT_MS);
      const erro = await resposta;

      expect(erro).toBeInstanceOf(ServiceUnavailableException);
      expect((erro as ServiceUnavailableException).getResponse()).toMatchObject(
        {
          dependencies: {
            database: { status: 'down' },
          },
        },
      );
    });
  });
});
