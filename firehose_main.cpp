#include "device_libraries/firehose_ipu.hpp"

int main() {

    long unsigned int row = 3;
    long unsigned int col = 3;
    long unsigned int num_streams = 1;
    long unsigned int num_devices = 1;


    tensorDecomp(row, col, num_streams, num_devices);

    return 0;
}