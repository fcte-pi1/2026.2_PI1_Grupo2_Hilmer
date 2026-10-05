// Driver dos motores (ponte H TB6612FNG).
#pragma once

void motores_iniciar();

// Velocidade de -1.0 (ré total) a 1.0 (frente total).
void motores_definir_velocidade(float esquerdo, float direito);

// Freio ativo nas duas rodas (IN1 = IN2 = 1).
void motores_frear();
