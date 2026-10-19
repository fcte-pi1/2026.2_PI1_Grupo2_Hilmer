import { Injectable } from '@nestjs/common';
import { PrismaService } from '../infra/prisma.service';

@Injectable()
export class RunsService {
  constructor(private readonly prisma: PrismaService) {}

  async findAll() {
    return this.prisma.corrida.findMany({
      orderBy: { criadoEm: 'desc' },
    });
  }

  async findRecords() {
    return this.prisma.corrida.findMany({
      where: { eRecorde: true },
      orderBy: { tempoMs: 'asc' },
    });
  }
}