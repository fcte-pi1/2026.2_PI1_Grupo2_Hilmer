// Controlador PID genérico.
// Lógica pura: não inclui Arduino.h nem ESP-IDF, então roda nos testes do PC.
#pragma once

class Pid {
public:
    // periodo em segundos; a saída fica entre -limite e +limite.
    Pid(float kp, float ki, float kd, float periodo, float limite);

    float atualizar(float referencia, float medida);
    void zerar();

private:
    float kp_;
    float ki_;
    float kd_;
    float periodo_;
    float limite_;

    float integral_ = 0.0f;
    float erroAnterior_ = 0.0f;
    bool primeiraChamada_ = true;
};
