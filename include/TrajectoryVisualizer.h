#pragma once
#ifndef TRAJECTORYVISUALIZER_H
#define TRAJECTORYVISUALIZER_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>    // Для std::sqrt, std::min, std::max
#include <iostream> // Для std::cerr, std::cout
#include <fstream>  // Для std::ifstream, std::ofstream
#include <sstream>  // Для std::istringstream, std::ostringstream
#include <iomanip>  // Для std::fixed, std::setprecision
#include <algorithm> // Для std::min, std::max (на всякий)

using WorldTrajectoryPoint = std::pair<double, double>;
using WorldTrajectoryData = std::vector<WorldTrajectoryPoint>;

class TrajectoryVisualizer {
public:
    TrajectoryVisualizer(unsigned int width, unsigned int height, const std::wstring& windowTitle = L"2D-визуализатор траектории");

    void setData(const WorldTrajectoryData& data);
    void run();
    void resetViewAndAnimation();

private:
    // --- Константы визуализации ---
    static constexpr float DEFAULT_SCALE = 150.0f;
    static constexpr unsigned int DEFAULT_POINTS_PER_FRAME = 1u;
    static constexpr unsigned int MIN_POINTS_PER_FRAME = 1u;
    static constexpr unsigned int MAX_POINTS_PER_FRAME = 2048u;
    static constexpr unsigned int ANIMATION_SPEED_MULTIPLIER = 2;
    const std::string FONT_FILENAME = "assets/fonts/arial.ttf";
    static constexpr unsigned int INFO_TEXT_CHAR_SIZE = 16;
    static constexpr float CENTER_POINT_RADIUS = 5.0f;
    static constexpr float TRAJECTORY_START_POINT_RADIUS = 2.0f;
    static constexpr float ZOOM_FACTOR_STEP = 1.3f;

    sf::RenderWindow m_window;
    WorldTrajectoryData m_worldTrajectoryData;
    std::vector<sf::Vertex> m_screenTrajectory;

    float m_scale;
    sf::Vector2f m_offset;
    sf::Vector2f m_screenCenter;

    size_t m_currentPointIndex;
    unsigned int m_pointsPerFrame;
    bool m_isPaused;
    bool m_showAllPointsImmediately;

    sf::Font m_font;
    sf::Text m_infoText;

    bool m_isDragging;
    sf::Vector2i m_lastMousePos;

    // Приватные методы
    sf::Vector2f toScreenCoords(double worldX, double worldY) const;
    sf::Vector2f toWorldCoords(sf::Vector2f screenPos) const;
    void recalculateScreenTrajectory();
    void setupInfoText();
    void updateInfoText();
    void handleEvent(const sf::Event& event);
    void handleKeyPress(const sf::Event::KeyEvent& keyEvent);
    void updateAnimation();
    void draw();
};

#endif TRAJECTORYVISUALIZER_H