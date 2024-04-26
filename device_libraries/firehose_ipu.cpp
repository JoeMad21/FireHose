#include "firehose_ipu.hpp"

#define NUM_PROGRAMS 3
#define PRODUCER 0
#define CONSUMER 1

#define IPU_HW 0
#define IPU_MDL 1
#define CPU_HW 2

void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet, int io) {

    std::string fileName;
  switch(io) {
    case 0:
        fileName = "IPU_INPUTS" + std::to_string(id) + ".out";
        break;
    default:
        fileName = "IPU_OUTPUTS" + std::to_string(id) + ".out";
        break;
  }
  std::ofstream fileStream(fileName, std::ios::app);
  fileStream << matrix_name << " THREAD " << id << " PACKET " << packet << std::endl;

  for (int i = 0; i < matrix.size(); i++) {

    fileStream << std::fixed << matrix[i] << "\t";
    
    if ( (i+1)%cols == 0) {
      fileStream << std::endl;
    }

  }

  fileStream << std::endl;
  fileStream.close();

}


poplar::Device getDevice(int hw_mode, int num_devices) {
    std::cout << "Getting Device..." << std::endl;

    auto manager = poplar::DeviceManager::createDeviceManager();
    std::vector<poplar::Device> hwDevices;

    switch(hw_mode) {
        case IPU_HW:
            hwDevices = manager.getDevices(poplar::TargetType::IPU, num_devices);
            break;

        case IPU_MDL:
            hwDevices = manager.getDevices(poplar::TargetType::IPU_MODEL, num_devices);
            break;

        case CPU_HW:
            hwDevices = manager.getDevices(poplar::TargetType::CPU, num_devices);
            break;
    }

    auto it = std::find_if(hwDevices.begin(), hwDevices.end(), [](poplar::Device &device) { return device.attach(); });
    poplar::Device device;

    if (it != hwDevices.end()) {
        device = std::move(*it);
    }

    std::cout << "Got Device!" << std::endl;

    return device;
}

//void createVariableTensors() {

//}

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file) {

    poplar::Device device = getDevice(0, num_devices);

    /* Expose Shared Memory */

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    // Programs
    std::vector<poplar::program::Program> progs(num_streams*NUM_PROGRAMS);

    // Flags
    bool data_ready_flags[num_streams];

    for (int i = 0; i < num_streams; i++) {
        data_ready_flags[i] = false;
    }

    // Variable Tensors
    std::cout << "Adding Tensors..." << std::endl;

    std::vector<poplar::Tensor> v_io_in0(num_streams);
    std::vector<poplar::Tensor> v_con0(num_streams);
    std::vector<poplar::Tensor> v_io_out0(num_streams);

    std::vector<poplar::Tensor> v_con1(num_streams);
    std::vector<poplar::Tensor> v_io_out1(num_streams);

    std::string db_name;

    for (int i = 0; i < num_streams; i++) {

        /* Input to QR Factorization */
        db_name = "Input Tensor " + std::to_string(i) + " of Set 0";
        v_io_in0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_in0[i]);

        db_name = "Consumption Tensor " + std::to_string(i) + " of Set 0";
        v_con0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con0[i]);

        db_name = "Output Tensor " + std::to_string(i) + " of Set 0";
        v_io_out0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_out0[i]);

        /* Necessary Identity to QR Factorization */
        db_name = "Consumption Tensor" + std::to_string(i) + " of Set 1";
        v_con1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con1[i]);

        db_name = "Output Tensor " + std::to_string(i) + " of Set 1";
        v_io_out1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_out1[i]);
    }

    // Constant Tensors
    std::vector<float> vec_id;

    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            if (i == j) {
                vec_id.push_back(1.0);
            }
            else {
                vec_id.push_back(0.0);
            }
        }
    }

    auto c_id = graph.addConstant<float>(poplar::FLOAT, {row, col}, vec_id.data(), "Constant Identity Tensor");
    poputil::mapTensorLinearly(graph, c_id);

    std::cout << "Added Tensors!" << std::endl;

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    // Vertices
    std::cout << "Adding Vertices..." << std::endl;

    std::vector<poplar::ComputeSet> cps_io_in(num_streams);
    std::vector<poplar::ComputeSet> cps_io_out(num_streams);

    for (int i = 0; i < num_streams; i++) {
        db_name = "IO in CS " + std::to_string(i);
        cps_io_in[i] = graph.addComputeSet(db_name);

        db_name = "IO in CS " + std::to_string(i);
        cps_io_out[i] = graph.addComputeSet(db_name);
    }

    std::vector<poplar::VertexRef> vtx_in0(num_streams);
    std::vector<poplar::VertexRef> vtx_out0(num_streams);
    std::vector<poplar::VertexRef> vtx_out1(num_streams);

    for (int i = 0; i < num_streams; i++) {

        vtx_in0[i] = graph.addVertex(cps_io_in[i], "IOVertex");
        graph.setTileMapping(vtx_in0[i], i+5);

        vtx_out0[i] = graph.addVertex(cps_io_out[i], "IOVertex");
        graph.setTileMapping(vtx_out0[i], i+7);
        vtx_out1[i] = graph.addVertex(cps_io_out[i], "IOVertex");
        graph.setTileMapping(vtx_out1[i], i+9);
    }

    for(int i = 0; i < num_streams; i++) {
        graph.connect(vtx_in0[i]["strm_in"], v_io_in0[i]);
        graph.connect(vtx_in0[i]["strm_out"], v_con0[i]);

        graph.connect(vtx_out0[i]["strm_in"], v_con0[i]);
        graph.connect(vtx_out0[i]["strm_out"], v_io_out0[i]);

        graph.connect(vtx_out1[i]["strm_in"], v_con1[i]);
        graph.connect(vtx_out1[i]["strm_out"], v_io_out1[i]);
    }

    std::cout << "Added Vertices!" << std::endl;

    // Streams
    std::cout << "Adding Streams..." << std::endl;

    std::vector<poplar::DataStream> strm_in0(num_streams);
    std::vector<poplar::DataStream> strm_out0(num_streams);
    std::vector<poplar::DataStream> strm_out1(num_streams);

    poplar::OptionFlags streamOpts {
      {"bufferingDepth", "2"},
    };

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        strm_in0[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        strm_out0[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col, streamOpts);

        db_name = "Output Stream " + std::to_string(i) + " for output 1";
        strm_out1[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col, streamOpts);
    }

    std::cout << "Added Streams!" << std::endl;

    // CPU Vectors
    std::vector<std::vector<float>> cpu_in0(num_streams, std::vector<float> (row*col, 5.0));
    std::vector<std::vector<float>> cpu_out0(num_streams, std::vector<float> (row*col, 5.0));
    std::vector<std::vector<float>> cpu_out1(num_streams, std::vector<float> (row*col, 5.0));

    std::cout << "Adding Programs..." << std::endl;

    /* Stream Inputs Programs */

    int prog_idx = 0;

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(strm_in0[i], v_io_in0[i]));

        seq.add(poplar::program::Execute(cps_io_in[i]));

        progs[prog_idx++] = seq;
    }

    /* Consumption Task Programs */

    poplin::addCodelets(graph);

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(c_id, v_con1[i]));

        poplin::experimental::QRFactorization(graph, v_con0[i], v_con1[i], seq);

        progs[prog_idx++] = seq;
    }

    /* Stream Outputs Programs */

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Execute(cps_io_out[i]));

        seq.add(poplar::program::Copy(v_io_out0[i], strm_out0[i]));
        seq.add(poplar::program::Copy(v_io_out1[i], strm_out1[i]));

        progs[prog_idx++] = seq;
    }

    std::cout << "Added Programs!" << std::endl;

    /* Create and Load Engine */

    std::cout << "Loading Device..." << std::endl;

    auto exe = poplar::compileGraph(graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(device);

    std::cout << "Loaded Device!" << std::endl;

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        engine.connectStream(db_name, cpu_in0[i].data(), cpu_in0[i].data() + cpu_in0[i].size());

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        engine.connectStream(db_name, cpu_out0[i].data(), cpu_out0[i].data() + cpu_out0[i].size());

        db_name = "Output Stream " + std::to_string(i) + " for output 1";
        engine.connectStream(db_name, cpu_out1[i].data(), cpu_out1[i].data() + cpu_out1[i].size());
    }

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        //int packet = 0;
        
        std::mt19937 gen(seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < row*col; i++) {
                        cpu_in0[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    printMatrix("GenMatrix", cpu_in0[rel_id], col, rel_id, packet, 0);

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                        engine.run(num_streams+rel_id);
                        engine.run((num_streams*2)+rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("QMatrix", cpu_out0[rel_id], col, rel_id, packet, 1);
                        printMatrix("RMatrix", cpu_out1[rel_id], col, rel_id, packet, 1);
                    }

                    data_ready_flags[rel_id] = false;
                }
                break;
        }
    }

    return;
}

void matMul(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file) {

    /* Get an IPU Device */

    std::cout << "Getting Device..." << std::endl;

    auto manager = poplar::DeviceManager::createDeviceManager();
    auto hwDevices = manager.getDevices(poplar::TargetType::IPU, num_devices);
    auto it = std::find_if(hwDevices.begin(), hwDevices.end(), [](poplar::Device &device) { return device.attach(); });
    poplar::Device device;

    if (it != hwDevices.end()) {
        device = std::move(*it);
    }

    std::cout << "Got Device!" << std::endl;

    /* Expose Shared Memory */

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    // Programs
    std::vector<poplar::program::Program> progs(num_streams*NUM_PROGRAMS);

    // Flags
    bool data_ready_flags[num_streams];

    for (int i = 0; i < num_streams; i++) {
        data_ready_flags[i] = false;
    }

    // Variable Tensors
    std::cout << "Adding Tensors..." << std::endl;

    std::vector<poplar::Tensor> v_io_in0(num_streams);
    std::vector<poplar::Tensor> v_con0(num_streams);

    std::vector<poplar::Tensor> v_io_in1(num_streams);
    std::vector<poplar::Tensor> v_con1(num_streams);

    std::vector<poplar::Tensor> v_io_out0(num_streams);

    std::string db_name;

    for (int i = 0; i < num_streams; i++) {

        /* Input to QR Factorization */
        db_name = "Input Tensor " + std::to_string(i) + " of Set 0";
        v_io_in0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_in0[i]);

        /* Input to QR Factorization */
        db_name = "Input Tensor " + std::to_string(i) + " of Set 1";
        v_io_in1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_in1[i]);

        db_name = "Consumption Tensor " + std::to_string(i) + " of Set 0";
        v_con0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con0[i]);

        db_name = "Consumption Tensor " + std::to_string(i) + " of Set 1";
        v_con1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con1[i]);

        db_name = "Output Tensor " + std::to_string(i) + " of Set 0";
        v_io_out0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_out0[i]);
    }

    // Constant Tensors
    //None needed

    std::cout << "Added Tensors!" << std::endl;

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    // Vertices
    std::cout << "Adding Vertices..." << std::endl;

    std::vector<poplar::ComputeSet> cps_io_in(num_streams);
    std::vector<poplar::ComputeSet> cps_io_out(num_streams);

    for (int i = 0; i < num_streams; i++) {
        db_name = "IO in CS " + std::to_string(i);
        cps_io_in[i] = graph.addComputeSet(db_name);

        db_name = "IO in CS " + std::to_string(i);
        cps_io_out[i] = graph.addComputeSet(db_name);
    }

    std::vector<poplar::VertexRef> vtx_in0(num_streams);
    std::vector<poplar::VertexRef> vtx_in1(num_streams);
    std::vector<poplar::VertexRef> vtx_out0(num_streams);

    for (int i = 0; i < num_streams; i++) {

        vtx_in0[i] = graph.addVertex(cps_io_in[i], "IOVertex");
        graph.setTileMapping(vtx_in0[i], i+5);
        vtx_in1[i] = graph.addVertex(cps_io_in[i], "IOVertex");
        graph.setTileMapping(vtx_in1[i], i+7);

        vtx_out0[i] = graph.addVertex(cps_io_out[i], "IOVertex");
        graph.setTileMapping(vtx_out0[i], i+9);
    }

    for(int i = 0; i < num_streams; i++) {
        graph.connect(vtx_in0[i]["strm_in"], v_io_in0[i]);
        graph.connect(vtx_in0[i]["strm_out"], v_con0[i]);

        graph.connect(vtx_in1[i]["strm_in"], v_io_in1[i]);
        graph.connect(vtx_in1[i]["strm_out"], v_con1[i]);

        graph.connect(vtx_out0[i]["strm_in"], v_con0[i]);
        graph.connect(vtx_out0[i]["strm_out"], v_io_out0[i]);
    }

    std::cout << "Added Vertices!" << std::endl;

    // Streams
    std::cout << "Adding Streams..." << std::endl;

    std::vector<poplar::DataStream> strm_in0(num_streams);
    std::vector<poplar::DataStream> strm_in1(num_streams);
    std::vector<poplar::DataStream> strm_out0(num_streams);

    poplar::OptionFlags streamOpts {
      {"bufferingDepth", "2"},
    };

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        strm_in0[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);

        db_name = "Input Stream " + std::to_string(i) + " for input 1";
        strm_in1[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        strm_out0[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col, streamOpts);
    }

    std::cout << "Added Streams!" << std::endl;

    // CPU Vectors
    std::vector<std::vector<float>> cpu_in0(num_streams, std::vector<float> (row*col, 5.0));
    std::vector<std::vector<float>> cpu_in1(num_streams, std::vector<float> (row*col, 5.0));
    std::vector<std::vector<float>> cpu_out0(num_streams, std::vector<float> (row*col, 5.0));

    std::cout << "Adding Programs..." << std::endl;

    /* Stream Inputs Programs */

    int prog_idx = 0;

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(strm_in0[i], v_io_in0[i]));
        seq.add(poplar::program::Copy(strm_in1[i], v_io_in1[i]));

        seq.add(poplar::program::Execute(cps_io_in[i]));

        progs[prog_idx++] = seq;
    }

    /* Consumption Task Programs */

    poplin::addCodelets(graph);

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        //seq.add(poplar::program::Copy(c_id, v_con1[i]));

        //poplin::experimental::QRFactorization(graph, v_con0[i], v_con1[i], seq);

        poplar::Tensor matmul_out = poplin::matMul(graph, v_con0[i], v_con1[i], seq, "MatMul");

        seq.add(poplar::program::Copy(matmul_out, v_con0[i]));

        progs[prog_idx++] = seq;
    }

    /* Stream Outputs Programs */

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Execute(cps_io_out[i]));

        seq.add(poplar::program::Copy(v_io_out0[i], strm_out0[i]));

        progs[prog_idx++] = seq;
    }

    std::cout << "Added Programs!" << std::endl;

    /* Create and Load Engine */

    std::cout << "Loading Device..." << std::endl;

    auto exe = poplar::compileGraph(graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(device);

    std::cout << "Loaded Device!" << std::endl;

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        engine.connectStream(db_name, cpu_in0[i].data(), cpu_in0[i].data() + cpu_in0[i].size());

        db_name = "Input Stream " + std::to_string(i) + " for input 1";
        engine.connectStream(db_name, cpu_in1[i].data(), cpu_in1[i].data() + cpu_in1[i].size());

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        engine.connectStream(db_name, cpu_out0[i].data(), cpu_out0[i].data() + cpu_out0[i].size());
    }

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        //int packet = 0;
        
        std::mt19937 gen(seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < row*col; i++) {
                        cpu_in0[rel_id][i] = distribution(gen);
                    }

                    for (int i = 0; i < row*col; i++) {
                        cpu_in1[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("MATRIX A", cpu_in0[rel_id], col, rel_id, packet, 0);
                        printMatrix("MATRIX B", cpu_in1[rel_id], col, rel_id, packet, 0);
                    }

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                        engine.run(num_streams+rel_id);
                        engine.run((num_streams*2)+rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("RESULT MATRIX", cpu_out0[rel_id], col, rel_id, packet, 1);
                    }

                    data_ready_flags[rel_id] = false;
                }
                break;
        }
    }

    return;
}

void matAdd(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file) {

    /* Get an IPU Device */

    std::cout << "Getting Device..." << std::endl;

    auto manager = poplar::DeviceManager::createDeviceManager();
    auto hwDevices = manager.getDevices(poplar::TargetType::IPU, num_devices);
    auto it = std::find_if(hwDevices.begin(), hwDevices.end(), [](poplar::Device &device) { return device.attach(); });
    poplar::Device device;

    if (it != hwDevices.end()) {
        device = std::move(*it);
    }

    std::cout << "Got Device!" << std::endl;

    /* Expose Shared Memory */

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    // Programs
    std::vector<poplar::program::Program> progs(num_streams*NUM_PROGRAMS);

    // Flags
    bool data_ready_flags[num_streams];

    for (int i = 0; i < num_streams; i++) {
        data_ready_flags[i] = false;
    }

    // Variable Tensors
    std::cout << "Adding Tensors..." << std::endl;

    std::vector<poplar::Tensor> v_io_in0(num_streams);
    std::vector<poplar::Tensor> v_con0(num_streams);

    std::vector<poplar::Tensor> v_io_in1(num_streams);
    std::vector<poplar::Tensor> v_con1(num_streams);

    std::vector<poplar::Tensor> v_io_out0(num_streams);

    std::string db_name;

    for (int i = 0; i < num_streams; i++) {

        /* Input to QR Factorization */
        db_name = "Input Tensor " + std::to_string(i) + " of Set 0";
        v_io_in0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_in0[i]);

        /* Input to QR Factorization */
        db_name = "Input Tensor " + std::to_string(i) + " of Set 1";
        v_io_in1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_in1[i]);

        db_name = "Consumption Tensor " + std::to_string(i) + " of Set 0";
        v_con0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con0[i]);

        db_name = "Consumption Tensor " + std::to_string(i) + " of Set 1";
        v_con1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con1[i]);

        db_name = "Output Tensor " + std::to_string(i) + " of Set 0";
        v_io_out0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_out0[i]);
    }

    // Constant Tensors
    //None needed

    std::cout << "Added Tensors!" << std::endl;

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    // Vertices
    std::cout << "Adding Vertices..." << std::endl;

    std::vector<poplar::ComputeSet> cps_io_in(num_streams);
    std::vector<poplar::ComputeSet> cps_io_out(num_streams);

    for (int i = 0; i < num_streams; i++) {
        db_name = "IO in CS " + std::to_string(i);
        cps_io_in[i] = graph.addComputeSet(db_name);

        db_name = "IO in CS " + std::to_string(i);
        cps_io_out[i] = graph.addComputeSet(db_name);
    }

    std::vector<poplar::VertexRef> vtx_in0(num_streams);
    std::vector<poplar::VertexRef> vtx_in1(num_streams);
    std::vector<poplar::VertexRef> vtx_out0(num_streams);

    for (int i = 0; i < num_streams; i++) {

        vtx_in0[i] = graph.addVertex(cps_io_in[i], "IOVertex");
        graph.setTileMapping(vtx_in0[i], i+5);
        vtx_in1[i] = graph.addVertex(cps_io_in[i], "IOVertex");
        graph.setTileMapping(vtx_in1[i], i+7);

        vtx_out0[i] = graph.addVertex(cps_io_out[i], "IOVertex");
        graph.setTileMapping(vtx_out0[i], i+9);
    }

    for(int i = 0; i < num_streams; i++) {
        graph.connect(vtx_in0[i]["strm_in"], v_io_in0[i]);
        graph.connect(vtx_in0[i]["strm_out"], v_con0[i]);

        graph.connect(vtx_in1[i]["strm_in"], v_io_in1[i]);
        graph.connect(vtx_in1[i]["strm_out"], v_con1[i]);

        graph.connect(vtx_out0[i]["strm_in"], v_con0[i]);
        graph.connect(vtx_out0[i]["strm_out"], v_io_out0[i]);
    }

    std::cout << "Added Vertices!" << std::endl;

    // Streams
    std::cout << "Adding Streams..." << std::endl;

    std::vector<poplar::DataStream> strm_in0(num_streams);
    std::vector<poplar::DataStream> strm_in1(num_streams);
    std::vector<poplar::DataStream> strm_out0(num_streams);

    poplar::OptionFlags streamOpts {
      {"bufferingDepth", "2"},
    };

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        strm_in0[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);

        db_name = "Input Stream " + std::to_string(i) + " for input 1";
        strm_in1[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        strm_out0[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col, streamOpts);
    }

    std::cout << "Added Streams!" << std::endl;

    // CPU Vectors
    std::vector<std::vector<float>> cpu_in0(num_streams, std::vector<float> (row*col, 5.0));
    std::vector<std::vector<float>> cpu_in1(num_streams, std::vector<float> (row*col, 5.0));
    std::vector<std::vector<float>> cpu_out0(num_streams, std::vector<float> (row*col, 5.0));

    std::cout << "Adding Programs..." << std::endl;

    /* Stream Inputs Programs */

    int prog_idx = 0;

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(strm_in0[i], v_io_in0[i]));
        seq.add(poplar::program::Copy(strm_in1[i], v_io_in1[i]));

        seq.add(poplar::program::Execute(cps_io_in[i]));

        progs[prog_idx++] = seq;
    }

    /* Consumption Task Programs */

    poplin::addCodelets(graph);

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        //seq.add(poplar::program::Copy(c_id, v_con1[i]));

        //poplin::experimental::QRFactorization(graph, v_con0[i], v_con1[i], seq);

        poplar::Tensor matadd_out = popops::add(graph, v_con0[i], v_con1[i], seq, "MatMul");

        seq.add(poplar::program::Copy(matadd_out, v_con0[i]));

        progs[prog_idx++] = seq;
    }

    /* Stream Outputs Programs */

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Execute(cps_io_out[i]));

        seq.add(poplar::program::Copy(v_io_out0[i], strm_out0[i]));

        progs[prog_idx++] = seq;
    }

    std::cout << "Added Programs!" << std::endl;

    /* Create and Load Engine */

    std::cout << "Loading Device..." << std::endl;

    auto exe = poplar::compileGraph(graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(device);

    std::cout << "Loaded Device!" << std::endl;

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        engine.connectStream(db_name, cpu_in0[i].data(), cpu_in0[i].data() + cpu_in0[i].size());

        db_name = "Input Stream " + std::to_string(i) + " for input 1";
        engine.connectStream(db_name, cpu_in1[i].data(), cpu_in1[i].data() + cpu_in1[i].size());

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        engine.connectStream(db_name, cpu_out0[i].data(), cpu_out0[i].data() + cpu_out0[i].size());
    }

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        //int packet = 0;
        
        std::mt19937 gen(seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < row*col; i++) {
                        cpu_in0[rel_id][i] = distribution(gen);
                    }

                    for (int i = 0; i < row*col; i++) {
                        cpu_in1[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("MATRIX A", cpu_in0[rel_id], col, rel_id, packet, 0);
                        printMatrix("MATRIX B", cpu_in1[rel_id], col, rel_id, packet, 0);
                    }

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                        engine.run(num_streams+rel_id);
                        engine.run((num_streams*2)+rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("RESULT MATRIX", cpu_out0[rel_id], col, rel_id, packet, 1);
                    }

                    data_ready_flags[rel_id] = false;
                }
                break;
        }
    }

    return;
}

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices) {

//}