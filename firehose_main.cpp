#include "device_libraries/firehose_ipu.hpp"

#define TENSOR_DECOMP 0
#define MAT_MUL 1
#define MAT_ADD 2

int main() {

    long unsigned int row = 3;
    long unsigned int col = 3;
    long unsigned int num_packets = 3;
    long unsigned int num_streams = 2;
    long unsigned int num_devices = 1;
    long unsigned int seed = 42;
    bool get_from_file = false;
    int con_task = TENSOR_DECOMP;

    switch(con_task) {
        case TENSOR_DECOMP:
            tensorDecomp(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;

        case MAT_MUL:
            matMul(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;

        case MAT_ADD:
            matAdd(row, col, num_packets, num_streams, num_devices, seed, get_from_file);
            break;
    }

    return 0;
}