#include "device_libraries/firehose_ipu.hpp"

enum TASK {TENSOR_DECOMP, MAT_MUL, MAT_ADD, TRANSPOSE, CONVOLUTION};

int main() {

    long unsigned int row = 2;
    long unsigned int col = 2;
    long unsigned int num_packets = 256;
    long unsigned int num_streams = 1;
    long unsigned int num_devices = 1;
    long unsigned int seed = 42;
    bool get_from_file = false;
    int con_task = TASK::MAT_MUL;

    switch(con_task) {
        case TASK::TENSOR_DECOMP:
            tensorDecomp(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;

        case TASK::MAT_MUL:
            matMul(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;

        case TASK::MAT_ADD:
            matAdd(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;

        case TASK::TRANSPOSE:
            transpose(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;

        case TASK::CONVOLUTION:
            convolution(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;
    }

    return 0;
}