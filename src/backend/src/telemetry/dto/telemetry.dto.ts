import 'reflect-metadata';
import {
  IsInt,
  IsNumber,
  IsString,
  IsNotEmpty,
  IsOptional,
  IsBoolean,
  IsIn,
  ValidateNested,
  Equals,
  Min,
  Max,
} from 'class-validator';
import { Type } from 'class-transformer';

export type HeadingDirection = 'N' | 'E' | 'S' | 'W';
export type RobotState =
  | 'IDLE'
  | 'EXPLORING'
  | 'RETURNING'
  | 'SPEED_RUN'
  | 'GOAL_REACHED'
  | 'ERROR';

export type RobotEventType =
  | 'CALIBRATION_DONE'
  | 'RUN_START'
  | 'GOAL_REACHED'
  | 'RUN_END';

export class PositionDto {
  @IsInt()
  @Min(0)
  x: number;

  @IsInt()
  @Min(0)
  y: number;
}

export class WallsDto {
  @IsOptional()
  @IsBoolean()
  n?: boolean;

  @IsOptional()
  @IsBoolean()
  e?: boolean;

  @IsOptional()
  @IsBoolean()
  s?: boolean;

  @IsOptional()
  @IsBoolean()
  w?: boolean;
}

export class BaseMessageDto {
  @IsInt()
  @Equals(1, { message: 'A versão do protocolo deve ser 1 (v: 1)' })
  v: number;

  @IsString()
  @IsIn(['telemetry', 'event'])
  type: 'telemetry' | 'event';

  @IsString()
  @IsNotEmpty()
  robot_id: string;

  @IsInt()
  @Min(0)
  seq: number;

  @IsInt()
  @Min(0)
  t_ms: number;
}

export class TelemetryPayloadDto extends BaseMessageDto {
  @Equals('telemetry')
  type: 'telemetry';

  @ValidateNested()
  @Type(() => PositionDto)
  pos: PositionDto;

  @IsIn(['N', 'E', 'S', 'W'])
  heading: HeadingDirection;

  @IsNumber()
  @Min(0)
  speed_mm_s: number;

  @IsOptional()
  @IsInt()
  @Min(0)
  odo_mm?: number;

  @IsNumber()
  @Min(0)
  @Max(100)
  battery_pct: number;

  @IsOptional()
  @ValidateNested()
  @Type(() => WallsDto)
  walls?: WallsDto;

  @IsIn([
    'IDLE',
    'EXPLORING',
    'RETURNING',
    'SPEED_RUN',
    'GOAL_REACHED',
    'ERROR',
  ])
  state: RobotState;

  @IsOptional()
  sensores?: any;
}

export class EventPayloadDto extends BaseMessageDto {
  @Equals('event')
  type: 'event';

  @IsIn(['CALIBRATION_DONE', 'RUN_START', 'GOAL_REACHED', 'RUN_END'])
  event: RobotEventType;

  @IsOptional()
  @IsBoolean()
  ok?: boolean;

  @IsOptional()
  @IsNumber()
  @Min(0)
  @Max(100)
  battery_pct?: number;

  @IsOptional()
  @ValidateNested()
  @Type(() => PositionDto)
  pos?: PositionDto;
}
