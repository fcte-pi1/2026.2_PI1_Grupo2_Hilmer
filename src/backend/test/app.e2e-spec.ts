import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication } from '@nestjs/common';
import request from 'supertest';
import { App } from 'supertest/types';
import { AppModule } from './../src/app.module';
import { HealthReport } from './../src/app.service';
import { PrismaService } from './../src/prisma/prisma.service';

describe('AppController (e2e)', () => {
  let app: INestApplication<App>;
  // O teste não depende de um Postgres de pé: o banco é simulado e cada
  // caso decide se o SELECT 1 responde ou falha.
  const prisma = {
    $queryRaw: jest.fn(),
    $connect: jest.fn(),
    $disconnect: jest.fn(),
    onModuleInit: jest.fn(),
    onModuleDestroy: jest.fn(),
  };

  beforeEach(async () => {
    // Porta aleatória para o /ingest, para não colidir com um backend já de pé
    process.env.TELEMETRY_WS_PORT = '0';

    const moduleFixture: TestingModule = await Test.createTestingModule({
      imports: [AppModule],
    })
      .overrideProvider(PrismaService)
      .useValue(prisma)
      .compile();

    app = moduleFixture.createNestApplication();
    await app.init();
  });

  it('/health (GET) responde 200 com o banco de pé', () => {
    prisma.$queryRaw.mockResolvedValueOnce([{ '?column?': 1 }]);

    return request(app.getHttpServer())
      .get('/health')
      .expect(200)
      .expect((res) => {
        const body = res.body as HealthReport;
        expect(body.status).toBe('ok');
        expect(typeof body.timestamp).toBe('string');
        expect(body.dependencies.database.status).toBe('up');
      });
  });

  it('/health (GET) responde 503 com o banco fora', () => {
    prisma.$queryRaw.mockRejectedValueOnce(
      new Error("Can't reach database server"),
    );

    return request(app.getHttpServer())
      .get('/health')
      .expect(503)
      .expect((res) => {
        const body = res.body as HealthReport;
        expect(body.status).toBe('error');
        expect(body.dependencies.database.status).toBe('down');
        // O motivo fica só no log, não na resposta pública
        expect(JSON.stringify(body)).not.toContain('database server');
      });
  });

  afterEach(async () => {
    await app.close();
  });
});
