#ifdef _MSC_VER // Только для компилятора MSVC
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h> // Для ShellExecute
#include <filesystem> // Для C++17 std::filesystem (опционально, для построения пути)
#endif

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif

#include "../include/TrajectoryVisualizer.h"
#include "../include/UserInterface.h"

#include <iostream>     // Для отладки
#include <algorithm>    // Для std::min_element, std::max_element
#include <cmath>        // для std::pow, std::sqrt
#include <fstream>      // Для std::ofstream и std::ifstream
#include <sstream>      // Для std::stringstream
#include <string>       // Для std::string, std::stod, substr, find
#include <locale>       // Для std::locale, std::codecvt
#include <codecvt>      // Для std::wstring_convert

// --- Вспомогательная функция для создания строки ввода ---
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

// --- Конструктор и инициализация ---
UserInterface::UserInterface()
    : m_window({ 1200, 800 }, L"Расчет траектории движения тела"),
    m_gui(m_window),
    m_trajectoryAvailable(false) {

    m_gui.setFont("assets/fonts/arial.ttf");

    // Загрузка шрифта для SFML (используется на Canvas)
    if (!m_sfmlFont.loadFromFile("assets/fonts/arial.ttf")) {
        std::cerr << "SFML: Error - Failed to load font 'arial.ttf' for SFML rendering!\n";
    }

    initializeGui();
}

void UserInterface::initializeGui() {
    loadWidgets();
    setupLayout(); // Вызываем setupLayout после loadWidgets
    connectSignals();
    populateTable({}); // Начальное пустое состояние таблицы
}

void UserInterface::loadMenuBar() {
    m_menuBar = tgui::MenuBar::create();
    if (!m_menuBar) {
        std::cerr << "Error: Failed to create MenuBar" << std::endl;
        return;
    }
    m_menuBar->getRenderer()->setBackgroundColor(tgui::Color(210, 210, 210)); // Цвет фона меню
    m_menuBar->getRenderer()->setTextColor(tgui::Color::Black);
    m_menuBar->getRenderer()->setSelectedBackgroundColor(tgui::Color(0, 110, 220));
    m_menuBar->getRenderer()->setSelectedTextColor(tgui::Color::White);
    m_menuBar->setHeight(28); // Высота панели меню

    // --- Меню "Файл" ---
    m_menuBar->addMenu(L"Файл");
    m_menuBar->addMenuItem(L"Файл", L"Сохранить параметры как...");
    m_menuBar->addMenuItem(L"Файл", L"Сохранить данные траектории как...");
    m_menuBar->addMenuItem(L"Файл", L"Открыть папку с данными");
    m_menuBar->addMenuItem(L"Файл", L"Выход");

    // --- Меню "Справка" ---
    m_menuBar->addMenu(L"Справка");
    m_menuBar->addMenuItem(L"Справка", L"Руководство пользователя");
    m_menuBar->addMenuItem(L"Справка", L"О программе"); 

    // Подключаем сигналы для пунктов меню "Файл"
    m_menuBar->onMenuItemClick.connect([this](const std::vector<tgui::String>& menuItemHierarchy) {
        if (menuItemHierarchy.empty()) return;

        const tgui::String& menuName = menuItemHierarchy[0];
        tgui::String itemName = (menuItemHierarchy.size() > 1) ? menuItemHierarchy[1] : "";

        if (menuName == L"Файл") {
            if (itemName == L"Сохранить параметры как...") {
                onSaveParamsAsMenuItemClicked();
            }
            else if (itemName == L"Сохранить данные траектории как...") {
                onSaveTrajectoryDataAsMenuItemClicked();
            }
            else if (itemName == L"Открыть папку с данными") {
                onOpenDataFolderMenuItemClicked();
            }
            else if (itemName == L"Выход") {
                m_window.close();
            }
        }
        else if (menuName == L"Справка") {
            if (itemName == L"Руководство пользователя") {
                onShowHelpMenuItemClicked();
            }
            else if (itemName == L"О программе") {
                onShowAboutMenuItemClicked(); // Вызовем метод, который пока не реализован
            }
        }

    });
    m_gui.add(m_menuBar); // Добавляем MenuBar напрямую в Gui
}

// --- Загрузка виджетов ---
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

    // 1. Заголовок "Исходные значения"
    m_inputTitleLabel = tgui::Label::create(L"Исходные значения");
    if (!m_inputTitleLabel) { std::cerr << "Error: Failed to create m_inputTitleLabel" << std::endl; return; }
    m_inputTitleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_inputTitleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
    m_inputTitleLabel->getRenderer()->setTextColor(tgui::Color::Black);
    m_inputTitleLabel->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), TITLE_HEIGHT });
    m_inputTitleLabel->setPosition({ PANEL_PADDING, PANEL_PADDING });
    m_leftPanel->add(m_inputTitleLabel);

    // 2. Grid для полей ввода
    m_inputControlsGrid = tgui::Grid::create();
    if (!m_inputControlsGrid) { std::cerr << "Error: Failed to create m_inputControlsGrid" << std::endl; return; }
    // Задаем ширину грида. Высота будет установлена после заполнения.
    m_inputControlsGrid->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 0 });
    m_inputControlsGrid->setPosition({ PANEL_PADDING, tgui::bindBottom(m_inputTitleLabel) + WIDGET_SPACING });

    unsigned int currentRow = 0;
    const float fixedLabelWidth = 100.f;
    const float gapBetweenWidgets = WIDGET_SPACING / 5.f;

    auto addInputRowToGrid =
        [&](const sf::String& sfLabelText, tgui::EditBox::Ptr& editBoxMember, const sf::String& sfToolTipText) {

        tgui::String tguiLabelText(sfLabelText);
        auto label = tgui::Label::create(tguiLabelText);
        if (!label) { /* ошибка */ return; }
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
        if (!editBox) { /* ошибка */ return; }
        editBox->setSize({ "50%", INPUT_ROW_HEIGHT });

        editBoxMember = editBox;

        m_inputControlsGrid->addWidget(label, currentRow, 0);
        m_inputControlsGrid->addWidget(editBoxMember, currentRow, 1);

        m_inputControlsGrid->setWidgetPadding(currentRow, 0, { 0, 2, gapBetweenWidgets, 2 });
        m_inputControlsGrid->setWidgetPadding(currentRow, 1, { 0, 2, 0, 2 });

        currentRow++;
    };

    addInputRowToGrid(L"m (кг):", m_edit_m, L"Масса вращающегося тела (спутника) в килограммах");
    addInputRowToGrid(L"M (x1e25 кг):", m_edit_M, L"Множитель для массы центрального тела. Итоговая масса = M * 1.0e25 кг");
    addInputRowToGrid(L"V0 (м/с):", m_edit_V0, L"Начальная скорость спутника в м/с (направлена по оси Y)");
    addInputRowToGrid(L"T (сут):", m_edit_T, L"Общее время симуляции в сутках");
    addInputRowToGrid(L"k (сопротив.):", m_edit_k, L"Безразмерный коэффициент сопротивления среды");
    addInputRowToGrid(L"F (тяга):", m_edit_F, L"Безразмерный коэффициент силы тяги");

    // Устанавливаем финальную высоту грида
    if (currentRow > 0) {
        float rowEffectiveHeight = INPUT_ROW_HEIGHT + 2 + 2; // Высота виджета + верхний_паддинг_ячейки + нижний_паддинг_ячейки
        float calculatedGridHeight = currentRow * rowEffectiveHeight;
        m_inputControlsGrid->setSize({ m_inputControlsGrid->getSizeLayout().x, calculatedGridHeight });
    }
    else {
        m_inputControlsGrid->setSize({ m_inputControlsGrid->getSizeLayout().x, 0 });
    }

    m_leftPanel->add(m_inputControlsGrid); // <<<--- ДОБАВЛЯЕМ ГРИД НА ПАНЕЛЬ ЗДЕСЬ

    // 3. Кнопка "Рассчитать траекторию!"
    m_calculateButton = tgui::Button::create(L"Рассчитать траекторию!");
    if (!m_calculateButton) { /*...*/ return; }
    m_calculateButton->getRenderer()->setRoundedBorderRadius(20);
    m_calculateButton->setTextSize(BUTTON_TEXT_SIZE);
    m_calculateButton->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 40 });
    m_calculateButton->setPosition({ PANEL_PADDING, tgui::bindBottom(m_inputControlsGrid) + WIDGET_SPACING * 1.5f });
    m_leftPanel->add(m_calculateButton);

    // 4. Кнопка "Открыть визуализатор"
    m_showVisualizerButton = tgui::Button::create(L"Открыть 2D визуализатор");
    if (!m_showVisualizerButton) { /*...*/ return; }
    m_showVisualizerButton->getRenderer()->setRoundedBorderRadius(20);
    m_showVisualizerButton->setTextSize(BUTTON_TEXT_SIZE);
    m_showVisualizerButton->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 40 });
    m_showVisualizerButton->setPosition({ PANEL_PADDING, tgui::bindBottom(m_calculateButton) + WIDGET_SPACING / 1.5f });
    m_leftPanel->add(m_showVisualizerButton);

    // 5. Кнопка "Загрузить тестовые данные"
    m_loadTestDataButton = tgui::Button::create(L"Загрузить тестовые данные");
    if (!m_loadTestDataButton) { /*...*/ return; }
    m_loadTestDataButton->getRenderer()->setRoundedBorderRadius(20);
    m_loadTestDataButton->setTextSize(BUTTON_TEXT_SIZE);
    m_loadTestDataButton->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING), 40 });
    m_loadTestDataButton->setPosition({ PANEL_PADDING, tgui::bindBottom(m_showVisualizerButton) + WIDGET_SPACING / 1.5f });
    m_leftPanel->add(m_loadTestDataButton);

    // 6. Label для сообщений об ошибках и статуса
    m_errorMessagesLabel = tgui::Label::create();
    if (!m_errorMessagesLabel) { std::cerr << "Error: Failed to create m_errorMessagesLabel" << std::endl; return; }

    m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red); // Красный для ошибок
    m_errorMessagesLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_errorMessagesLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left); // или Center
    m_errorMessagesLabel->setVerticalAlignment(tgui::Label::VerticalAlignment::Top);
    m_errorMessagesLabel->setTextSize(14);
    m_errorMessagesLabel->setPosition({ PANEL_PADDING, tgui::bindBottom(m_loadTestDataButton) + WIDGET_SPACING });
    // Высота: от его верха до низа панели минус отступ
    m_errorMessagesLabel->setSize({ "100% - " + tgui::String::fromNumber(2 * PANEL_PADDING),
                                   "100% - top - " + tgui::String::fromNumber(PANEL_PADDING) });
    m_leftPanel->add(m_errorMessagesLabel);
}

void UserInterface::loadRightPanelWidgets() {
    m_rightPanel = tgui::Panel::create();
    if (!m_rightPanel) { std::cerr << "Error: Failed to create m_rightPanel" << std::endl; return; }
    m_gui.add(m_rightPanel); // Сначала добавляем, потом настраиваем содержимое

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

    m_trajectoryTitleLabel = tgui::Label::create(L"Траектория движения тела");
    if (!m_trajectoryTitleLabel) { std::cerr << "Error: Failed to create m_trajectoryTitleLabel" << std::endl; return; }
    m_trajectoryTitleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    m_trajectoryTitleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
    m_trajectoryTitleLabel->getRenderer()->setTextColor(tgui::Color::Black);
    m_trajectoryTitleLabel->setSize({ "100%", TITLE_HEIGHT });
    m_trajectoryContainerPanel->add(m_trajectoryTitleLabel, "TrajectoryTitle"); // Используем имя для позиционирования канваса

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

    m_tableTitleLabel = tgui::Label::create(L"Таблица координат и скоростей");
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

    std::vector<sf::String> headers = { L"h, сут", L"x", L"y", L"Vx", L"Vy" };
    std::vector<tgui::String> columnHeaderWidths = { "20%", "20%", "20%", "20%", "20%" }; // Для заголовков используем проценты
    unsigned int borderThickness = 1;

    for (size_t i = 0; i < headers.size(); ++i) {
        auto headerLabel = tgui::Label::create(tgui::String(headers[i]));
        if (!headerLabel) continue;
        headerLabel->getRenderer()->setTextColor(tgui::Color::Black);
        headerLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
        headerLabel->setVerticalAlignment(tgui::Label::VerticalAlignment::Center);
        headerLabel->setSize({ columnHeaderWidths[i], "100%" }); // Процентная ширина для заголовка

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

// --- Компоновка ---
void UserInterface::setupLayout() {
    float menuBarHeight = 0; // Объявляем и инициализируем
    if (m_menuBar) {
        menuBarHeight = m_menuBar->getSize().y;
        if (menuBarHeight < 1.0f) { // Если getSize().y вернул 0 (например, компоновка еще не прошла)
            menuBarHeight = 28; // Используем высоту по умолчанию, которую мы задали для MenuBar
            std::cout << "DEBUG: MenuBar height from getSize() was 0, using default: " << menuBarHeight << std::endl;
        }
    }
    
    // Левая панель
    if (m_leftPanel) {
        m_leftPanel->setSize({ "30%", "100% - " + tgui::String::fromNumber(menuBarHeight) });
        m_leftPanel->setPosition({ 0, menuBarHeight }); // Смещаем вниз на высоту меню
    }

    // Правая панель
    if (m_rightPanel) {
        m_rightPanel->setSize({ "70%", "100% - " + tgui::String::fromNumber(menuBarHeight) });
        m_rightPanel->setPosition({ "30%", menuBarHeight }); // Смещаем вниз на высоту меню
    }

    // Контейнеры внутри правой панели
    const float rightPanelPadding = PANEL_PADDING;
    const float verticalSpacing = WIDGET_SPACING / 2.f;

    if (m_trajectoryContainerPanel && m_rightPanel) { // Добавляем проверки
        m_trajectoryContainerPanel->setSize(
            { tgui::bindWidth(m_rightPanel) - 2 * rightPanelPadding,
             "60% - " + tgui::String::fromNumber(rightPanelPadding + verticalSpacing / 2.f) }
        );
        m_trajectoryContainerPanel->setPosition({ rightPanelPadding, rightPanelPadding });
    }

    if (m_tableContainerPanel && m_rightPanel && m_trajectoryContainerPanel) { // Добавляем проверки
        m_tableContainerPanel->setSize(
            { tgui::bindWidth(m_rightPanel) - 2 * rightPanelPadding,
             "40% - " + tgui::String::fromNumber(rightPanelPadding + verticalSpacing / 2.f) }
        );
        m_tableContainerPanel->setPosition({ rightPanelPadding, tgui::bindBottom(m_trajectoryContainerPanel) + verticalSpacing });
    }
}

// --- Подключение сигналов ---
void UserInterface::connectSignals() {
    if (m_calculateButton) {
        m_calculateButton->onPress.connect(&UserInterface::onCalculateButtonPressed, this);
    }
    else {
        std::cerr << "Error: m_calculateButton is null in connectSignals! Cannot connect." << std::endl;
    }
    
    // Сигнал для кнопки 2D-визуализатора
    if (m_showVisualizerButton) {
        m_showVisualizerButton->onPress.connect(&UserInterface::onShowVisualizerButtonPressed, this);
    }
    else {
        std::cerr << "Error: m_showVisualizerButton is null in connectSignals! Cannot connect." << std::endl;
    }

    // Сигнал для кнопки чтения данных из файла
    if (m_loadTestDataButton) {
        m_loadTestDataButton->onPress.connect(&UserInterface::onLoadTestDataButtonPressed, this);
    }
    else {
        std::cerr << "Error: m_loadTestDataButton is null in connectSignals!" << std::endl;
    }
}

// --- Обработчики и логика ---
void UserInterface::onCalculateButtonPressed() {
    std::cout << "Calculate button pressed! Initiating save-load-validate-calculate sequence." << std::endl;
    
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(""); // Очищаем предыдущие ошибки/статус
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Исходные значения"); // Восстанавливаем заголовок

    // 1. Собрать текущие значения из полей EditBox
    std::string m_str = m_edit_m ? m_edit_m->getText().toStdString() : "0";
    std::string M_str = m_edit_M ? m_edit_M->getText().toStdString() : "0";
    std::string V0_str = m_edit_V0 ? m_edit_V0->getText().toStdString() : "0";
    std::string T_str = m_edit_T ? m_edit_T->getText().toStdString() : "0";
    std::string k_str = m_edit_k ? m_edit_k->getText().toStdString() : "0";
    std::string F_str = m_edit_F ? m_edit_F->getText().toStdString() : "0";

    // 2. Сохранить эти значения в файл
    std::ofstream outFile(PARAMS_FILENAME);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open file '" << PARAMS_FILENAME << "' for saving parameters." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Ошибка сохр. перед расчетом!");
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
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Ошибка записи перед расчетом!");
        return;
    }
    std::cout << "Parameters from UI fields saved to '" << PARAMS_FILENAME << "'." << std::endl;

    // 3. Прочитать значения из файла
    std::ifstream inFile(PARAMS_FILENAME);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not re-open file '" << PARAMS_FILENAME << "' for loading parameters." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Ошибка чтения файла!");
        return;
    }

    std::string line, key, value;
    // Переменные для хранения прочитанных из файла значений
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

    // 4. Заполнить поля EditBox прочитанными из файла значениями
    if (m_edit_m) m_edit_m->setText(loaded_m_str);
    if (m_edit_M) m_edit_M->setText(loaded_M_str);
    if (m_edit_V0) m_edit_V0->setText(loaded_V0_str);
    if (m_edit_T) m_edit_T->setText(loaded_T_str);
    if (m_edit_k) m_edit_k->setText(loaded_k_str);
    if (m_edit_F) m_edit_F->setText(loaded_F_str);

    // 5. Провести валидацию прочитанных параметров
    InputParameters validatedParams = validateAndParseParameters(
        loaded_m_str, loaded_M_str, loaded_V0_str, loaded_T_str, loaded_k_str, loaded_F_str
    );

    if (!validatedParams.isValid) {
        std::wcerr << L"Error: Parameter validation failed after loading from file.\n" << validatedParams.errorMessage << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(tgui::String(validatedParams.errorMessage)); // validatedParams.errorMessage это std::wstring
        }
        m_trajectoryAvailable = false;
        m_calculatedStates.clear();
        prepareTrajectoryForDisplay();
        populateTable({});
        return;
    }
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0)); // Темно-зеленый для успеха
        m_errorMessagesLabel->setText(L"Параметры проверены успешно.");
    }
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Параметры проверены");

    const double G_SI = 6.67430e-11;
    const double REFERENCE_PHYSICAL_LENGTH_FOR_X_1_5 = 1.495978707e11;
    const double SECONDS_PER_DAY = 24.0 * 60.0 * 60.0;

    SimulationParameters paramsForCalc; // Используем значения по умолчанию из Calculations.h

    // Используем провалидированные значения
    double m_satellite_ui_kg_val = validatedParams.m_satellite_kg;
    double M_ui_val_from_editbox_val = validatedParams.M_central_body_factor;
    double V0_ui_si_val = validatedParams.V0_m_per_s;
    double T_total_ui_days_val = validatedParams.T_days;
    double k_val_input_val = validatedParams.k_coeff;
    double F_val_input_val = validatedParams.F_coeff;

    // 1. Применяем модификатор к M_central и определяем масштабы
    double M_central_body_physical_kg = M_ui_val_from_editbox_val * 1.0e25;
    double mass_unit_for_scaling = M_central_body_physical_kg;
    double length_unit = REFERENCE_PHYSICAL_LENGTH_FOR_X_1_5 / paramsForCalc.initialState.x;

    if (mass_unit_for_scaling <= 1e-9) {
        std::cerr << "Error: Scaling mass unit (M_central_body_physical_kg) must be significantly positive." << std::endl;
        if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Масса центр. тела > 0!");
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
        m_lastCalculationDT = paramsForCalc.DT; // <--- СОХРАНЯЕМ DT
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

    if (m_inputTitleLabel) { // Обновляем основной заголовок по результату расчета
        if (m_trajectoryAvailable) m_inputTitleLabel->setText(L"Расчет выполнен");
        else m_inputTitleLabel->setText(L"Расчет не дал траектории");
    }
    // Сообщение об успехе в m_errorMessagesLabel
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0)); // Темно-зеленый
        if (m_trajectoryAvailable) m_errorMessagesLabel->setText(L"Расчет выполнен. Траектория доступна.");
        else m_errorMessagesLabel->setText(L"Расчет завершен, но траектория не построена (возможно, столкновение в начале).");
    }
}

void UserInterface::onSaveParamsAsMenuItemClicked() {
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");

    const tgui::String expectedFilename = L"мои_параметры.txt";

    auto dialog = tgui::FileDialog::create(L"Сохранить параметры как...", L"Сохранить");
    dialog->setFileTypeFilters({ {L"Текстовые файлы (*.txt)", {L"*.txt"}}, {L"Все файлы (*.*)", {L"*.*"}} });
    dialog->getRenderer()->setTitleBarHeight(30);

    tgui::Filesystem::Path defaultSavePath(USER_SAVES_DIR);
    // Проверяем и создаем директорию, если ее нет
    if (!tgui::Filesystem::directoryExists(defaultSavePath)) {
        if (!tgui::Filesystem::createDirectory(defaultSavePath)) {
            std::cerr << "Warning: Could not create directory for user saves: " << defaultSavePath.asString() << std::endl;
        }
    }
    if (tgui::Filesystem::directoryExists(defaultSavePath)) {
        dialog->setPath(defaultSavePath);
    }
    dialog->setFilename(expectedFilename); // Имя файла по умолчанию в диалоге
    dialog->setPosition("(&.size - size) / 2"); // Центрируем окно

    // Этот сигнал вызывается, когда пользователь нажимает "Сохранить"
    dialog->onFileSelect.connect([this, expectedFilename](const std::vector<tgui::Filesystem::Path>& paths) {
        if (paths.empty()) {
            if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"Сохранение параметров отменено.");
            return;
        }

        const tgui::Filesystem::Path& fsPath = paths[0]; // Берем первый (и единственный) путь
        tgui::String selectedFilename = fsPath.getFilename();

        if (selectedFilename != expectedFilename) {
            std::cerr << "Error: Incorrect filename selected/entered. Expected: "
                << expectedFilename.toStdString() << ", Got: " << selectedFilename.toStdString() << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"Ошибка: Пожалуйста, сохраняйте файл\nс именем '" + expectedFilename + L"'.");
            }
            return;
        }


        tgui::String pathForDisplay = fsPath.asString(); // Для отображения в UI (использует '/')
        auto nativePathForStream = fsPath.asString();

        std::string pathStringToOpen;
#ifdef TGUI_SYSTEM_WINDOWS
        // На Windows, asNativeString() возвращает std::wstring. Используем его напрямую с ofstream.
        std::ofstream outFile(fsPath.asNativeString(), std::ios::binary); // std::ios::binary на всякий случай
#else
        // На других системах, asNativeString() возвращает std::string (UTF-8).
        std::ofstream outFile(fsPath.asNativeString(), std::ios::binary);
#endif

        // std::ofstream outFile(pathStringToOpen);

        if (!outFile.is_open()) {
            std::cerr << "Error: Could not open/create file via ofstream." << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"Ошибка: Не удалось создать/открыть файл\n'" + pathForDisplay + L"'. Проверьте права доступа и путь.");
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
                m_errorMessagesLabel->setText(L"Ошибка записи в файл\n'" + pathForDisplay + L"'.");
            }
        }
        else {
            std::cout << "Parameters saved to '" << pathStringToOpen << "'" << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0));
                m_errorMessagesLabel->setText(L"Параметры сохранены в\n'" + pathForDisplay + L"'.");
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
            m_errorMessagesLabel->setText(L"Нет данных траектории для сохранения.\nСначала выполните расчет.");
        }
        return;
    }

    const tgui::String expectedFilename = L"мои_данные_траектории.txt"; // Ожидаемое имя файла

    auto dialog = tgui::FileDialog::create(L"Сохранить данные траектории как...", L"Сохранить");
    dialog->setFileTypeFilters({ {L"Текстовые файлы (*.txt)", {L"*.txt"}}, {L"CSV файлы (*.csv)", {L"*.csv"}}, {L"Все файлы (*.*)", {L"*.*"}} });
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
    dialog->setPosition("(&.size - size) / 2"); // Центрируем окно

    dialog->onFileSelect.connect([this, expectedFilename](const std::vector<tgui::Filesystem::Path>& paths) {
        if (paths.empty()) {
            std::cout << "Save Trajectory Data: Dialog closed without selection." << std::endl;
            if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"Сохранение данных отменено.");
            return;
        }

        const tgui::Filesystem::Path& fsPath = paths[0];
        tgui::String selectedFilename = fsPath.getFilename();

        if (selectedFilename != expectedFilename) {
            std::cerr << "Error: Incorrect filename for trajectory. Expected: "
                << expectedFilename.toStdString() << ", Got: " << selectedFilename.toStdString() << std::endl;
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"Ошибка: Пожалуйста, сохраняйте файл\nс именем '" + expectedFilename + L"'.");
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
                m_errorMessagesLabel->setText(L"Ошибка: Не удалось создать/открыть файл для траектории\n'" + selectedFilename + L"'. Проверьте права доступа и путь.");
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
            // ... (обработка ошибки записи) ...
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"Ошибка записи данных траектории в\n'" + selectedFilename + L"'.");
            }
        }
        else {
            // ... (сообщение об успехе) ...
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0));
                m_errorMessagesLabel->setText(L"Данные траектории (" + tgui::String::fromNumber(m_calculatedStates.size())
                    + L" точек)\nсохранены в '" + selectedFilename + L"'.");
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
        // Получаем путь к директории, где находится исполняемый файл
        std::filesystem::path exePath = getExecutablePath_Windows();
        std::filesystem::path dataPath = exePath.parent_path() / "data" / "user_data";

        // Убедимся, что папка существует, или попытаемся ее создать
        if (!std::filesystem::exists(dataPath)) {
            if (!std::filesystem::create_directory(dataPath)) {
                std::cerr << "Error: Could not create data directory: " << dataPath.string() << std::endl;
                if (m_errorMessagesLabel) {
                    m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                    m_errorMessagesLabel->setText(L"Ошибка: Не удалось создать папку 'data'.");
                }
                return;
            }
        }
        dataFolderPathNative = dataPath.wstring(); // Получаем std::wstring

        std::wcout << L"Attempting to open folder: " << dataFolderPathNative << std::endl;

        HINSTANCE result = ShellExecuteW(m_window.getSystemHandle(), L"explore", dataFolderPathNative.c_str(), NULL, NULL, SW_SHOWNORMAL);

        // ShellExecuteW возвращает значение > 32 в случае успеха
        if ((intptr_t)result <= 32) {
            DWORD errorCode = GetLastError(); // Можно использовать для более детальной диагностики
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
                m_errorMessagesLabel->setText(L"Ошибка: Не удалось открыть папку с данными.\nКод: " + tgui::String::fromNumber((intptr_t)result));
            }
        }
        else {
            if (m_errorMessagesLabel) {
                m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0));
                m_errorMessagesLabel->setText(L"Папка с данными открыта.");
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while trying to open data folder: " << e.what() << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"Исключение при открытии папки.");
        }
    }
#else
    std::cerr << "Open Data Folder: Not implemented for this OS." << std::endl;
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
        m_errorMessagesLabel->setText(L"Функция 'Открыть папку' не реализована для данной ОС.");
    }
#endif
}

void UserInterface::onShowHelpMenuItemClicked() {

    auto helpWindow = tgui::ChildWindow::create();
    if (!helpWindow) {
        std::cerr << "Error: Failed to create Help ChildWindow" << std::endl;
        return;
    }
    helpWindow->setTitle(L"Руководство пользователя");
    helpWindow->setSize({ "60%", "70%" }); // Размер относительно родителя (Gui)
    helpWindow->setPosition("(&.size - size) / 2"); // Центрируем окно
    helpWindow->setResizable(true);
    helpWindow->getRenderer()->setTitleBarHeight(30);

    auto readmeArea = tgui::TextArea::create();
    if (!readmeArea) {
        helpWindow->destroy();
        return;
    }
    readmeArea->setReadOnly(true);
    readmeArea->setSize({ "100%", "100%" }); // Заполнить все дочернее окно
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
        readmeArea->setVerticalScrollbarValue(0); // Сразу в начало
    }
    else {
        readmeArea->setText(L"Файл README.txt не найден.");
    }

    helpWindow->add(readmeArea);

    m_gui.add(helpWindow); // Добавляем дочернее окно в Gui
    helpWindow->setFocused(true); // Делаем его активным
}

void UserInterface::onShowAboutMenuItemClicked() {
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");

    if (m_gui.get<tgui::ChildWindow>("AboutProgramWindow")) {
        m_gui.get<tgui::ChildWindow>("AboutProgramWindow")->setFocused(true);
        return;
    }

    auto aboutWindow = tgui::ChildWindow::create();
    if (!aboutWindow) { /* ... обработка ошибки ... */ return; }

    aboutWindow->setWidgetName("AboutProgramWindow");
    aboutWindow->setTitle(L"О программе: Расчет Траектории");
    aboutWindow->setSize({ 500, 580 });
    aboutWindow->setPosition("(&.size - size) / 2");
    aboutWindow->setResizable(false);
    aboutWindow->setTitleButtons(tgui::ChildWindow::TitleButton::Close);
    aboutWindow->getRenderer()->setTitleBarHeight(30);

    auto layout = tgui::VerticalLayout::create();
    layout->setSize({ "100%", "100%" });
    layout->getRenderer()->setPadding({ 15, 15, 15, 15 });
    aboutWindow->add(layout);

    // 1. Название программы
    auto titleLabel = tgui::Label::create(L"Расчет Траектории Движения Тела");
    titleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    titleLabel->setTextSize(18);
    titleLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center); // Центрирует текст ВНУТРИ Label
    layout->add(titleLabel);
    layout->addSpace(0.2f);

    // 2. Версия
    auto versionLabel = tgui::Label::create(L"Версия: 2.37 (от 28.05.2025)");
    versionLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center); // Центрирует текст ВНУТРИ Label
    layout->add(versionLabel);
    layout->addSpace(0.4f);

    // 3. Авторы
    auto authorLabel = tgui::Label::create(L"Авторы: QiriQ, GGachivar");
    authorLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center); // Центрирует текст ВНУТРИ Label
    layout->add(authorLabel);
    layout->addSpace(0.6f);

    // 4. Описание или благодарности
    auto descriptionText = tgui::Label::create(
        L"Программа для моделирования и визуализации\n"
        L"орбитального движения небесных тел.\n\n"
        L"Используемые библиотеки:\n"
        L"  - SFML (Simple and Fast Multimedia Library)\n    (www.sfml-dev.org)\n"
        L"  - TGUI (Texus' Graphical User Interface)\n    (tgui.eu)"
    );
    // Для многострочного Label, HorizontalAlignment::Left обычно выглядит лучше
    descriptionText->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Left);
    layout->add(descriptionText);

    layout->setRatio(descriptionText, 3);

    layout->addSpace(0.6f);

    // 5. Кнопка "OK"
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
            sf::Vector2f(static_cast<float>(state.x), static_cast<float>(-state.y)), // Y инвертируется для отображения
            sf::Color::Blue // Цвет линии траектории
        );
    }
}

void UserInterface::drawTrajectoryOnCanvas(sf::RenderTarget& canvasRenderTarget) {
    sf::View originalView = canvasRenderTarget.getView();
    sf::View fittedView;

    if (m_trajectoryAvailable && !m_trajectoryDisplayPoints.empty()) {

        // 1. Рассчитываем ограничивающий прямоугольник (bounding box) для всего содержимого
        //    (точки траектории + начало координат (0,0) для центрального тела)
        float min_x_content = m_trajectoryDisplayPoints[0].position.x;
        float max_x_content = m_trajectoryDisplayPoints[0].position.x;
        float min_y_content = m_trajectoryDisplayPoints[0].position.y; // Будет <= 0
        float max_y_content = m_trajectoryDisplayPoints[0].position.y; // Будет >= min_y_content, может быть > 0 если траектория пересекает y_sim=0

        for (const auto& vertex : m_trajectoryDisplayPoints) {
            min_x_content = std::min(min_x_content, vertex.position.x);
            max_x_content = std::max(max_x_content, vertex.position.x);
            min_y_content = std::min(min_y_content, vertex.position.y);
            max_y_content = std::max(max_y_content, vertex.position.y);
        }

        // Убедимся, что (0,0) включено в bounding box
        min_x_content = std::min(min_x_content, 0.0f);
        max_x_content = std::max(max_x_content, 0.0f);
        min_y_content = std::min(min_y_content, 0.0f); // Если все y < 0, то max_y_content станет 0.0f
        max_y_content = std::max(max_y_content, 0.0f); // Если все y > 0 (не наш случай с инверсией), min_y_content станет 0.0f

        // Ширина и высота содержимого без отступов
        float content_width_no_padding = max_x_content - min_x_content;
        float content_height_no_padding = max_y_content - min_y_content; // Положительное число

        // 2. Добавляем отступы (padding)
        float paddingFactor = 0.1f; // 10% отступ

        // Базовые размеры для расчета процентного отступа.
        // Если контент очень маленький (точка), нужен минимальный абсолютный отступ.
        const float MIN_DIM_FOR_PERCENT_PADDING = 0.1f; // Если размер меньше этого, отступ будет от этого значения
        float base_width_for_padding = std::max(content_width_no_padding, MIN_DIM_FOR_PERCENT_PADDING);
        float base_height_for_padding = std::max(content_height_no_padding, MIN_DIM_FOR_PERCENT_PADDING);

        float padding_x = base_width_for_padding * paddingFactor;
        float padding_y = base_height_for_padding * paddingFactor;

        // Координаты отпадингованного прямоугольника
        float padded_min_x = min_x_content - padding_x;
        float padded_max_x = max_x_content + padding_x;
        float padded_min_y = min_y_content - padding_y; // Станет еще более отрицательным
        float padded_max_y = max_y_content + padding_y; // Станет еще более положительным (или дальше от нуля, если max_y_content был <0)

        // Фактические размеры отпадингованного контента
        float actual_padded_content_width = padded_max_x - padded_min_x;
        float actual_padded_content_height = padded_max_y - padded_min_y;

        // 3. Определяем "эффективные" размеры контента для расчета View.
        //    Это нужно, чтобы избежать деления на ноль или слишком маленьких размеров View.
        const float MIN_EFFECTIVE_VIEW_DIMENSION = 0.02f; // Минимальный размер стороны View в мировых координатах (чуть больше радиуса тела)
        float effective_view_content_width = std::max(actual_padded_content_width, MIN_EFFECTIVE_VIEW_DIMENSION);
        float effective_view_content_height = std::max(actual_padded_content_height, MIN_EFFECTIVE_VIEW_DIMENSION);

        // 4. Рассчитываем размеры sf::View, чтобы он соответствовал соотношению сторон канваса
        //    и вмещал effective_view_content_width/height.
        sf::Vector2u canvasSize = canvasRenderTarget.getSize();
        if (canvasSize.x == 0 || canvasSize.y == 0) {
            canvasRenderTarget.setView(originalView); // Размер канваса нулевой, ничего не рисуем
            return;
        }
        float canvasAspectRatio = static_cast<float>(canvasSize.x) / canvasSize.y;
        float effectiveContentAspectRatio = effective_view_content_width / effective_view_content_height;

        float view_width_world;  // Конечная ширина View в мировых координатах
        float view_height_world; // Конечная высота View в мировых координатах

        if (canvasAspectRatio > effectiveContentAspectRatio) {
            view_height_world = effective_view_content_height;
            view_width_world = view_height_world * canvasAspectRatio;
        }
        else {
            view_width_world = effective_view_content_width;
            view_height_world = view_width_world / canvasAspectRatio;
        }
        fittedView.setSize(view_width_world, view_height_world);

        // 5. Центрируем sf::View на центре *фактического* отпадингованного контента.
        //    Это ключевой момент. Центр должен быть от actual_padded_*, а не effective_*.
        sf::Vector2f actual_padded_content_center(
            padded_min_x + actual_padded_content_width / 2.0f,
            padded_min_y + actual_padded_content_height / 2.0f
        );
        fittedView.setCenter(actual_padded_content_center);

        canvasRenderTarget.setView(fittedView);

        // --- Отрисовка ---
        const float actual_central_body_radius = 0.01f; // Физический радиус
        sf::CircleShape centerBody(actual_central_body_radius);
        centerBody.setFillColor(sf::Color::Red);
        centerBody.setOrigin(actual_central_body_radius, actual_central_body_radius);
        centerBody.setPosition(0.f, 0.f);
        canvasRenderTarget.draw(centerBody);

        // m_trajectoryDisplayPoints уже проверен на !empty() в начале
        canvasRenderTarget.draw(m_trajectoryDisplayPoints.data(), m_trajectoryDisplayPoints.size(), sf::LineStrip);
    }
    else {
        canvasRenderTarget.setView(canvasRenderTarget.getDefaultView());
        sf::Text placeholderText;
        if (m_sfmlFont.hasGlyph(L'Т')) { // Проверяем, загрузился ли шрифт
            placeholderText.setFont(m_sfmlFont);
            placeholderText.setString(L"Траектория не рассчитана.\nНажмите 'Рассчитать траекторию!'");
        }
        else {
            placeholderText.setString("Trajectory not calculated.\nPress 'Calculate Trajectory!' (Font error)"); // Фоллбэк
        }
        
        placeholderText.setCharacterSize(16);
        placeholderText.setFillColor(sf::Color(105, 105, 105));
        sf::FloatRect textRect = placeholderText.getLocalBounds();
        placeholderText.setOrigin(textRect.left + textRect.width / 2.0f, textRect.top + textRect.height / 2.0f);
        placeholderText.setPosition(static_cast<float>(canvasRenderTarget.getSize().x) / 2.0f,
            static_cast<float>(canvasRenderTarget.getSize().y) / 2.0f);
        canvasRenderTarget.draw(placeholderText);
    }
    canvasRenderTarget.setView(originalView); // Восстанавливаем исходный View
}

void UserInterface::onShowVisualizerButtonPressed() {
    std::cout << "Show Visualizer button pressed!" << std::endl;

    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L"");
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Исходные значения");

    if (!m_trajectoryAvailable || m_calculatedStates.empty()) {
        std::cerr << "UserInterface: No trajectory data to visualize. Please calculate first." << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"Сначала рассчитайте траекторию!");
        }
        return;
    }

    // Преобразуем m_calculatedStates (std::vector<State>) 
    // в WorldTrajectoryData (std::vector<std::pair<double, double>>)
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
        visualizer.run(); // Этот вызов блокирует выполнение здесь, пока окно visualizer не закроется
    }
    catch (const std::exception& e) {
        std::cerr << "UserInterface: Exception while running TrajectoryVisualizer: " << e.what() << std::endl;
        // Обработка ошибки, если создание или запуск TrajectoryVisualizer не удались
    }
    std::cout << "UserInterface: TrajectoryVisualizer window closed." << std::endl;
    // После закрытия окна визуализатора можно вернуть фокус или обновить что-то в основном UI, если нужно.
    if (m_errorMessagesLabel && m_errorMessagesLabel->getText() == L"Сначала рассчитайте траекторию!") {
        m_errorMessagesLabel->setText(L""); // Очистить, если все прошло хорошо
    }
}

void UserInterface::onLoadTestDataButtonPressed() {
    std::cout << "Load Test Data button pressed!" << std::endl;
    if (m_errorMessagesLabel) m_errorMessagesLabel->setText(L""); // Очищаем предыдущие сообщения
    if (m_inputTitleLabel) m_inputTitleLabel->setText(L"Исходные значения");

    std::ifstream inFile(TEST_DATA_FILENAME); // Используем константу для имени файла
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open test data file '" << TEST_DATA_FILENAME << "'." << std::endl;
        if (m_errorMessagesLabel) {
            m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color::Red);
            m_errorMessagesLabel->setText(L"Ошибка: Файл тестовых данных\n'" + tgui::String(TEST_DATA_FILENAME) + L"' не найден!");
        }
        return;
    }

    std::string line, key, value;
    std::string loaded_m_str, loaded_M_str, loaded_V0_str, loaded_T_str, loaded_k_str, loaded_F_str;

    // Считываем все параметры из файла
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

    // Заполняем поля EditBox прочитанными значениями
    // Проверяем, что поля не пустые, чтобы избежать установки пустой строки, если параметр отсутствовал в файле
    if (m_edit_m && !loaded_m_str.empty()) m_edit_m->setText(loaded_m_str);
    if (m_edit_M && !loaded_M_str.empty()) m_edit_M->setText(loaded_M_str);
    if (m_edit_V0 && !loaded_V0_str.empty()) m_edit_V0->setText(loaded_V0_str);
    if (m_edit_T && !loaded_T_str.empty()) m_edit_T->setText(loaded_T_str);
    if (m_edit_k && !loaded_k_str.empty()) m_edit_k->setText(loaded_k_str);
    if (m_edit_F && !loaded_F_str.empty()) m_edit_F->setText(loaded_F_str);

    std::cout << "Test data loaded from '" << TEST_DATA_FILENAME << "' into UI fields." << std::endl;
    if (m_errorMessagesLabel) {
        m_errorMessagesLabel->getRenderer()->setTextColor(tgui::Color(0, 128, 0)); // Зеленый
        m_errorMessagesLabel->setText(L"Тестовые данные загружены.\nНажмите 'Рассчитать траекторию!'.");
    }
}

void UserInterface::populateTable(const std::vector<TableRowData>& data) {
    if (!m_tableDataGrid || !m_tableDataPanel) { return; }
    m_tableDataGrid->removeAllWidgets();
    
    const float CELL_ROW_HEIGHT = 30.f;
    std::vector<float> columnWidthPercentages = { 0.20f, 0.20f, 0.20f, 0.20f, 0.20f };
    
    if (data.empty()) {
        auto emptyLabel = tgui::Label::create(L"Нет данных для отображения");
        if (emptyLabel) {
            emptyLabel->setHorizontalAlignment(tgui::Label::HorizontalAlignment::Center);
            emptyLabel->setSize({ "100%", CELL_ROW_HEIGHT }); // Занять всю ширину грида, высота строки
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
        actualDataGridWidth = 400; // Крайний безопасный вариант
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

                if (grid_row_index < 1 && i < 1 * step) { // Отладка только для первой строки данных
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
            errorMessages << L"Масса спутника (m) вне диапазона [0.1, 100.000] кг.\n"; 
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"Неверный формат массы спутника (m).\n";
    }

    try {
        params.M_central_body_factor = std::stod(M_str);
        if (params.M_central_body_factor < 0.1 || params.M_central_body_factor > 1.0e5) {
            params.isValid = false;
            errorMessages << L"Масса центр. тела (M) вне диапазона [0,1, 100.000].\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"Неверный формат массы центр. тела (M).\n";
    }

    try {
        params.V0_m_per_s = std::stod(V0_str);
        if (params.V0_m_per_s < 0.0 || params.V0_m_per_s > 1000) { 
            params.isValid = false;
            errorMessages << L"Начальная скорость (V0) вне диапазона [0, 1000] м/с.\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"Неверный формат начальной скорости (V0).\n";
    }

    try {
        params.T_days = std::stod(T_str);
        if (params.T_days < 1 || params.T_days > 1.0e7) {
            params.isValid = false;
            errorMessages << L"Время симуляции (T) вне диапазона [1, 10.000.000] сут.\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"Неверный формат времени симуляции (T).\n";
    }

    try {
        params.k_coeff = std::stod(k_str);
        if (params.k_coeff < 0.0 || params.k_coeff > 2.0) {
            params.isValid = false;
            errorMessages << L"Коэффициент k вне диапазона [0.0, 2.0].\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"Неверный формат коэффициента k.\n";
    }

    try {
        params.F_coeff = std::stod(F_str);
        if (params.F_coeff < 0.0 || params.F_coeff > 2.0) {
            params.isValid = false;
            errorMessages << L"Коэффициент F вне диапазона [0.0, 2.0].\n";
        }
    }
    catch (const std::exception&) {
        params.isValid = false;
        errorMessages << L"Неверный формат коэффициента F.\n";
    }

    params.errorMessage = errorMessages.str();
    return params;
}

// --- Главный цикл и обработка событий ---
void UserInterface::run() {
    m_window.setFramerateLimit(60); // Ограничение FPS для плавности и снижения нагрузки
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

            // Перерисовываем таблицу с новыми размерами
            // Важно: m_currentTableData должно быть актуальным
            if (m_trajectoryAvailable || !m_currentTableData.empty() || m_tableDataGrid->getWidgets().size() > 0) {
                populateTable(m_currentTableData);
            }
        }

        
    }
}

void UserInterface::update() {
    // Убрана логика работы с ReadME
}

void UserInterface::render() {
    if (m_trajectoryCanvas) {
        sf::RenderTexture& canvasRT = m_trajectoryCanvas->getRenderTexture();
        sf::Vector2f canvasWidgetSize = m_trajectoryCanvas->getSize(); // Размер виджета TGUI

        if (canvasRT.getSize().x != static_cast<unsigned int>(canvasWidgetSize.x) ||
            canvasRT.getSize().y != static_cast<unsigned int>(canvasWidgetSize.y)) {

            if (canvasWidgetSize.x > 0 && canvasWidgetSize.y > 0) {
                if (!canvasRT.create(static_cast<unsigned int>(canvasWidgetSize.x), static_cast<unsigned int>(canvasWidgetSize.y))) {
                    std::cerr << "ERROR: Failed to recreate Canvas RenderTexture!" << std::endl;
                }
            }
        }

        canvasRT.clear(sf::Color(250, 250, 250));   // Фон канваса
        drawTrajectoryOnCanvas(canvasRT);           // Этот метод теперь сам устанавливает и сбрасывает View
        m_trajectoryCanvas->display();
    }
    m_window.clear(sf::Color(220, 220, 220));
    m_gui.draw();
    m_window.display();
}