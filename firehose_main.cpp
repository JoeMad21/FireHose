#include "device_libraries/firehose_ipu.hpp"

enum TASK {TENSOR_DECOMP, MAT_MUL, MAT_ADD, TRANSPOSE, CONVOLUTION};

int main(int argc, char *argv[]) {

    boost::program_options::options_description desc("Options");

    int row = 3;
    int col = 3;
    int num_packets = 3;
    int num_streams = 3;
    int num_devices = 1;
    int seed = 42;
    bool get_from_file = false;
    int con_task = TASK::MAT_ADD;

    desc.add_options()
        ("help", "produce help message")
        ("row", value<int>(&row)->default_value(3), "number of rows in matrices")
        ("col", value<int>(&col)->default_value(3), "number of columns in matrices")
        ("num_packets", value<int>(&num_packets)->default_value(3), "number of packets")
        ("num_streams", value<int>(&num_streams)->default_value(3), "number of streams")
        ("num_devices", value<int>(&num_devices)->default_value(1), "number of devices")
        ("seed", value<int>(&seed)->default_value(42), "seed for random generation")
        ("get_from_file", value<bool>(&get_from_file)->default_value(false), "whether or not to read input from a file")
        ("con_task", value<int>(&con_task)->default_value(TASK::MAT_MUL), "the chosen consumption task");

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
        boost::program_options::notify(vm);

        if(vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

    switch(vm.count["con_task"]) {
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