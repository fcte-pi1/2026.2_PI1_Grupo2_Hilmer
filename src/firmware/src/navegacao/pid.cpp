#include "pid.h"

Pid::Pid(float kp, float ki, float kd, float periodo, float limite)
    : kp_(kp), ki_(ki), kd_(kd), periodo_(periodo), limite_(limite) {}

float Pid::atualizar(float referencia, float medida) {
    float erro = referencia - medida;
    integral_ += erro * periodo_;

    // Sem erro anterior na primeira chamada, o D fica em zero para não dar pico.
    float derivada = 0.0f;
    if (!primeiraChamada_) {
        derivada = (erro - erroAnterior_) / periodo_;
    }
    erroAnterior_ = erro;
    primeiraChamada_ = false;

    float saida = kp_ * erro + ki_ * integral_ + kd_ * derivada;

    // Anti-windup: com a saída saturada, não acumula mais erro na integral.
    bool saturouParaCima = saida > limite_ && erro > 0.0f;
    bool saturouParaBaixo = saida < -limite_ && erro < 0.0f;
    if (saturouParaCima || saturouParaBaixo) {
        integral_ -= erro * periodo_;
    }

    if (saida > limite_) {
        saida = limite_;
    } else if (saida < -limite_) {
        saida = -limite_;
    }
    return saida;
}

void Pid::zerar() {
    integral_ = 0.0f;
    erroAnterior_ = 0.0f;
    primeiraChamada_ = true;
}
