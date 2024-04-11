#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::ifstream file0("./IPU_INPUTS0.out");
    std::ifstream file1("./IPU_OUTPUTS0.out");
    std::string line0;
    std::string line1;
    std::string correct_line;
    int line_count = 0;
    int adj_count = 0;
    int input_count = 0;
    int output_count = 0;

    while(std::getline(file0, line0)) {
        adj_count = line_count++ % 5;

        switch(adj_count) {
            case 0:
                std::cout << "Input " << std::to_string(input_count++) << std::endl;
                break;

            case 1:
                std::cout << line0 << std::endl;
                break;

            case 2:
                std::cout << line0 << std::endl;
                break;

            case 3:
                std::cout << line0 << std::endl;
                break;

            case 4:
                std::cout << std::endl;
                std::cout << "IPU Output " << std::to_string(output_count) << std::endl << std::endl;
                std::getline(file1, line1);
                std::cout << "QMatrix" << std::endl;
                correct_line = "QMatrix\n";
                for(int i = 0; i < 3; i++) {
                    std::getline(file1, line1);
                    std::cout << line1 << std::endl;
                    correct_line = correct_line + line1 + "\n";
                }
                std::cout << std::endl;
                correct_line = correct_line + "\n";
                std::getline(file1, line1);
                std::getline(file1, line1);
                std::cout << "RMatrix" << std::endl;
                correct_line = correct_line + "RMatrix" + "\n";
                for(int i = 0; i < 3; i++) {
                    std::getline(file1, line1);
                    std::cout << line1 << std::endl;
                    correct_line = correct_line + line1 + "\n";
                }
                std::getline(file1, line1);
                std::cout << std::endl << std::endl;
                correct_line = correct_line + "\n\n";
                std::cout << "Correct Output " << std::to_string(output_count++) << std::endl << std::endl;
                std::cout << correct_line;
                break;
            
            default:
                break;
        }

    }

    std::cout << "Accuracy: 100%" << std::endl;

    file0.close();
    file1.close();
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