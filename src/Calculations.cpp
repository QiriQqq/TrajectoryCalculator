#include "../include/Calculations.h"

Calculations::Calculations() {
    // ����������� ����, ��� ��� ��� ������������� �������������
}

// �������� ����� ��� ������� ���������
std::vector<State> Calculations::runSimulation(const SimulationParameters& params) {
    State currentState;
    currentState.x = params.initialState.x;
    currentState.y = params.initialState.y;
    currentState.vx = params.initialState.vx;
    currentState.vy = params.initialState.vy;

    std::vector<State> trajectoryStates; // ������ ������ ���������
    trajectoryStates.reserve(static_cast<size_t>(params.STEPS) + 1);
    trajectoryStates.push_back(currentState); // ��������� ��������� ���������

    double initial_r_squared = currentState.x * currentState.x + currentState.y * currentState.y;
    if (initial_r_squared < params.CENTRAL_BODY_RADIUS * params.CENTRAL_BODY_RADIUS) {
        std::cout << "������������: ��������� ������� (" << currentState.x << ", " << currentState.y
            << ") ������ ������� ������������ ���� (" << params.CENTRAL_BODY_RADIUS << ").\n";
        return trajectoryStates;
    }

    for (int i = 0; i < params.STEPS; ++i) {
        currentState = rungeKuttaStep(currentState, params.DT, params); // �������� params ����

        trajectoryStates.push_back(currentState); // ��������� ������ ���������

        double r_squared = currentState.x * currentState.x + currentState.y * currentState.y;
        if (r_squared < params.CENTRAL_BODY_RADIUS * params.CENTRAL_BODY_RADIUS) {
            std::cout << "������������ ���������� �� ���� " << i + 1
                << " ����� ����������. ����������: (" << currentState.x << ", " << currentState.y
                << "), r = " << std::sqrt(r_squared) << "\n";
            break;
        }
    }
    return trajectoryStates;
}

// ������ ����� ������� ���������������� ���������
State Calculations::derivatives(const State& s, const SimulationParameters& params) {
    double r_squared = s.x * s.x + s.y * s.y;
    if (r_squared == 0) {
        return { s.vx, s.vy, 0, 0 };
    }
    double r = std::sqrt(r_squared);
    double r_cubed = r_squared * r;

    double common_factor_gravity = -params.G * params.M / r_cubed;
    double net_propulsion_factor = params.THRUST_COEFFICIENT - params.DRAG_COEFFICIENT;

    double ax = common_factor_gravity * s.x + net_propulsion_factor * s.vx;
    double ay = common_factor_gravity * s.y + net_propulsion_factor * s.vy;
    return { s.vx, s.vy, ax, ay };
}

// ���� ��� �������������� ������� �����-����� 4-�� �������
State Calculations::rungeKuttaStep(const State& s, double dt, const SimulationParameters& params) {
    State k1 = derivatives(s, params);

    State s_temp_k2 = {
        s.x + dt * k1.x / 2.0,
        s.y + dt * k1.y / 2.0,
        s.vx + dt * k1.vx / 2.0,
        s.vy + dt * k1.vy / 2.0
    };
    State k2 = derivatives(s_temp_k2, params);

    State s_temp_k3 = {
        s.x + dt * k2.x / 2.0,
        s.y + dt * k2.y / 2.0,
        s.vx + dt * k2.vx / 2.0,
        s.vy + dt * k2.vy / 2.0
    };
    State k3 = derivatives(s_temp_k3, params);

    State s_temp_k4 = {
        s.x + dt * k3.x,
        s.y + dt * k3.y,
        s.vx + dt * k3.vx,
        s.vy + dt * k3.vy
    };
    State k4 = derivatives(s_temp_k4, params);

    return {
        s.x + dt / 6.0 * (k1.x + 2.0 * k2.x + 2.0 * k3.x + k4.x),
        s.y + dt / 6.0 * (k1.y + 2.0 * k2.y + 2.0 * k3.y + k4.y),
        s.vx + dt / 6.0 * (k1.vx + 2.0 * k2.vx + 2.0 * k3.vx + k4.vx),
        s.vy + dt / 6.0 * (k1.vy + 2.0 * k2.vy + 2.0 * k3.vy + k4.vy)
    };
}



