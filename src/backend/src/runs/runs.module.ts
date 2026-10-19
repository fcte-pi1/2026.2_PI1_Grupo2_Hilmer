import { Module } from '@nestjs/common';
import { PrismaModule } from '../prisma/prisma.module';
import { RunsService } from './runs.service';

@Module({
  imports: [PrismaModule],
  providers: [RunsService],
  exports: [RunsService],
})
export class RunsModule {}
