#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <vector>
#include <string>
#include <cmath>    // Для std::sqrt
#include <iostream> // Для std::cerr

// Параметры симуляции
struct SimulationParameters {
    double G = 1.0;
    double M = 1.0;
    double CENTRAL_BODY_RADIUS = 0.01;
    double DRAG_COEFFICIENT = 0.05;
    double THRUST_COEFFICIENT = 0.00;
    double DT = 0.001;
    int STEPS = 100000;

    struct InitialStateParams {
        double x = 1.5;
        double y = 0.0;
        double vx = 0.0;
        double vy = 0.8;
    } initialState;
};

// Состояние системы
struct State {
    double x, y, vx, vy;
};

class Calculations {
public:
    Calculations(); // Конструктор по умолчанию

    // Основной метод для запуска симуляции
    std::vector<State> runSimulation(const SimulationParameters& params);

private:
    // Правая часть системы дифференциальных уравнений
    static State derivatives(const State& s, const SimulationParameters& params);

    // Один шаг интегрирования методом Рунге-Кутты 4-го порядка
    static State rungeKuttaStep(const State& s, double dt, const SimulationParameters& params);
};

#endif CALCULATIONS_H