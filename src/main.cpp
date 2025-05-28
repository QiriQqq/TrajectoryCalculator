#include "../include/Calculations.h"         // Для расчетов
#include "../include/TrajectoryVisualizer.h" // Для визуализации
#include "../include/UserInterface.h"        // Для TGUI интерфейса

#include <iostream>
#include <string>
#include <stdexcept>   // Для tgui::Exception и std::exception
#include <iomanip>     // Для std::fixed, std::setprecision в saveTrajectoryToFile
#include <fstream>     // Для std::ofstream в saveTrajectoryToFile

int main() {
    setlocale(LC_ALL, "Rus");

    try {
        UserInterface uiApp;
        uiApp.run();
    }
    catch (const tgui::Exception& e) {
        std::cerr << "TGUI Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Runtime Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        std::cerr << "Standard Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...) {
        std::cerr << "An unknown C++ exception occurred." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
