#include "device_libraries/firehose_ipu.hpp"
#include <cstdio>
#include <ctime>
#include <chrono>

enum TASK {TENSOR_DECOMP, MAT_MUL, MAT_ADD, TRANSPOSE, CONVOLUTION};
enum HARDWARE {IPU, MODEL, CPU};


int main(int argc, char *argv[]) {

    namespace po = boost::program_options;

    po::options_description desc("Options");

    int device = HARDWARE::IPU;
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
        ("device", po::value<int>(&device)->default_value(HARDWARE::IPU), "Device mode selected")
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

    std::clock_t startcputime = std::clock();
    double cpu_duration = (std::clock() - startcputime) / (double)CLOCKS_PER_SEC;
    auto wcts = std::chrono::system_clock::now();
    std::chrono::duration<double> wctduration = (std::chrono::system_clock::now() - wcts);


    switch(vm["con_task"].as<int>()) {
        case TASK::TENSOR_DECOMP:
            startcputime = std::clock();
            wcts = std::chrono::system_clock::now();
            tensorDecomp(vm);
            cpu_duration = (std::clock() - startcputime) / (double)CLOCKS_PER_SEC;
            wctduration = (std::chrono::system_clock::now() - wcts);
            break;

        case TASK::MAT_MUL:
            startcputime = std::clock();
            wcts = std::chrono::system_clock::now();
            matMul(vm);
            cpu_duration = (std::clock() - startcputime) / (double)CLOCKS_PER_SEC;
            wctduration = (std::chrono::system_clock::now() - wcts);
            break;

        case TASK::MAT_ADD:
            startcputime = std::clock();
            wcts = std::chrono::system_clock::now();
            matAdd(vm);
            cpu_duration = (std::clock() - startcputime) / (double)CLOCKS_PER_SEC;
            wctduration = (std::chrono::system_clock::now() - wcts);
            break;

        case TASK::TRANSPOSE:
            startcputime = std::clock();
            wcts = std::chrono::system_clock::now();
            transpose(vm);
            cpu_duration = (std::clock() - startcputime) / (double)CLOCKS_PER_SEC;
            wctduration = (std::chrono::system_clock::now() - wcts);
            break;

        case TASK::CONVOLUTION:
            startcputime = std::clock();
            wcts = std::chrono::system_clock::now();
            convolution(vm);
            cpu_duration = (std::clock() - startcputime) / (double)CLOCKS_PER_SEC;
            wctduration = (std::chrono::system_clock::now() - wcts);
            break;
    }

    std::cout << "Finished in " << cpu_duration << " seconds [CPU Clock] " << std::endl;
    std::cout << "Finished in " << wctduration.count() << " seconds [Wall Clock]" << std::endl;

    return 0;
}