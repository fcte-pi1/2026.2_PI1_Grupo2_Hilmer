import { Controller, Get } from '@nestjs/common';
import { RunsService } from './runs.service';

@Controller('runs')
export class RunsController {
  constructor(private readonly runsService: RunsService) {}

  @Get()
  async getRuns() {
    return this.runsService.findAll();
  }

  @Get('records')
  async getRecords() {
    return this.runsService.findRecords();
  }
}