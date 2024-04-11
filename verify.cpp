#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::ifstream file("your_file_path.txt");
    std::string line;
    bool printSection = false;
    int sectionCount = 0;

    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    while (std::getline(file, line)) {
        // Check if the line starts with "GenMatrix"
        if (line.find("GenMatrix") != std::string::npos) {
            sectionCount++;
            // Start printing if this is the second section
            printSection = (sectionCount == 2);
            continue; // Skip the header line
        }
        
        // If in the second section, print the line
        if (printSection) {
            std::cout << line << std::endl;
            // Optional: Stop printing after the second section
            // if(line == "") printSection = false; // Assuming sections are separated by blank lines
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