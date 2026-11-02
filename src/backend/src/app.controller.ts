import { Controller, Get, ServiceUnavailableException } from '@nestjs/common';
import { AppService } from './app.service';

@Controller()
export class AppController {
  constructor(private readonly appService: AppService) {}

  /** 200 com tudo de pé; 503 (mesmo corpo) se alguma dependência cair. */
  @Get('health')
  async health() {
    const report = await this.appService.checkHealth();
    if (report.status !== 'ok') {
      throw new ServiceUnavailableException(report);
    }
    return report;
  }
}
