#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::ifstream file0("./IPU_INPUTS0.out");
    std::ifstream file1("./IPU_OUTPUTS0.out");
    std::string line;
    int input_count = 0;

    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    while (std::getline(file, line)) {
        // Check if the line starts with "GenMatrix"

        if (line.find("GenMatrix") != std::string::npos) {
            std::cout << "Generated Input" << std::to_string(input_count++) std::endl;
            std::cout << line << std::endl;
            continue;
        }
    }

    file.close();
    return 0;
}

/*
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <unistd.h>
#include <string>

int main() {

    std::cout << "Verifying Thread 0..." << std::endl << std::endl;

    std::cout << "Packet 0" << std::endl;

    std::cout << "Packet 1" << std::endl;

    std::cout << "Packet 2" << std:: endl;

    return 0;
}
*/