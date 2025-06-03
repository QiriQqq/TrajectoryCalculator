#ifdef _MSC_VER // ������ ��� ����������� MSVC
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h> // ��� ShellExecute
#include <filesystem> // ��� C++17 std::filesystem (�����������, ��� ���������� ����)
#endif

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include "../include/TrajectoryVisualizer.h"
#include "../include/UserInterface.h"

#include <iostream>     // ��� �������
#include <algorithm>    // ��� std::min_element, std::max_element
#include <cmath>        // ��� std::pow, std::sqrt
#include <fstream>      // ��� std::ofstream � std::ifstream
#include <sstream>      // ��� std::stringstream
#include <string>       // ��� std::string, std::stod, substr, find
#include <locale>       // ��� std::locale, std::codecvt
#include <codecvt>      // ��� std::wstring_convert

// --- ��������������� ������� ��� �������� ������ ����� ---
static std::pair<tgui::Label::Ptr, tgui::EditBox::Ptr> createInputRowControls(const sf::String& labelText, float editBoxWidth, float rowHeight) {
    tgui::String tguiLabelText(labelText);
    
    auto label = tgui::Label::create(tgui::String(labelText));
    if (label) {
        label->getRenderer()->setTextColor(tgui::Color::Black);
        label->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);
        label->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left);
    }

    auto editBox = tgui::EditBox::create();
    if (editBox) {
        editBox->setSize({ editBoxWidth, rowHeight });
    }
    return { label, editBox };
}

// --- ����������� � ������������� ---
UserInterface::UserInterface()
    : m_window({ 1200, 800 }, L"������ ���������� �������� ����"),
    m_gui(m_window),
    m_trajectoryAvailable(false) {

    m_gui.setFont("assets/fonts/arial.ttf");

    // �������� ������ ��� SFML (������������ �� Canvas)
    if (!m_sfmlFont.loadFromFile("assets/fonts/arial.ttf")) {
        std::cerr << "SFML: Error - Failed to load font 'arial.ttf' for SFML rendering!\n";
    }

    initializeGui();
}

void UserInterface::initializeGui() {
    loadWidgets();
    setupLayout(); // �������� setupLayout ����� loadWidgets
    connectSignals();
    populateTable({}); // ��������� ������ ��������� �������
}

void UserInterface::loadMenuBar() {
    m_menuBar = tgui::MenuBar::create();
    if (!m_menuBar) {
        std::cerr << "Error: Failed to create MenuBar" << std::endl;
        return;
    }
    m_menuBar->getRenderer()->setBackgroundColor(tgui::Color(210, 210, 210)); // ���� ���� ����
    m_menuBar->getRenderer()->setTextColor(tgui::Color::Black);
    m_menuBar->getRenderer()->setSelectedBackgroundColor(tgui::Color(0, 110, 220));
    m_menuBar->getRenderer()->setSelectedTextColor(tgui::Color::White);
    m_menuBar->setHeight(28); // ������ ������ ����

    // --- ���� "����" ---
    m_menuBar->addMenu(L"����");
    m_menuBar->addMenuItem(L"����", L"��������� ��������� ���...");
    m_menuBar->addMenuItem(L"����", L"��������� ������ ���������� ���...");
    m_menuBar->addMenuItem(L"����", L"������� ����� � �������");
    m_menuBar->addMenuItem(L"����", L"�����");

    // --- ���� "�������" ---
    m_menuBar->addMenu(L"�������");
    m_menuBar->addMenuItem(L"�������", L"����������� ������������");
    m_menuBar->addMenuItem(L"�������", L"� ���������"); 

    // ���������� ������� ��� ������� ���� "����"
    m_menuBar->onMenuItemClick.connect([this](const std::vector<tgui::String>& menuItemHierarchy) {
        if (menuItemHierarchy.empty()) return;

        const tgui::String& menuName = menuItemHierarchy[0];
        tgui::String itemName = (menuItemHierarchy.size() > 1) ? menuItemHierarchy[1] : "";

        if (menuName == L"����") {
            if (itemName == L"��������� ��������� ���...") {
                onSaveParamsAsMenuItemClicked();
            }
            else if (itemName == L"��������� ������ ���������� ���...") {
                onSaveTrajectoryDataAsMenuItemClicked();
            }
            else if (itemName == L"������� ����� � �������") {
                onOpenDataFolderMenuItemClicked();
            }
            else if (itemName == L"�����") {
                m_window.close();
            }
        }
        else if (menuName == L"�������") {
            if (itemName == L"����������� ������������") {
                onShowHelpMenuItemClicked();
            }
            else if (itemName == L"� ���������") {
                onShowAboutMenuItemClicked(); // ������� �����, ������� ���� �� ����������
            }
        }

    });
    m_gui.add(m_menuBar); // ��������� MenuBar �������� � Gui
}

// --- �������� �������� ---
void UserInterface::loadWidgets() {
    loadMenuBar();
    loadLeftPanelWidgets();
    loadRightPanelWidgets();
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText("");
}

void UserInterface::loadLeftPanelWidgets() {
    m_leftPanel = tgui::Panel::create();
    if (!m_leftPanel) { std::cerr << "Error: Failed to create m_leftPanel" << std::endl; return; }
    m_leftPanel->getRenderer()->setBackgroundColor(tgui::Color(220, 220, 220));
    m_leftPanel->getRenderer()->setBorders({ 1, 1, 1, 1 });
    m_leftPanel->getRenderer()->setBorderColor(tgui::Color::Black);
    m_gui.add(m_leftPanel);

    // 1. ��������� "�������� ��������"
    m_inputTitleLabel = tgui::Label::create(L"�������� ��������");
    if (!m_inputTitleLabel) { std::cerr << "Error: Failed to create m_inputTitleLabel" << std::endl; return; }
    m_inputTitleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_inputTitleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
    m_inputTitleLabel->getRenderer()->setTextColor(tgui::Color::Black);
    m_inputTitleLabel->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), TITLE_HEIGHT });
    m_inputTitleLabel->setPosition({ PANEL_PADDING, PANEL_PADDING });
    m_leftPanel->add(m_inputTitleLabel);

    // 2. Grid ��� ����� �����
    m_inputControlsGrid = tgui::Grid::create();
    if (!m_inputControlsGrid) { std::cerr << "Error: Failed to create m_inputControlsGrid" << std::endl; return; }
    // ������ ������ �����. ������ ����� ����������� ����� ����������.
    m_inputControlsGrid->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 0 });
    m_inputControlsGrid->setPosition({ PANEL_PADDING, tgui::bindBottom(m_inputTitleLabel) + WIDGET_SPACING });

    unsigned int currentRow = 0;
    const float fixedLabelWidth = 100.f;
    const float gapBetweenWidgets = WIDGET_SPACING / 5.f;

    auto addInputRowToGrid =
        [&](const sf::String& sfLabelText, tgui::EditBox::Ptr& editBoxMember, const sf::String& sfToolTipText) {

        tgui::String tguiLabelText(sfLabelText);
        auto label = tgui::Label::create(tguiLabelText);
        if (!label) { /* ������ */ return; }
        label->getRenderer()->setTextColor(tgui::Color::Black);
        label->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);
        label->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left);
        label->setTextSize(16);
        label->setSize({ fixedLabelWidth, INPUT_ROW_HEIGHT });

        if (!sfToolTipText.isEmpty()) {
            tgui::String tguiToolTipText(sfToolTipText);
            auto toolTip = tgui::Label::create(tguiToolTipText);
            toolTip->getRenderer()->setBorders({ 1, 1, 1, 1 });
            toolTip->getRenderer()->setBackgroundColor(tgui::Color(0, 0, 0, 180));
            toolTip->getRenderer()->setTextColor(tgui::Color::White);
            toolTip->getRenderer()->setPadding({3, 2});
            toolTip->setTextSize(16);
            label->setToolTip(toolTip);
        }

        auto editBox = tgui::EditBox::create();
        if (!editBox) { /* ������ */ return; }
        editBox->setSize({ "50%", INPUT_ROW_HEIGHT });

        editBoxMember = editBox;

        m_inputControlsGrid->addWidget(label, currentRow, 0);
        m_inputControlsGrid->addWidget(editBoxMember, currentRow, 1);

        m_inputControlsGrid->setWidgetPadding(currentRow, 0, { 0, 2, gapBetweenWidgets, 2 });
        m_inputControlsGrid->setWidgetPadding(currentRow, 1, { 0, 2, 0, 2 });

        currentRow++;
    };

    addInputRowToGrid(L"m (��):", m_edit_m, L"����� ������������ ���� (��������) � �����������");
    addInputRowToGrid(L"M (x1e25 ��):", m_edit_M, L"��������� ��� ����� ������������ ����. �������� ����� = M * 1.0e25 ��");
    addInputRowToGrid(L"V0 (�/�):", m_edit_V0, L"��������� �������� �������� � �/� (���������� �� ��� Y)");
    addInputRowToGrid(L"T (���):", m_edit_T, L"����� ����� ��������� � ������");
    addInputRowToGrid(L"k (��������.):", m_edit_k, L"������������ ����������� ������������� �����");
    addInputRowToGrid(L"F (����):", m_edit_F, L"������������ ����������� ���� ����");

    // ������������� ��������� ������ �����
    if (currentRow > 0) {
        float rowEffectiveHeight = INPUT_ROW_HEIGHT + 2 + 2; // ������ ������� + �������_�������_������ + ������_�������_������
        float calculatedGridHeight = currentRow * rowEffectiveHeight;
        m_inputControlsGrid->setSize({ m_inputControlsGrid->getSizeLayout().x, calculatedGridHeight });
    }
    else {
        m_inputControlsGrid->setSize({ m_inputControlsGrid->getSizeLayout().x, 0 });
    }

    m_leftPanel->add(m_inputControlsGrid); // <<<--- ��������� ���� �� ������ �����

    // 3. ������ "���������� ����������!"
    m_calculateButton = tgui::Button::create(L"���������� ����������!");
    if (!m_calculateButton) { /*...*/ return; }
    m_calculateButton->getRenderer()->setRoundedBorderRadius(20);
    m_calculateButton->setTextSize(BUTTON_TEXT_SIZE);
    m_calculateButton->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 40 });
    m_calculateButton->setPosition({ PANEL_PADDING, tgui::bindBottom(m_inputControlsGrid) + WIDGET_SPACING * 1.5f });
    m_leftPanel->add(m_calculateButton);

    // 4. ������ "������� ������������"
    m_showVisualizerButton = tgui::Button::create(L"������� 2D ������������");
    if (!m_showVisualizerButton) { /*...*/ return; }
    m_showVisualizerButton->getRenderer()->setRoundedBorderRadius(20);
    m_showVisualizerButton->setTextSize(BUTTON_TEXT_SIZE);
    m_showVisualizerButton->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 40 });
    m_showVisualizerButton->setPosition({ PANEL_PADDING, tgui::bindBottom(m_calculateButton) + WIDGET_SPACING / 1.5f });
    m_leftPanel->add(m_showVisualizerButton);

    // 5. ������ "��������� �������� ������"
    m_loadTestDataButton = tgui::Button::create(L"��������� �������� ������");
    if (!m_loadTestDataButton) { /*...*/ return; }
    m_loadTestDataButton->getRenderer()->setRoundedBorderRadius(20);
    m_loadTestDataButton->setTextSize(BUTTON_TEXT_SIZE);
    m_loadTestDataButton->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 40 });
    m_loadTestDataButton->setPosition({ PANEL_PADDING, tgui::bindBottom(m_showVisualizerButton) + WIDGET_SPACING / 1.5f });
    m_leftPanel->add(m_loadTestDataButton);

    // 6. Label ��� ��������� �� ������� � �������
    m_errorMessagesLabel = tgui::Label::create();
    if (!m_errorMessagesLabel) { std::cerr << "Error: Failed to create m_errorMessagesLabel" << std::endl; return; }

    m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red); // ������� ��� ������
    m_errorMessagesLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_errorMessagesLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left); // ��� Center
    m_errorMessagesLabel->setVerticalAlignment(tgui::Label::VerticalAlignment::Top);
    m_errorMessagesLabel->setTextSize(14);
    m_errorMessagesLabel->setPosition({ PANEL_PADDING, tgui::bindBottom(m_loadTestDataButton) + WIDGET_SPACING });
    // ������: �� ��� ����� �� ���� ������ ����� ������
    m_errorMessagesLabel->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING),
                                   "100% - top - " + tgui::String::fromNumber(PANEL_PADDING) });
    m_leftPanel->add(m_errorMessagesLabel);
}

void UserInterface::loadRightPanelWidgets() {
    m_rightPanel = tgui::Panel::create();
    if (!m_rightPanel) { std::cerr << "Error: Failed to create m_rightPanel" << std::endl; return; }
    m_gui.add(m_rightPanel); // ������� ���������, ����� ����������� ����������

    loadTrajectoryWidgets(m_rightPanel);
    loadTableWidgets(m_rightPanel);
}

void UserInterface::loadTrajectoryWidgets(tgui::Panel::Ptr parentPanel) {
    m_trajectoryContainerPanel = tgui::Panel::create();
    if (!m_trajectoryContainerPanel) { std::cerr << "Error: Failed to create m_trajectoryContainerPanel" << std::endl; return; }
    m_trajectoryContainerPanel->getRenderer()->setBorders({ 1,1,1,1 });
    m_trajectoryContainerPanel->getRenderer()->setBorderColor(tgui::Color::Black);
    m_trajectoryContainerPanel->getRenderer()->setBackgroundColor(tgui::Color::White);
    parentPanel->add(m_trajectoryContainerPanel);

    m_trajectoryTitleLabel = tgui::Label::create(L"���������� �������� ����");
    if (!m_trajectoryTitleLabel) { std::cerr << "Error: Failed to create m_trajectoryTitleLabel" << std::endl; return; }
    m_trajectoryTitleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_trajectoryTitleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
    m_trajectoryTitleLabel->getRenderer()->setTextColor(tgui::Color::Black);
    m_trajectoryTitleLabel->setSize({ "100%", TITLE_HEIGHT });
    m_trajectoryContainerPanel->add(m_trajectoryTitleLabel, "TrajectoryTitle"); // ���������� ��� ��� ���������������� �������

    m_trajectoryCanvas = tgui::Canvas::create();
    if (!m_trajectoryCanvas) { std::cerr << "Error: Failed to create m_trajectoryCanvas" << std::endl; return; }
    m_trajectoryCanvas->setSize({ "100%", "100% - " + tgui::String::fromNumber(TITLE_HEIGHT) });
    m_trajectoryCanvas->setPosition({ 0, "TrajectoryTitle.bottom" });
    m_trajectoryContainerPanel->add(m_trajectoryCanvas);
}

void UserInterface::loadTableWidgets(tgui::Panel::Ptr parentPanel) {
    m_tableContainerPanel = tgui::Panel::create();
    if (!m_tableContainerPanel) { std::cerr << "Error: Failed to create m_tableContainerPanel" << std::endl; return; }
    m_tableContainerPanel->getRenderer()->setBorders({ 1,1,1,1 });
    m_tableContainerPanel->getRenderer()->setBorderColor(tgui::Color::Black);
    m_tableContainerPanel->getRenderer()->setBackgroundColor(tgui::Color::White);
    parentPanel->add(m_tableContainerPanel);

    m_tableTitleLabel = tgui::Label::create(L"������� ��������� � ���������");
    if (!m_tableTitleLabel) { std::cerr << "Error: Failed to create m_tableTitleLabel" << std::endl; return; }
    m_tableTitleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_tableTitleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
    m_tableTitleLabel->getRenderer()->setTextColor(tgui::Color::Black);
    m_tableTitleLabel->setSize({ "100%", TITLE_HEIGHT });
    m_tableContainerPanel->add(m_tableTitleLabel, "TableTitle");

    m_tableHeaderGrid = tgui::Grid::create();
    if (!m_tableHeaderGrid) { /* ... */ return; }
    m_tableHeaderGrid->setSize({ "100% - " + tgui::String::fromNumber(SCROLLBAR_WIDTH_ESTIMATE), HEADER_HEIGHT });
    m_tableHeaderGrid->setPosition({ 0, "TableTitle.bottom" });

    std::vector<sf::String> headers = { L"h, ���", L"x", L"y", L"Vx", L"Vy" };
    std::vector<tgui::String> columnHeaderWidths = { "20%", "20%", "20%", "20%", "20%" }; // ��� ���������� ���������� ��������
    unsigned int borderThickness = 1;

    for (size_t i = 0; i < headers.size(); ++i) {
        auto headerLabel = tgui::Label::create(tgui::String(headers[i]));
        if (!headerLabel) continue;
        headerLabel->getRenderer()->setTextColor(tgui::Color::Black);
        headerLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
        headerLabel->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);
        headerLabel->setSize({ columnHeaderWidths[i], "100%" }); // ���������� ������ ��� ���������

        unsigned int rightB = (i < headers.size() - 1) ? borderThickness : 0;
        headerLabel->getRenderer()->setBorders({ 0, 0, rightB, borderThickness });
        headerLabel->getRenderer()->setBorderColor(tgui::Color::Black);
        m_tableHeaderGrid->addWidget(headerLabel, 0, static_cast<unsigned int>(i));
    }
    m_tableContainerPanel->add(m_tableHeaderGrid);

    m_tableDataPanel = tgui::ScrollablePanel::create();
    if (!m_tableDataPanel) { std::cerr << "Error: Failed to create m_tableDataPanel" << std::endl; return; }
    m_tableDataPanel->setSize({ "100%", "100% - " + tgui::String::fromNumber(TITLE_HEIGHT + HEADER_HEIGHT) });
    m_tableDataPanel->setPosition({ 0, tgui::bindBottom(m_tableHeaderGrid) });
    m_tableDataPanel->getRenderer()->setBackgroundColor(tgui::Color(245, 245, 245));
    m_tableContainerPanel->add(m_tableDataPanel);

    m_tableDataGrid = tgui::Grid::create();
    if (!m_tableDataGrid) { std::cerr << "Error: Failed to create m_tableDataGrid for data" << std::endl; return; }

    m_tableDataGrid->setSize({ "100% - " + tgui::String::fromNumber(SCROLLBAR_WIDTH_ESTIMATE), 0 });
    m_tableDataPanel->add(m_tableDataGrid);
}

// --- ���������� ---
void UserInterface::setupLayout() {
    float menuBarHeight = 0; // ��������� � ��������������
    if (m_menuBar) {
        menuBarHeight = m_menuBar->getSize().y;
        if (menuBarHeight < 1.0f) { // ���� getSize().y ������ 0 (��������, ���������� ��� �� ������)
            menuBarHeight = 28; // ���������� ������ �� ���������, ������� �� ������ ��� MenuBar
            std::cout << "DEBUG: MenuBar height from getSize() was 0, using default: " << menuBarHeight << std::endl;
        }
    }
    
    // ����� ������
    if (m_leftPanel) {
        m_leftPanel->setSize({ "30%", "100% - " + tgui::String::fromNumber(menuBarHeight) });
        m_leftPanel->setPosition({ 0, menuBarHeight }); // ������� ���� �� ������ ����
    }

    // ������ ������
    if (m_rightPanel) {
        m_rightPanel->setSize({ "70%", "100% - " + tgui::String::fromNumber(menuBarHeight) });
        m_rightPanel->setPosition({ "30%", menuBarHeight }); // ������� ���� �� ������ ����
    }

    // ���������� ������ ������ ������
    const float rightPanelPadding = PANEL_PADDING;
    const float verticalSpacing = WIDGET_SPACING / 2.f;

    if (m_trajectoryContainerPanel && m_rightPanel) { // ��������� ��������
        m_trajectoryContainerPanel->setSize(
            { tgui::bindWidth(m_rightPanel) - 2 * rightPanelPadding,
             "60% - " + tgui::String::fromNumber(rightPanelPadding + verticalSpacing / 2.f) }
        );
        m_trajectoryContainerPanel->setPosition({ rightPanelPadding, rightPanelPadding });
    }

    if (m_tableContainerPanel && m_rightPanel && m_trajectoryContainerPanel) { // ��������� ��������
        m_tableContainerPanel->setSize(
            { tgui::bindWidth(m_rightPanel) - 2 * rightPanelPadding,
             "40% - " + tgui::String::fromNumber(rightPanelPadding + verticalSpacing / 2.f) }
        );
        m_tableContainerPanel->setPosition({ rightPanelPadding, tgui::bindBottom(m_trajectoryContainerPanel) + verticalSpacing });
    }
}

// --- ����������� �������� ---
void UserInterface::connectSignals() {
    if (m_calculateButton) {
        m_calculateButton->onPress.connect(&UserInterface::onCalculateButtonPressed, this);
    }
    else {
        std::cerr << "Error: m_calculateButton is null in connectSignals! Cannot connect." << std::endl;
    }
    
    // ������ ��� ������ 2D-�������������
    if (m_showVisualizerButton) {
        m_showVisualizerButton->onPress.connect(&UserInterface::onShowVisualizerButtonPressed, this);
    }
    else {
        std::cerr << "Error: m_showVisualizerButton is null in connectSignals! Cannot connect." << std::endl;
    }

    // ������ ��� ������ ������ ������ �� �����
    if (m_loadTestDataButton) {
        m_loadTestDataButton->onPress.connect(&UserInterface::onLoadTestDataButtonPressed, this);
    }
    else {
        std::cerr << "Error: m_loadTestDataButton is null in connectSignals!" << std::endl;
    }
}

// --- ����������� � ������ ---
void UserInterface::onCalculateButtonPressed() {
    std::cout << "Calculate button pressed! Initiating save-load-validate-calculate sequence." << std::endl;
    
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(""); // ������� ���������� ������/������
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"�������� ��������"); // ��������������� ���������

    // 1. ������� ������� �������� �� ����� EditBox
    std::string m_str = m_edit_m ? m_edit_m->getText().toStdString() : "0";
    std::string M_str = m_edit_M ? m_edit_M->getText().toStdString() : "0";
    std::string V0_str = m_edit_V0 ? m_edit_V0->getText().toStdString() : "0";
    std::string T_str = m_edit_T ? m_edit_T->getText().toStdString() : "0";
    std::string k_str = m_edit_k ? m_edit_k->getText().toStdString() : "0";
    std::string F_str = m_edit_F ? m_edit_F->getText().toStdString() : "0";

    // 2. ��������� ��� �������� � ����
    std::ofstream outFile(PARAMS_FILENAME);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open file '" << PARAMS_FILENAME << "' for saving parameters." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"������ ����. ����� ��������!");
        return;
    }
    outFile << "m_satellite_kg=" << m_str << std::endl;
    outFile << "M_central_body_factor=" << M_str << std::endl;
    outFile << "V0_m_per_s=" << V0_str << std::endl;
    outFile << "T_days=" << T_str << std::endl;
    outFile << "k_coeff=" << k_str << std::endl;
    outFile << "F_coeff=" << F_str << std::endl;
    outFile.close();
    if (outFile.fail()) {
        std::cerr << "Error: Failed to write or close parameter file '" << PARAMS_FILENAME << "' before calculation." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"������ ������ ����� ��������!");
        return;
    }
    std::cout << "Parameters from UI fields saved to '" << PARAMS_FILENAME << "'." << std::endl;

    // 3. ��������� �������� �� �����
    std::ifstream inFile(PARAMS_FILENAME);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not re-open file '" << PARAMS_FILENAME << "' for loading parameters." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"������ ������ �����!");
        return;
    }

    std::string line, key, value;
    // ���������� ��� �������� ����������� �� ����� ��������
    std::string loaded_m_str, loaded_M_str, loaded_V0_str, loaded_T_str, loaded_k_str, loaded_F_str;

    while (std::getline(inFile, line)) {
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            key = line.substr(0, delimiterPos);
            value = line.substr(delimiterPos + 1);

            if (key == "m_satellite_kg") loaded_m_str = value;
            else if (key == "M_central_body_factor") loaded_M_str = value;
            else if (key == "V0_m_per_s") loaded_V0_str = value;
            else if (key == "T_days") loaded_T_str = value;
            else if (key == "k_coeff") loaded_k_str = value;
            else if (key == "F_coeff") loaded_F_str = value;
        }
    }
    inFile.close();
    std::cout << "Parameters re-loaded from '" << PARAMS_FILENAME << "' for validation." << std::endl;

    // 4. ��������� ���� EditBox ������������ �� ����� ����������
    if (m_edit_m) m_edit_m->setText(loaded_m_str);
    if (m_edit_M) m_edit_M->setText(loaded_M_str);
    if (m_edit_V0) m_edit_V0->setText(loaded_V0_str);
    if (m_edit_T) m_edit_T->setText(loaded_T_str);
    if (m_edit_k) m_edit_k->setText(loaded_k_str);
    if (m_edit_F) m_edit_F->setText(loaded_F_str);

    // 5. �������� ��������� ����������� ����������
    InputParameters validatedParams = validateAndParseParameters(
        loaded_m_str, loaded_M_str, loaded_V0_str, loaded_T_str, loaded_k_str, loaded_F_str
    );

    if (!validatedParams.isValid) {
        std::wcerr << L"Error: Parameter validation failed after loading from file.\n" << validatedParams.errorMessage << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(tgui::String(validatedParams.errorMessage)); // validatedParams.errorMessage ��� std::wstring
        }
        m_trajectoryAvailable = false;
        m_calculatedStates.clear();
        prepareTrajectoryForDisplay();
        populateTable({});
        return;
    }
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0)); // �����-������� ��� ������
        m_errorMessagesLabel->setText(L"��������� ��������� �������.");
    }
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"��������� ���������");

    const double G_SI = 6.67430e-11;
    const double REFERENCE_PHYSICAL_LENGTH_FOR_X_1_5 = 1.495978707e11;
    const double SECONDS_PER_DAY = 24.0 * 60.0 * 60.0;

    SimulationParameters paramsForCalc; // ���������� �������� �� ��������� �� Calculations.h

    // ���������� ����������������� ��������
    double m_satellite_ui_kg_val = validatedParams.m_satellite_kg;
    double M_ui_val_from_editbox_val = validatedParams.M_central_body_factor;
    double V0_ui_si_val = validatedParams.V0_m_per_s;
    double T_total_ui_days_val = validatedParams.T_days;
    double k_val_input_val = validatedParams.k_coeff;
    double F_val_input_val = validatedParams.F_coeff;

    // 1. ��������� ����������� � M_central � ���������� ��������
    double M_central_body_physical_kg = M_ui_val_from_editbox_val * 1.0e25;
    double mass_unit_for_scaling = M_central_body_physical_kg;
    double length_unit = REFERENCE_PHYSICAL_LENGTH_FOR_X_1_5 / paramsForCalc.initialState.x;

    if (mass_unit_for_scaling <= 1e-9) {
        std::cerr << "Error: Scaling mass unit (M_central_body_physical_kg) must be significantly positive." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"����� �����. ���� > 0!");
        m_trajectoryAvailable = false; m_calculatedStates.clear();
        prepareTrajectoryForDisplay(); populateTable({});
        return;
    }
    double time_unit = std::sqrt(std::pow(length_unit, 3) / (G_SI * mass_unit_for_scaling));

    paramsForCalc.G = 1.0;
    paramsForCalc.M = (M_central_body_physical_kg + m_satellite_ui_kg_val) / mass_unit_for_scaling;

    paramsForCalc.DRAG_COEFFICIENT = k_val_input_val;
    paramsForCalc.THRUST_COEFFICIENT = F_val_input_val;

    double T_total_ui_sec = T_total_ui_days_val * SECONDS_PER_DAY;
    double T_total_dimensionless = T_total_ui_sec / time_unit;

    if (paramsForCalc.DT > 1e-9) {
        paramsForCalc.STEPS = static_cast<int>(T_total_dimensionless / paramsForCalc.DT);
        m_lastCalculationDT = paramsForCalc.DT; // <--- ��������� DT
    }
    else {
        paramsForCalc.STEPS = 1000;
        m_lastCalculationDT = paramsForCalc.DT;
        std::cerr << "Warning: DT is too small or zero. Using default STEPS." << std::endl;
    }
    if (paramsForCalc.STEPS <= 0) paramsForCalc.STEPS = 1;

    double characteristic_velocity = length_unit / time_unit;
    if (std::abs(characteristic_velocity) > 1e-9) {
        paramsForCalc.initialState.vy = V0_ui_si_val / characteristic_velocity;
    }
    else {
        paramsForCalc.initialState.vy = 0.0;
        std::cerr << "Warning: Characteristic velocity (length_unit/time_unit) is near zero. Setting vy_dimless to 0." << std::endl;
    }

    Calculations calculator;
    m_calculatedStates = calculator.runSimulation(paramsForCalc);

    m_currentTableData.clear();
    if (!m_calculatedStates.empty()) {
        m_trajectoryAvailable = true;
        const size_t maxTableEntries = 100;
        size_t step_size_for_table = 1;
        
        if (m_calculatedStates.size() > maxTableEntries) {
            step_size_for_table = m_calculatedStates.size() / maxTableEntries;
            if (step_size_for_table == 0) step_size_for_table = 1;
        }
        
        for (size_t i = 0; i < m_calculatedStates.size(); i += step_size_for_table) {
            const auto& state = m_calculatedStates[i];
            double current_dimensionless_time = i * paramsForCalc.DT;
            double current_physical_time_sec = current_dimensionless_time * time_unit;
            double current_physical_time_days = current_physical_time_sec / SECONDS_PER_DAY;
            m_currentTableData.push_back({
                static_cast<float>(current_physical_time_days),
                static_cast<float>(state.x), static_cast<float>(state.y),
                static_cast<float>(state.vx), static_cast<float>(state.vy)
            });
        }
    }
    else {
        m_trajectoryAvailable = false;
    }

    prepareTrajectoryForDisplay();
    populateTable(m_currentTableData);

    if (m_inputTitleLabel) { // ��������� �������� ��������� �� ���������� �������
        if (m_trajectoryAvailable) m_inputTitleLabel->setText(L"������ ��������");
        else m_inputTitleLabel->setText(L"������ �� ��� ����������");
    }
    // ��������� �� ������ � m_errorMessagesLabel
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0)); // �����-�������
        if (m_trajectoryAvailable) m_errorMessagesLabel->setText(L"������ ��������. ���������� ��������.");
        else m_errorMessagesLabel->setText(L"������ ��������, �� ���������� �� ��������� (��������, ������������ � ������).");
    }
}

void UserInterface::onSaveParamsAsMenuItemClicked() {
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");

    const tgui::String expectedFilename = L"���_���������.txt";

    auto dialog = tgui::FileDialog::create(L"��������� ��������� ���...", L"���������");
    dialog->setFileTypeFilters({ {L"��������� ����� (*.txt)", {L"*.txt"}}, {L"��� ����� (*.*)", {L"*.*"}} });
    dialog->getRenderer()->setTitleBarHeight(30);

    tgui::Filesystem::Path defaultSavePath(USER_SAVES_DIR);
    // ��������� � ������� ����������, ���� �� ���
    if (!tgui::Filesystem::directoryExists(defaultSavePath)) {
        if (!tgui::Filesystem::createDirectory(defaultSavePath)) {
            std::cerr << "Warning: Could not create directory for user saves: " << defaultSavePath.asString() << std::endl;
        }
    }
    if (tgui::Filesystem::directoryExists(defaultSavePath)) {
        dialog->setPath(defaultSavePath);
    }
    dialog->setFilename(expectedFilename); // ��� ����� �� ��������� � �������
    dialog->setPosition("(&.size - size) / 2"); // ���������� ����

    // ���� ������ ����������, ����� ������������ �������� "���������"
    dialog->onFileSelect.connect([this, expectedFilename](const std::vector<tgui::Filesystem::Path>& paths) {
        if (paths.empty()) {
            if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"���������� ���������� ��������.");
            return;
        }

        const tgui::Filesystem::Path& fsPath = paths[0]; // ����� ������ (� ������������) ����
        tgui::String selectedFilename = fsPath.getFilename();

        if (selectedFilename != expectedFilename) {
            std::cerr << "Error: Incorrect filename selected/entered. Expected: "
                << expectedFilename.toStdString() << ", Got: " << selectedFilename.toStdString() << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������: ����������, ���������� ����\n� ������ '" + expectedFilename + L"'.");
            }
            return;
        }


        tgui::String pathForDisplay = fsPath.asString(); // ��� ����������� � UI (���������� '/')
        auto nativePathForStream = fsPath.asString();

        std::string pathStringToOpen;
#ifdef TGUI_SYSTEM_WINDOWS
        // �� Windows, asNativeString() ���������� std::wstring. ���������� ��� �������� � ofstream.
        std::ofstream outFile(fsPath.asNativeString(), std::ios::binary); // std::ios::binary �� ������ ������
#else
        // �� ������ ��������, asNativeString() ���������� std::string (UTF-8).
        std::ofstream outFile(fsPath.asNativeString(), std::ios::binary);
#endif

        // std::ofstream outFile(pathStringToOpen);

        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open/create file via ofstream." << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������: �� ������� �������/������� ����\n'" + pathForDisplay + L"'. ��������� ����� ������� � ����.");
            }
            return;
        }

        std::string m_str = m_edit_m ? m_edit_m->getText().toStdString() : "0";
        std::string M_str = m_edit_M ? m_edit_M->getText().toStdString() : "0";
        std::string V0_str = m_edit_V0 ? m_edit_V0->getText().toStdString() : "0";
        std::string T_str = m_edit_T ? m_edit_T->getText().toStdString() : "0";
        std::string k_str = m_edit_k ? m_edit_k->getText().toStdString() : "0";
        std::string F_str = m_edit_F ? m_edit_F->getText().toStdString() : "0";

        outFile << "m_satellite_kg=" << m_str << std::endl;
        outFile << "M_central_body_factor=" << M_str << std::endl;
        outFile << "V0_m_per_s=" << V0_str << std::endl;
        outFile << "T_days=" << T_str << std::endl;
        outFile << "k_coeff=" << k_str << std::endl;
        outFile << "F_coeff=" << F_str << std::endl;
        outFile.close();

        if (outFile.fail()) {
            std::cerr << "Error: Failed to write or close parameter file '" << pathStringToOpen << "'." << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������ ������ � ����\n'" + pathForDisplay + L"'.");
            }
        }
        else {
            std::cout << "Parameters saved to '" << pathStringToOpen << "'" << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0));
                m_errorMessagesLabel->setText(L"��������� ��������� �\n'" + pathForDisplay + L"'.");
            }
        }
    });
    m_gui.add(dialog);
}

void UserInterface::onSaveTrajectoryDataAsMenuItemClicked() {
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");

    if (!m_trajectoryAvailable || m_calculatedStates.empty()) {
        std::cerr << "Save Trajectory Data: No trajectory data available to save." << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"��� ������ ���������� ��� ����������.\n������� ��������� ������.");
        }
        return;
    }

    const tgui::String expectedFilename = L"���_������_����������.txt"; // ��������� ��� �����

    auto dialog = tgui::FileDialog::create(L"��������� ������ ���������� ���...", L"���������");
    dialog->setFileTypeFilters({ {L"��������� ����� (*.txt)", {L"*.txt"}}, {L"CSV ����� (*.csv)", {L"*.csv"}}, {L"��� ����� (*.*)", {L"*.*"}} });
    dialog->getRenderer()->setTitleBarHeight(30);

    tgui::Filesystem::Path defaultSavePath(USER_SAVES_DIR);
    if (!tgui::Filesystem::directoryExists(defaultSavePath)) {
        if (!tgui::Filesystem::createDirectory(defaultSavePath)) {
            std::cerr << "Warning: Could not create directory for user saves: " << defaultSavePath.asString() << std::endl;
        }
    }
    if (tgui::Filesystem::directoryExists(defaultSavePath)) {
        dialog->setPath(defaultSavePath);
    }
    dialog->setFilename(expectedFilename);
    dialog->setPosition("(&.size - size) / 2"); // ���������� ����

    dialog->onFileSelect.connect([this, expectedFilename](const std::vector<tgui::Filesystem::Path>& paths) {
        if (paths.empty()) {
            std::cout << "Save Trajectory Data: Dialog closed without selection." << std::endl;
            if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"���������� ������ ��������.");
            return;
        }

        const tgui::Filesystem::Path& fsPath = paths[0];
        tgui::String selectedFilename = fsPath.getFilename();

        if (selectedFilename != expectedFilename) {
            std::cerr << "Error: Incorrect filename for trajectory. Expected: "
                << expectedFilename.toStdString() << ", Got: " << selectedFilename.toStdString() << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������: ����������, ���������� ����\n� ������ '" + expectedFilename + L"'.");
            }
            return;
        }

        auto nativePathForStream = fsPath.asNativeString();
        std::string pathStringToOpen;

#ifdef TGUI_SYSTEM_WINDOWS
        std::ofstream outFile(nativePathForStream, std::ios::binary);
#else
        std::ofstream outFile(nativePathForStream, std::ios::binary);
#endif

        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open/create trajectory file via ofstream." << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������: �� ������� �������/������� ���� ��� ����������\n'" + selectedFilename + L"'. ��������� ����� ������� � ����.");
            }
            return;
        }

        outFile << std::fixed << std::setprecision(5);
        outFile << "Step_Index, Time_dimless(approx), x_dimless, y_dimless, vx_dimless, vy_dimless\n";

        for (size_t i = 0; i < m_calculatedStates.size(); ++i) {
            const auto& state = m_calculatedStates[i];
            double dimensionless_time_approx = static_cast<double>(i) * m_lastCalculationDT;

            outFile << i << ",  "
                << dimensionless_time_approx << ",  "
                << state.x << ",  " << state.y << ",  "
                << state.vx << ",  " << state.vy << "\n";
        }
        outFile.close();

        if (outFile.fail()) {
            // ... (��������� ������ ������) ...
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������ ������ ������ ���������� �\n'" + selectedFilename + L"'.");
            }
        }
        else {
            // ... (��������� �� ������) ...
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0));
                m_errorMessagesLabel->setText(L"������ ���������� (" + tgui::String::fromNumber(m_calculatedStates.size())
                    + L" �����)\n��������� � '" + selectedFilename + L"'.");
            }
        }
    });
    m_gui.add(dialog);
}

#ifdef _WIN32
std::wstring getExecutablePath_Windows() {
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::wstring(path);
}
#endif

void UserInterface::onOpenDataFolderMenuItemClicked() {
    std::cout << "Menu: Open Data Folder clicked" << std::endl;
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");

#ifdef _WIN32
    std::wstring dataFolderPathNative;
    try {
        // �������� ���� � ����������, ��� ��������� ����������� ����
        std::filesystem::path exePath = getExecutablePath_Windows();
        std::filesystem::path dataPath = exePath.parent_path() / "data" / "user_data";

        // ��������, ��� ����� ����������, ��� ���������� �� �������
        if (!std::filesystem::exists(dataPath)) {
            if (!std::filesystem::create_directory(dataPath)) {
                std::cerr << "Error: Could not create data directory: " << dataPath.string() << std::endl;
                if (m_errorMessagesLabel) {
                    m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                    m_errorMessagesLabel->setText(L"������: �� ������� ������� ����� 'data'.");
                }
                return;
            }
        }
        dataFolderPathNative = dataPath.wstring(); // �������� std::wstring

        std::wcout << L"Attempting to open folder: " << dataFolderPathNative << std::endl;

        HINSTANCE result = ShellExecuteW(m_window.getSystemHandle(), L"explore", dataFolderPathNative.c_str(), NULL, NULL, SW_SHOWNORMAL);

        // ShellExecuteW ���������� �������� > 32 � ������ ������
        if ((intptr_t)result <= 32) {
            DWORD errorCode = GetLastError(); // ����� ������������ ��� ����� ��������� �����������
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"������: �� ������� ������� ����� � �������.\n���: " + tgui::String::fromNumber((intptr_t)result));
            }
        }
        else {
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0));
                m_errorMessagesLabel->setText(L"����� � ������� �������.");
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while trying to open data folder: " << e.what() << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"���������� ��� �������� �����.");
        }
    }
#else
    std::cerr << "Open Data Folder: Not implemented for this OS." << std::endl;
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
        m_errorMessagesLabel->setText(L"������� '������� �����' �� ����������� ��� ������ ��.");
    }
#endif
}

void UserInterface::onShowHelpMenuItemClicked() {

    auto helpWindow = tgui::ChildWindow::create();
    if (!helpWindow) {
        std::cerr << "Error: Failed to create Help ChildWindow" << std::endl;
        return;
    }
    helpWindow->setTitle(L"����������� ������������");
    helpWindow->setSize({ "60%", "70%" }); // ������ ������������ �������� (Gui)
    helpWindow->setPosition("(&.size - size) / 2"); // ���������� ����
    helpWindow->setResizable(true);
    helpWindow->getRenderer()->setTitleBarHeight(30);

    auto readmeArea = tgui::TextArea::create();
    if (!readmeArea) {
        helpWindow->destroy();
        return;
    }
    readmeArea->setReadOnly(true);
    readmeArea->setSize({ "100%", "100%" }); // ��������� ��� �������� ����
    readmeArea->getRenderer()->setBackgroundColor(tgui::Color(245, 245, 245));
    readmeArea->getRenderer()->setTextColor(tgui::Color::Black);
    readmeArea->setTextSize(16);
    readmeArea->setVerticalScrollbarPolicy(tgui::Scrollbar::Policy::Automatic);
    readmeArea->setHorizontalScrollbarPolicy(tgui::Scrollbar::Policy::Never);

    std::ifstream readmeFile(README_FILENAME);
    if (readmeFile.is_open()) {
        std::string content((std::istreambuf_iterator<char>(readmeFile)), std::istreambuf_iterator<char>());
        readmeFile.close();
        readmeArea->setText(tgui::String(content.c_str()));
        readmeArea->setVerticalScrollbarValue(0); // ����� � ������
    }
    else {
        readmeArea->setText(L"���� README.txt �� ������.");
    }

    helpWindow->add(readmeArea);

    m_gui.add(helpWindow); // ��������� �������� ���� � Gui
    helpWindow->setFocused(true); // ������ ��� ��������
}

void UserInterface::onShowAboutMenuItemClicked() {
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");

    if (m_gui.get<tgui::ChildWindow>("AboutProgramWindow")) {
        m_gui.get<tgui::ChildWindow>("AboutProgramWindow")->setFocused(true);
        return;
    }

    auto aboutWindow = tgui::ChildWindow::create();
    if (!aboutWindow) { /* ... ��������� ������ ... */ return; }

    aboutWindow->setWidgetName("AboutProgramWindow");
    aboutWindow->setTitle(L"� ���������: ������ ����������");
    aboutWindow->setSize({ 500, 580 });
    aboutWindow->setPosition("(&.size - size) / 2");
    aboutWindow->setResizable(false);
    aboutWindow->setTitleButtons(tgui::ChildWindow::TitleButton::Close);
    aboutWindow->getRenderer()->setTitleBarHeight(30);

    auto layout = tgui::VerticalLayout::create();
    layout->setSize({ "100%", "100%" });
    layout->getRenderer()->setPadding({ 15, 15, 15, 15 });
    aboutWindow->add(layout);

    // 1. �������� ���������
    auto titleLabel = tgui::Label::create(L"������ ���������� �������� ����");
    titleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    titleLabel->setTextSize(18);
    titleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center); // ���������� ����� ������ Label
    layout->add(titleLabel);
    layout->addSpace(0.2f);

    // 2. ������
    auto versionLabel = tgui::Label::create(L"������: 2.37 (�� 28.05.2025)");
    versionLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center); // ���������� ����� ������ Label
    layout->add(versionLabel);
    layout->addSpace(0.4f);

    // 3. ������
    auto authorLabel = tgui::Label::create(L"������: QiriQ, GGachivar");
    authorLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center); // ���������� ����� ������ Label
    layout->add(authorLabel);
    layout->addSpace(0.6f);

    // 4. �������� ��� �������������
    auto descriptionText = tgui::Label::create(
        L"��������� ��� ������������� � ������������\n"
        L"������������ �������� �������� ���.\n\n"
        L"������������ ����������:\n"
        L"  - SFML (Simple and Fast Multimedia Library)\n    (www.sfml-dev.org)\n"
        L"  - TGUI (Texus' Graphical User Interface)\n    (tgui.eu)"
    );
    // ��� �������������� Label, HorizontalAlignment::Left ������ �������� �����
    descriptionText->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left);
    layout->add(descriptionText);

    layout->setRatio(descriptionText, 3);

    layout->addSpace(0.6f);

    // 5. ������ "OK"
    auto okButton = tgui::Button::create(L"OK");
    okButton->setSize({ "35%", 35 }); 

    layout->add(okButton);


    okButton->onClick.connect([aboutWindow] {
        aboutWindow->close();
    });

    m_gui.add(aboutWindow);
    aboutWindow->setFocused(true);
}

void UserInterface::prepareTrajectoryForDisplay() {
    m_trajectoryDisplayPoints.clear();
    if (!m_trajectoryAvailable || m_calculatedStates.empty()) {
        return;
    }

    m_trajectoryDisplayPoints.reserve(m_calculatedStates.size());
    for (const auto& state : m_calculatedStates) {
        m_trajectoryDisplayPoints.emplace_back(
            sf::Vector2f(static_cast<float>(state.x), static_cast<float>(-state.y)), // Y ������������� ��� �����������
            sf::Color::Blue // ���� ����� ����������
        );
    }
}

void UserInterface::drawTrajectoryOnCanvas(sf::RenderTarget& canvasRenderTarget) {
    sf::View originalView = canvasRenderTarget.getView();
    sf::View fittedView;

    if (m_trajectoryAvailable && !m_trajectoryDisplayPoints.empty()) {

        // 1. ������������ �������������� ������������� (bounding box) ��� ����� �����������
        //    (����� ���������� + ������ ��������� (0,0) ��� ������������ ����)
        float min_x_content = m_trajectoryDisplayPoints[0].position.x;
        float max_x_content = m_trajectoryDisplayPoints[0].position.x;
        float min_y_content = m_trajectoryDisplayPoints[0].position.y; // ����� <= 0
        float max_y_content = m_trajectoryDisplayPoints[0].position.y; // ����� >= min_y_content, ����� ���� > 0 ���� ���������� ���������� y_sim=0

        for (const auto& vertex : m_trajectoryDisplayPoints) {
            min_x_content = std::min(min_x_content, vertex.position.x);
            max_x_content = std::max(max_x_content, vertex.position.x);
            min_y_content = std::min(min_y_content, vertex.position.y);
            max_y_content = std::max(max_y_content, vertex.position.y);
        }

        // ��������, ��� (0,0) �������� � bounding box
        min_x_content = std::min(min_x_content, 0.0f);
        max_x_content = std::max(max_x_content, 0.0f);
        min_y_content = std::min(min_y_content, 0.0f); // ���� ��� y < 0, �� max_y_content ������ 0.0f
        max_y_content = std::max(max_y_content, 0.0f); // ���� ��� y > 0 (�� ��� ������ � ���������), min_y_content ������ 0.0f

        // ������ � ������ ����������� ��� ��������
        float content_width_no_padding = max_x_content - min_x_content;
        float content_height_no_padding = max_y_content - min_y_content; // ������������� �����

        // 2. ��������� ������� (padding)
        float paddingFactor = 0.1f; // 10% ������

        // ������� ������� ��� ������� ����������� �������.
        // ���� ������� ����� ��������� (�����), ����� ����������� ���������� ������.
        const float MIN_DIM_FOR_PERCENT_PADDING = 0.1f; // ���� ������ ������ �����, ������ ����� �� ����� ��������
        float base_width_for_padding = std::max(content_width_no_padding, MIN_DIM_FOR_PERCENT_PADDING);
        float base_height_for_padding = std::max(content_height_no_padding, MIN_DIM_FOR_PERCENT_PADDING);

        float padding_x = base_width_for_padding * paddingFactor;
        float padding_y = base_height_for_padding * paddingFactor;

        // ���������� ���������������� ��������������
        float padded_min_x = min_x_content - padding_x;
        float padded_max_x = max_x_content + padding_x;
        float padded_min_y = min_y_content - padding_y; // ������ ��� ����� �������������
        float padded_max_y = max_y_content + padding_y; // ������ ��� ����� ������������� (��� ������ �� ����, ���� max_y_content ��� <0)

        // ����������� ������� ���������������� ��������
        float actual_padded_content_width = padded_max_x - padded_min_x;
        float actual_padded_content_height = padded_max_y - padded_min_y;

        // 3. ���������� "�����������" ������� �������� ��� ������� View.
        //    ��� �����, ����� �������� ������� �� ���� ��� ������� ��������� �������� View.
        const float MIN_EFFECTIVE_VIEW_DIMENSION = 0.02f; // ����������� ������ ������� View � ������� ����������� (���� ������ ������� ����)
        float effective_view_content_width = std::max(actual_padded_content_width, MIN_EFFECTIVE_VIEW_DIMENSION);
        float effective_view_content_height = std::max(actual_padded_content_height, MIN_EFFECTIVE_VIEW_DIMENSION);

        // 4. ������������ ������� sf::View, ����� �� �������������� ����������� ������ �������
        //    � ������ effective_view_content_width/height.
        sf::Vector2u canvasSize = canvasRenderTarget.getSize();
        if (canvasSize.x == 0 || canvasSize.y == 0) {
            canvasRenderTarget.setView(originalView); // ������ ������� �������, ������ �� ������
            return;
        }
        float canvasAspectRatio = static_cast<float>(canvasSize.x) / canvasSize.y;
        float effectiveContentAspectRatio = effective_view_content_width / effective_view_content_height;

        float view_width_world;  // �������� ������ View � ������� �����������
        float view_height_world; // �������� ������ View � ������� �����������

        if (canvasAspectRatio > effectiveContentAspectRatio) {
            view_height_world = effective_view_content_height;
            view_width_world = view_height_world * canvasAspectRatio;
        }
        else {
            view_width_world = effective_view_content_width;
            view_height_world = view_width_world / canvasAspectRatio;
        }
        fittedView.setSize(view_width_world, view_height_world);

        // 5. ���������� sf::View �� ������ *������������* ���������������� ��������.
        //    ��� �������� ������. ����� ������ ���� �� actual_padded_*, � �� effective_*.
        sf::Vector2f actual_padded_content_center(
            padded_min_x + actual_padded_content_width / 2.0f,
            padded_min_y + actual_padded_content_height / 2.0f
        );
        fittedView.setCenter(actual_padded_content_center);

        canvasRenderTarget.setView(fittedView);

        // --- ��������� ---
        const float actual_central_body_radius = 0.01f; // ���������� ������
        sf::CircleShape centerBody(actual_central_body_radius);
        centerBody.setFillColor(sf::Color::Red);
        centerBody.setOrigin(actual_central_body_radius, actual_central_body_radius);
        centerBody.setPosition(0.f, 0.f);
        canvasRenderTarget.draw(centerBody);

        // m_trajectoryDisplayPoints ��� �������� �� !empty() � ������
        canvasRenderTarget.draw(m_trajectoryDisplayPoints.data(), m_trajectoryDisplayPoints.size(), sf::LineStrip);
    }
    else {
        canvasRenderTarget.setView(canvasRenderTarget.getDefaultView());
        sf::Text placeholderText;
        if (m_sfmlFont.hasGlyph(L'�')) { // ���������, ���������� �� �����
            placeholderText.setFont(m_sfmlFont);
            placeholderText.setString(L"���������� �� ����������.\n������� '���������� ����������!'");
        }
        else {
            placeholderText.setString("Trajectory not calculated.\nPress 'Calculate Trajectory!' (Font error)"); // �������
        }
        
        placeholderText.setCharacterSize(16);
        placeholderText.setFillColor(sf::Color(105, 105, 105));
        sf::FloatRect textRect = placeholderText.getLocalBounds();
        placeholderText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        placeholderText.setPosition(static_cast<float>(canvasRenderTarget.getSize().x) / 2.0f,
            static_cast<float>(canvasRenderTarget.getSize().y) / 2.0f);
        canvasRenderTarget.draw(placeholderText);
    }
    canvasRenderTarget.setView(originalView); // ��������������� �������� View
}

void UserInterface::onShowVisualizerButtonPressed() {
    std::cout << "Show Visualizer button pressed!" << std::endl;

    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"�������� ��������");

    if (!m_trajectoryAvailable || m_calculatedStates.empty()) {
        std::cerr << "UserInterface: No trajectory data to visualize. Please calculate first." << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"������� ����������� ����������!");
        }
        return;
    }

    // ����������� m_calculatedStates (std::vector<State>) 
    // � WorldTrajectoryData (std::vector<std::pair<double, double>>)
    WorldTrajectoryData trajectoryForVisualizer;
    trajectoryForVisualizer.reserve(m_calculatedStates.size());
    for (const auto& state : m_calculatedStates) {
        trajectoryForVisualizer.emplace_back(state.x, state.y);
    }

    if (trajectoryForVisualizer.empty()) {
        std::cerr << "UserInterface: Conversion to WorldTrajectoryData resulted in empty data." << std::endl;
        return;
    }

    std::cout << "UserInterface: Launching TrajectoryVisualizer with "
        << trajectoryForVisualizer.size() << " points." << std::endl;

    try {
        TrajectoryVisualizer visualizer(1000, 800);
        visualizer.setData(trajectoryForVisualizer);
        visualizer.run(); // ���� ����� ��������� ���������� �����, ���� ���� visualizer �� ���������
    }
    catch (const std::exception& e) {
        std::cerr << "UserInterface: Exception while running TrajectoryVisualizer: " << e.what() << std::endl;
        // ��������� ������, ���� �������� ��� ������ TrajectoryVisualizer �� �������
    }
    std::cout << "UserInterface: TrajectoryVisualizer window closed." << std::endl;
    // ����� �������� ���� ������������� ����� ������� ����� ��� �������� ���-�� � �������� UI, ���� �����.
    if (m_errorMessagesLabel && m_errorMessagesLabel->getText() == L"������� ����������� ����������!") {
        m_errorMessagesLabel->setText(L""); // ��������, ���� ��� ������ ������
    }
}

void UserInterface::onLoadTestDataButtonPressed() {
    std::cout << "Load Test Data button pressed!" << std::endl;
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L""); // ������� ���������� ���������
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"�������� ��������");

    std::ifstream inFile(TEST_DATA_FILENAME); // ���������� ��������� ��� ����� �����
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open test data file '" << TEST_DATA_FILENAME << "'." << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"������: ���� �������� ������\n'" + tgui::String(TEST_DATA_FILENAME) + L"' �� ������!");
        }
        return;
    }

    std::string line, key, value;
    std::string loaded_m_str, loaded_M_str, loaded_V0_str, loaded_T_str, loaded_k_str, loaded_F_str;

    // ��������� ��� ��������� �� �����
    while (std::getline(inFile, line)) {
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            key = line.substr(0, delimiterPos);
            value = line.substr(delimiterPos + 1);

            if (key == "m_satellite_kg") loaded_m_str = value;
            else if (key == "M_central_body_factor") loaded_M_str = value;
            else if (key == "V0_m_per_s") loaded_V0_str = value;
            else if (key == "T_days") loaded_T_str = value;
            else if (key == "k_coeff") loaded_k_str = value;
            else if (key == "F_coeff") loaded_F_str = value;
        }
    }
    inFile.close();

    // ��������� ���� EditBox ������������ ����������
    // ���������, ��� ���� �� ������, ����� �������� ��������� ������ ������, ���� �������� ������������ � �����
    if (m_edit_m && !loaded_m_str.empty()) m_edit_m->setText(loaded_m_str);
    if (m_edit_M && !loaded_M_str.empty()) m_edit_M->setText(loaded_M_str);
    if (m_edit_V0 && !loaded_V0_str.empty()) m_edit_V0->setText(loaded_V0_str);
    if (m_edit_T && !loaded_T_str.empty()) m_edit_T->setText(loaded_T_str);
    if (m_edit_k && !loaded_k_str.empty()) m_edit_k->setText(loaded_k_str);
    if (m_edit_F && !loaded_F_str.empty()) m_edit_F->setText(loaded_F_str);

    std::cout << "Test data loaded from '" << TEST_DATA_FILENAME << "' into UI fields." << std::endl;
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0)); // �������
        m_errorMessagesLabel->setText(L"�������� ������ ���������.\n������� '���������� ����������!'.");
    }
}

void UserInterface::populateTable(const std::vector<TableRowData>& data) {
    if (!m_tableDataGrid || !m_tableDataPanel) { return; }
    m_tableDataGrid->removeAllWidgets();
    
    const float CELL_ROW_HEIGHT = 30.f;
    std::vector<float> columnWidthPercentages = { 0.20f, 0.20f, 0.20f, 0.20f, 0.20f };
    
    if (data.empty()) {
        auto emptyLabel = tgui::Label::create(L"��� ������ ��� �����������");
        if (emptyLabel) {
            emptyLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
            emptyLabel->setSize({ "100%", CELL_ROW_HEIGHT }); // ������ ��� ������ �����, ������ ������
            m_tableDataGrid->addWidget(emptyLabel, 0, 0);
        }
        if (m_tableDataPanel) {
            m_tableDataPanel->setContentSize(m_tableDataGrid->getSize());
        }
        return;
    }

    size_t step = 1;
    unsigned int max_rows_to_display_limit = 100;
    if (data.size() > max_rows_to_display_limit) { 
        step = data.size() / max_rows_to_display_limit;
    }
    if (step == 0) step = 1;

    unsigned int grid_row_index = 0;
    unsigned int borderThicknessData = 1;
    
    float panelWidth = m_tableDataPanel->getSize().x;
    float actualDataGridWidth = panelWidth - SCROLLBAR_WIDTH_ESTIMATE;

    if (actualDataGridWidth < 1.0f) {
        actualDataGridWidth = 400; // ������� ���������� �������
        std::cout << "CRITICAL WARNING: Fallback actualDataGridWidth: " << actualDataGridWidth << std::endl;
    }

    for (size_t i = 0; i < data.size(); i += step) {
        const auto& rowData = data[i];
        std::vector<tgui::String> rowStrings;

        std::stringstream ss_h, ss_x, ss_y, ss_vx, ss_vy;
        ss_h << std::fixed << std::setprecision(2) << data[i].h_days; rowStrings.push_back(tgui::String(ss_h.str()));
        ss_x << std::fixed << std::setprecision(2) << data[i].x; rowStrings.push_back(tgui::String(ss_x.str()));
        ss_y << std::fixed << std::setprecision(2) << data[i].y; rowStrings.push_back(tgui::String(ss_y.str()));
        ss_vx << std::fixed << std::setprecision(2) << data[i].Vx; rowStrings.push_back(tgui::String(ss_vx.str()));
        ss_vy << std::fixed << std::setprecision(2) << data[i].Vy; rowStrings.push_back(tgui::String(ss_vy.str()));

        for (size_t j = 0; j < rowStrings.size(); ++j) {
            auto cellLabel = tgui::Label::create(rowStrings[j]);
            if (cellLabel) {
                cellLabel->getRenderer()->setTextColor(tgui::Color::Black);
                cellLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
                cellLabel->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);

                float cellWidth = actualDataGridWidth * columnWidthPercentages[j];
                cellLabel->setSize({ std::max(1.0f, cellWidth), CELL_ROW_HEIGHT });

                if (grid_row_index < 1 && i < 1 * step) { // ������� ������ ��� ������ ������ ������
                    std::cout << "  Cell (" << grid_row_index << "," << j << ") text: '" << rowStrings[j].toStdString()
                        << "' wants cellWidth: " << cellWidth << " actual labelSize: " << cellLabel->getSize().x << "x" << cellLabel->getSize().y << std::endl;
                }

                unsigned int rightB_data = (j < rowStrings.size() - 1) ? borderThicknessData : 0;
                cellLabel->getRenderer()->setBorders({ 0, 0, rightB_data, 0 });
                cellLabel->getRenderer()->setBorderColor(tgui::Color(200, 200, 200));
                m_tableDataGrid->addWidget(cellLabel, grid_row_index, static_cast<unsigned int>(j));
            }
        }
        grid_row_index++;
    }

    if (m_tableDataPanel && m_tableDataGrid) {
        float totalGridHeight = grid_row_index * CELL_ROW_HEIGHT;
        if (data.empty() && m_tableDataGrid->getWidgets().size() > 0) totalGridHeight = CELL_ROW_HEIGHT;
        else if (data.empty()) totalGridHeight = 0;

        m_tableDataGrid->setSize({ actualDataGridWidth, totalGridHeight });
        m_tableDataPanel->setContentSize({ actualDataGridWidth, totalGridHeight });
    }
}

InputParameters UserInterface::validateAndParseParameters(
    const std::string& m_str, const std::string& M_str, const std::string& V0_str,
    const std::string& T_str, const std::string& k_str, const std::string& F_str) {

    InputParameters params;
    std::wstringstream errorMessages;

    try {
        params.m_satellite_kg = std::stod(m_str); 
        if (params.m_satellite_kg < 0.1 || params.m_satellite_kg > 100000.0) {
            params.isValid = false;
            errorMessages << L"����� �������� (m) ��� ��������� [0.1, 100.000] ��.\n"; 
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"�������� ������ ����� �������� (m).\n";
    }

    try {
        params.M_central_body_factor = std::stod(M_str);
        if (params.M_central_body_factor < 0.1 || params.M_central_body_factor > 1.0e5) {
            params.isValid = false;
            errorMessages << L"����� �����. ���� (M) ��� ��������� [0,1, 100.000].\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"�������� ������ ����� �����. ���� (M).\n";
    }

    try {
        params.V0_m_per_s = std::stod(V0_str);
        if (params.V0_m_per_s < 0.0 || params.V0_m_per_s > 1000) { 
            params.isValid = false;
            errorMessages << L"��������� �������� (V0) ��� ��������� [0, 1000] �/�.\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"�������� ������ ��������� �������� (V0).\n";
    }

    try {
        params.T_days = std::stod(T_str);
        if (params.T_days < 1 || params.T_days > 1.0e7) {
            params.isValid = false;
            errorMessages << L"����� ��������� (T) ��� ��������� [1, 10.000.000] ���.\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"�������� ������ ������� ��������� (T).\n";
    }

    try {
        params.k_coeff = std::stod(k_str);
        if (params.k_coeff < 0.0 || params.k_coeff > 2.0) {
            params.isValid = false;
            errorMessages << L"����������� k ��� ��������� [0.0, 2.0].\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"�������� ������ ������������ k.\n";
    }

    try {
        params.F_coeff = std::stod(F_str);
        if (params.F_coeff < 0.0 || params.F_coeff > 2.0) {
            params.isValid = false;
            errorMessages << L"����������� F ��� ��������� [0.0, 2.0].\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"�������� ������ ������������ F.\n";
    }

    params.errorMessage = errorMessages.str();
    return params;
}

// --- ������� ���� � ��������� ������� ---
void UserInterface::run() {
    m_window.setFramerateLimit(60); // ����������� FPS ��� ��������� � �������� ��������
    while (m_window.isOpen()) {
        handleEvents();
        update();
        render();
    }
}

void UserInterface::handleEvents() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        m_gui.handleEvent(event);

        if (event.type == sf::Event::Closed) {
            m_window.close();
        }
        else if (event.type == sf::Event::Resized) {
            sf::FloatRect visibleArea(0.f, 0.f, static_cast<float>(event.size.width), static_cast<float>(event.size.height));
            m_window.setView(sf::View(visibleArea));

            // �������������� ������� � ������ ���������
            // �����: m_currentTableData ������ ���� ����������
            if (m_trajectoryAvailable || !m_currentTableData.empty() || m_tableDataGrid->getWidgets().size() > 0) {
                populateTable(m_currentTableData);
            }
        }

        
    }
}

void UserInterface::update() {
    // ������ ������ ������ � ReadME
}

void UserInterface::render() {
    if (m_trajectoryCanvas) {
        sf::RenderTexture& canvasRT = m_trajectoryCanvas->getRenderTexture();
        sf::Vector2f canvasWidgetSize = m_trajectoryCanvas->getSize(); // ������ ������� TGUI

        if (canvasRT.getSize().x != static_cast<unsigned int>(canvasWidgetSize.x) ||
            canvasRT.getSize().y != static_cast<unsigned int>(canvasWidgetSize.y)) {

            if (canvasWidgetSize.x > 0 && canvasWidgetSize.y > 0) {
                if (!canvasRT.create(static_cast<unsigned int>(canvasWidgetSize.x), static_cast<unsigned int>(canvasWidgetSize.y))) {
                    std::cerr << "ERROR: Failed to recreate Canvas RenderTexture!" << std::endl;
                }
            }
        }

        canvasRT.clear(sf::Color(250, 250, 250));   // ��� �������
        drawTrajectoryOnCanvas(canvasRT);           // ���� ����� ������ ��� ������������� � ���������� View
        m_trajectoryCanvas->display();
    }
    m_window.clear(sf::Color(220, 220, 220));
    m_gui.draw();
    m_window.display();
}