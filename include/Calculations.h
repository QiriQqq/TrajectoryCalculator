#ifndef CALCULATIONS_H
#define CALCULATIONS_H

#include <vector>
#include <string>
#include <cmath>    // ��� std::sqrt
#include <iostream> // ��� std::cerr

// ��������� ���������
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

// ��������� �������
struct State {
    double x, y, vx, vy;
};

class Calculations {
public:
    Calculations(); // ����������� �� ���������

    // �������� ����� ��� ������� ���������
    std::vector<State> runSimulation(const SimulationParameters& params);

private:
    // ������ ����� ������� ���������������� ���������
    static State derivatives(const State& s, const SimulationParameters& params);

    // ���� ��� �������������� ������� �����-����� 4-�� �������
    static State rungeKuttaStep(const State& s, double dt, const SimulationParameters& params);
};

#endif CALCULATIONS_H