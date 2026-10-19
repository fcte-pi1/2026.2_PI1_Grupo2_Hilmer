import { Controller, Get, NotFoundException, Param, Query } from '@nestjs/common';
import { RunsService } from './runs.service';

@Controller('runs')
export class RunsController {
  constructor(private readonly runsService: RunsService) {}

  @Get()
  async findAll(@Query('mazeId') mazeId?: string) {
    return this.runsService.findAll(mazeId);
  }

  @Get('best')
  async findBestRuns() {
    return this.runsService.findBestRuns();
  }

  @Get(':id')
  async findById(@Param('id') id: string) {
    const run = await this.runsService.findById(id);
    if (!run) {
      throw new NotFoundException(`Corrida com ID ${id} não encontrada.`);
    }
    return run;
  }

  @Get(':id/samples')
  async findSamples(@Param('id') id: string) {
    const samples = await this.runsService.findSamplesByRunId(id);
    if (!samples) {
      throw new NotFoundException(`Amostras para a corrida ${id} não encontradas.`);
    }
    return samples;
  }
}