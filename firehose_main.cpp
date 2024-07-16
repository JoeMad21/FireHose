#include "device_libraries/firehose_ipu.hpp"
#include <string>
#include <cstdio>
#include <ctime>
#include <chrono>

enum TASK {TENSOR_DECOMP, MAT_MUL, MAT_ADD, TRANSPOSE, CONVOLUTION};
enum HARDWARE {IPU, MODEL, CPU};
enum COMPATSHAPE {TRIANGLEUP, TRIANGLEQR, TRIANGLEDOWN, SQUARE, LINE};

int main(int argc, char *argv[]) {

    namespace po = boost::program_options;

    po::options_description desc("Options");

    long unsigned int device = HARDWARE::IPU;
    long unsigned int dim = 3;
    long unsigned int num_packets = 3;
    long unsigned int num_streams = 3;
    long unsigned int num_devices = 1;
    long unsigned int seed = 42;
    long unsigned int offset = 4;
    bool get_from_file = false;
    long unsigned int experimental = 0;
    long unsigned int con_task = TASK::MAT_ADD;

    desc.add_options()
        ("help", "produce help message")
        ("hw", po::value<long unsigned int>(&device)->default_value(HARDWARE::IPU), "Device mode selected")
        ("dim", po::value<long unsigned int>(&out_dim)->default_value(3), "Matrices dimensions")
        ("packets", po::value<long unsigned int>(&num_packets)->default_value(3), "number of packets")
        ("streams", po::value<long unsigned int>(&num_streams)->default_value(3), "number of streams")
        ("devices", po::value<long unsigned int>(&num_devices)->default_value(1), "number of devices")
        ("seed", po::value<long unsigned int>(&seed)->default_value(42), "seed for random generation")
        ("offset", po::value<long unsigned int>(&offset)->default_value(4), "value to offset tiles")
        ("experimental", po::value<long unsigned int>(&seed)->default_value(0), "whether to use experimental configuration")
        ("get_from_file", po::value<bool>(&get_from_file)->default_value(false), "whether or not to read input from a file")
        ("con_task", po::value<long unsigned int>(&con_task)->default_value(TASK::MAT_MUL), "the chosen consumption task");

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

    switch(vm["con_task"].as<long unsigned int>()) {
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

    std::ofstream fileStream("TimeReport.out", std::ios::app);
    fileStream << "Report: CT" << vm["con_task"].as<long unsigned int>() << " DIM" << vm["col"].as<long unsigned int>() << std::endl;

    fileStream << "Finished in " << cpu_duration << " seconds [CPU Clock] " << std::endl;
    fileStream << "Finished in " << wctduration.count() << " seconds [Wall Clock]" << std::endl;

    return 0;
}