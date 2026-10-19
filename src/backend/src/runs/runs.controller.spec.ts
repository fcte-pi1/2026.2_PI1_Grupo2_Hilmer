import { NotFoundException } from '@nestjs/common';
import { Test, TestingModule } from '@nestjs/testing';
import { RunsController } from './runs.controller';
import { RunsService } from './runs.service';

describe('RunsController', () => {
  let controller: RunsController;
  let service: {
    findAll: jest.Mock;
    findBestRuns: jest.Mock;
    findById: jest.Mock;
    findSamplesByRunId: jest.Mock;
  };

  const mockRun = {
    id: 'run-1',
    labirintoId: 'lab-1',
    labirinto: '4x4',
    status: 'FINALIZADA',
    iniciadaEm: new Date('2026-10-18T10:00:00Z'),
    finalizadaEm: new Date('2026-10-18T10:00:20Z'),
    duracaoSegundos: 20,
  };

  beforeEach(async () => {
    service = {
      findAll: jest.fn().mockResolvedValue([mockRun]),
      findBestRuns: jest.fn().mockResolvedValue([
        { labirinto: '4x4', tempoSegundos: 20, corridaId: 'run-1' },
      ]),
      findById: jest.fn().mockResolvedValue(mockRun),
      findSamplesByRunId: jest.fn().mockResolvedValue([]),
    };

    const module: TestingModule = await Test.createTestingModule({
      controllers: [RunsController],
      providers: [{ provide: RunsService, useValue: service }],
    }).compile();

    controller = module.get<RunsController>(RunsController);
  });

  it('deve listar todas as corridas', async () => {
    const result = await controller.findAll();
    expect(result).toEqual([mockRun]);
    expect(service.findAll).toHaveBeenCalledWith(undefined);
  });

  it('deve retornar o recorde por labirinto', async () => {
    const result = await controller.findBestRuns();
    expect(result).toHaveLength(1);
    expect(service.findBestRuns).toHaveBeenCalled();
  });

  it('deve retornar detalhes de uma corrida por ID', async () => {
    const result = await controller.findById('run-1');
    expect(result).toEqual(mockRun);
    expect(service.findById).toHaveBeenCalledWith('run-1');
  });

  it('deve lançar NotFoundException quando a corrida não existir', async () => {
    service.findById.mockResolvedValueOnce(null);
    await expect(controller.findById('run-inexistente')).rejects.toThrow(
      NotFoundException,
    );
  });

  it('deve retornar amostras de uma corrida', async () => {
    const result = await controller.findSamples('run-1');
    expect(result).toEqual([]);
    expect(service.findSamplesByRunId).toHaveBeenCalledWith('run-1');
  });

  it('deve lançar NotFoundException quando as amostras/corrida não existirem', async () => {
    service.findSamplesByRunId.mockResolvedValueOnce(null);
    await expect(controller.findSamples('run-inexistente')).rejects.toThrow(
      NotFoundException,
    );
  });
});