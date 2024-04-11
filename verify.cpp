#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::ifstream file0("./IPU_INPUTS0.out");
    std::ifstream file1("./IPU_OUTPUTS0.out");
    std::string line0;
    std::string line1;
    int input_count = 0;
    int adj_count = 0;

    while(std::getline(file0, line0)) {
        adj_count = input_count++ % 5;

        switch(adj_count) {
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
                break;
            
            default:
                break;
        }

    }

    file0.close();
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