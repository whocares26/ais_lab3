#pragma once
#include <string>
#include <iostream>
#include "engine/Colors.hpp"

inline int readChoice(int min, int max) {
    while (true) {
        std::cout << GREEN << "Ваш выбор: " << RESET;
        std::string line;
        std::getline(std::cin, line);
        try {
            int value = std::stoi(line);
            if (value >= min && value <= max)
                return value;
        } catch (...) {}
        std::cout << RED << "Ожидается число от " << min << " до " << max << RESET << "\n";
    }
}