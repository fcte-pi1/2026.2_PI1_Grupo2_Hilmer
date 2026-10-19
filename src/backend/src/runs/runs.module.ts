import { Module } from '@nestjs/common';
import { RunsController } from './runs.controller';
import { PrismaModule } from '../prisma/prisma.module';
import { RunsService } from './runs.service';

@Module({
  imports: [PrismaModule],
  providers: [RunsService],
  exports: [RunsService],
  controllers: [RunsController],
})
export class RunsModule {}
