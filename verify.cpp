#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::ifstream file0("./IPU_INPUTS0.out");
    std::ifstream file1("./IPU_OUTPUTS0.out");
    std::string line0;
    std::string line1;
    int input_count = 0;

    if (!file0.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    while (std::getline(file0, line0)) {
        // Check if the line starts with "GenMatrix"

        //std::cout << "Generated Input " << std::to_string(input_count++) << std::endl;
        if (line0.find("GenMatrix") != std::string::npos) {
            std::cout << line0 << std::endl;
            continue;
        }
    }

    file0.close();
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