#include "device_libraries/firehose_ipu.hpp"

enum TASK {TENSOR_DECOMP, MAT_MUL, MAT_ADD, TRANSPOSE, CONVOLUTION};

int main(int argc, char *argv[]) {

    namespace po = boost::program_options;

    po::options_description desc("Options");

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
        ("row", po::value<int>(&row)->default_value(3), "number of rows in matrices")
        ("col", po::value<int>(&col)->default_value(3), "number of columns in matrices")
        ("num_packets", po::value<int>(&num_packets)->default_value(3), "number of packets")
        ("num_streams", po::value<int>(&num_streams)->default_value(3), "number of streams")
        ("num_devices", po::value<int>(&num_devices)->default_value(1), "number of devices")
        ("seed", po::value<int>(&seed)->default_value(42), "seed for random generation")
        ("get_from_file", po::value<bool>(&get_from_file)->default_value(false), "whether or not to read input from a file")
        ("con_task", po::value<int>(&con_task)->default_value(TASK::MAT_MUL), "the chosen consumption task");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if(vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

    switch(vm["con_task"].as<int>()) {
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