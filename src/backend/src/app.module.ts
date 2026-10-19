import { Module } from '@nestjs/common';
import { AppController } from './app.controller';
import { AppService } from './app.service';
import { PrismaModule } from './prisma/prisma.module';
import { RunsModule } from './runs/runs.module';
import { TelemetryModule } from './telemetry/telemetry.module';

@Module({
  imports: [PrismaModule, TelemetryModule, RunsModule],
  controllers: [AppController],
  providers: [AppService],
})
export class AppModule {}
