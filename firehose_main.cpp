#include "device_libraries/firehose_ipu.hpp"

int main() {
    std::cout << "BEGIN" << std::endl;
    int row = 3;
    int col = 3;
    int num_streams = 1;

    tensorDecomp(row, col, num_streams);
    std::cout << "END" << std::endl;
    return 0;
}