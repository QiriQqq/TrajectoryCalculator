#pragma once

#ifndef USERINTERFACE_H
#define USERINTERFACE_H
#include "../include/Calculations.h" // Включаем Calculations.h для доступа к State

#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>
#include <TGUI/Widgets/TextArea.hpp>
#include <TGUI/Widgets/MenuBar.hpp>     // Для MenuBar
#include <TGUI/Widgets/FileDialog.hpp>  // Для FileDialog
#include <TGUI/Widgets/ChildWindow.hpp> // Для ChildWindow
#include <TGUI/Widgets/VerticalLayout.hpp>

#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

struct TableRowData {
    float h_days;
    float x, y;
    float Vx, Vy;
};

struct InputParameters {  
   double m_satellite_kg = 0.0;  
   double M_central_body_factor = 0.0;  
   double V0_m_per_s = 0.0;  
   double T_days = 0.0;  
   double k_coeff = 0.0;  
   double F_coeff = 0.0;  
   bool isValid = true;  
   std::wstring errorMessage;  
};

class UserInterface {
public:
    UserInterface();
    void run();

private:
    static constexpr float INPUT_FIELD_WIDTH = 140.f;
    static constexpr float INPUT_ROW_HEIGHT = 30.f;
    static constexpr float PANEL_PADDING = 10.f;
    static constexpr float WIDGET_SPACING = 10.f;
    static constexpr float HEADER_HEIGHT = 30.f;
    static constexpr float TITLE_HEIGHT = 30.f;
    static constexpr float SCROLLBAR_WIDTH_ESTIMATE = 16.f;
    
    double m_lastCalculationDT = 0.001;

    const std::string PARAMS_FILENAME = "data/simulation_params.txt";
    const std::string README_FILENAME = "data/README.txt";
    const std::string TEST_DATA_FILENAME = "data/test_data.txt";

    bool m_readmeNeedsInitialization = false;

    void initializeGui();
    
    // Методы-обработчики для новых пунктов меню
    void onSaveParamsAsMenuItemClicked();
    void onSaveTrajectoryDataAsMenuItemClicked();
    void onOpenDataFolderMenuItemClicked();
    void onShowHelpMenuItemClicked();       // <--- НОВЫЙ МЕТОД
    void onShowAboutMenuItemClicked();      // <--- (Заготовка для "О программе")
    void loadMenuBar();

    void loadWidgets();
    void loadLeftPanelWidgets();
    void loadRightPanelWidgets();
    void loadTrajectoryWidgets(tgui::Panel::Ptr parentPanel);
    void loadTableWidgets(tgui::Panel::Ptr parentPanel);
    
    void setupLayout();
    void connectSignals();
    void handleEvents();
    
    void update();
    void render();
    
    void onCalculateButtonPressed();
    void onShowVisualizerButtonPressed();
    void onLoadTestDataButtonPressed();

    void populateTable(const std::vector<TableRowData>& data);
    InputParameters validateAndParseParameters(
        const std::string& m_str, const std::string& M_str, const std::string& V0_str,
        const std::string& T_str, const std::string& k_str, const std::string& F_str
    );

    void drawTrajectoryOnCanvas(sf::RenderTarget& target_rt);
    void prepareTrajectoryForDisplay();

    sf::RenderWindow m_window;
    tgui::Gui m_gui;
    
    tgui::MenuBar::Ptr m_menuBar;           // <--- НОВЫЙ МЕНЮ БАР

    tgui::Label::Ptr m_inputTitleLabel;
    tgui::EditBox::Ptr m_edit_m;
    tgui::EditBox::Ptr m_edit_M;
    tgui::EditBox::Ptr m_edit_V0;
    tgui::EditBox::Ptr m_edit_T;
    tgui::EditBox::Ptr m_edit_k;
    tgui::EditBox::Ptr m_edit_F;
    tgui::Button::Ptr m_calculateButton;
    tgui::Button::Ptr m_showVisualizerButton;
    tgui::Button::Ptr m_loadTestDataButton;
    tgui::Grid::Ptr m_inputControlsGrid;
    tgui::Label::Ptr m_errorMessagesLabel;
    tgui::TextArea::Ptr m_readmeTextBox;

    tgui::Panel::Ptr m_leftPanel;
    tgui::Panel::Ptr m_rightPanel;
    tgui::Panel::Ptr m_trajectoryContainerPanel;
    tgui::Panel::Ptr m_tableContainerPanel;

    tgui::Label::Ptr m_trajectoryTitleLabel;
    tgui::Canvas::Ptr m_trajectoryCanvas;
    sf::Font m_sfmlFont;

    std::vector<TableRowData> m_currentTableData;
    std::vector<State> m_calculatedStates;
    std::vector<sf::Vertex> m_trajectoryDisplayPoints;
    bool m_trajectoryAvailable;

    sf::View m_fittedCanvasView;

    tgui::Label::Ptr m_tableTitleLabel;
    tgui::Grid::Ptr m_tableHeaderGrid;
    tgui::ScrollablePanel::Ptr m_tableDataPanel;
    tgui::Grid::Ptr m_tableDataGrid;
};

#endif USERINTERFACE_H