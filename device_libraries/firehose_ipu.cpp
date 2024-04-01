#include "firehose_ipu.hpp"

enum Progs {
    STREAM_INPUTS,
    CONSUMPTION_TASK,
    STREAM_OUTPUTS,
    NUM_PROGRAMS
};

void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet) {
  std::string fileName = "results" + std::to_string(id) + ".txt";
  std::ofstream fileStream(fileName, std::ios::app);
  fileStream << matrix_name << " THREAD " << id << " PACKET " << packet << std::endl;
  std::cout << matrix_name << " THREAD " << id << " PACKET " << packet << std::endl;

  for (int i = 0; i < matrix.size(); i++) {

    fileStream << std::fixed << matrix[i] << "\t";
    std::cout << std::fixed << matrix[i] << "\t";
    
    if ( (i+1)%cols == 0) {
      fileStream << std::endl;
      std::cout << std::endl;
    }

  }

  fileStream << std::endl;
  fileStream.close();
  std::cout << std::endl;

}

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices) {

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
    std::vector<poplar::program::Program> progs(num_streams*Progs::NUM_PROGRAMS);

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
    std::vector<float> temp_vec_id(col);
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

    auto cps_io_in = graph.addComputeSet("IO in CS");
    auto cps_io_out = graph.addComputeSet("IO out CS");

    std::vector<poplar::VertexRef> vtx_in0(num_streams);
    std::vector<poplar::VertexRef> vtx_out0(num_streams);
    std::vector<poplar::VertexRef> vtx_out1(num_streams);

    for (int i = 0; i < num_streams; i++) {

        vtx_in0[i] = graph.addVertex(cps_io_in, "IOVertex");
        graph.setTileMapping(vtx_in0[i], i+5);

        vtx_out0[i] = graph.addVertex(cps_io_out, "IOVertex");
        graph.setTileMapping(vtx_out0[i], i+7);
        vtx_out1[i] = graph.addVertex(cps_io_out, "IOVertex");
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

    for (int i = 0; i < num_streams; i++) {
        db_name = "Input Stream " + std::to_string(i) + " for input 0";
        strm_in0[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col);

        db_name = "Output Stream " + std::to_string(i) + " for output 0";
        strm_out0[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col);

        db_name = "Output Stream " + std::to_string(i) + " for output 1";
        strm_out1[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col);
    }

    std::cout << "Added Streams!" << std::endl;

    // CPU Vectors
    std::vector<std::vector<float>> cpu_in0(num_streams);
    std::vector<std::vector<float>> cpu_out0(num_streams);
    std::vector<std::vector<float>> cpu_out1(num_streams);

    std::vector<float> temp_vec(row*col);

    for (int i = 0; i < num_streams; i++) {
        cpu_in0[i] = temp_vec;
        
        cpu_out0[i] = temp_vec;
        cpu_out1[i] = temp_vec;
    }

    std::cout << "Adding Programs..." << std::endl;

    /* Stream Inputs Programs */

    int prog_idx = 0;

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(strm_in0[i], v_io_in0[i]));

        seq.add(poplar::program::Execute(cps_io_in));

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

    progs[Progs::CONSUMPTION_TASK] = seq;

    /* Stream Outputs Programs */

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Execute(cps_io_out));

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

    int id = 0;

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        //std::cout << "THREAD ID " << std::to_string(thread_id) << std::endl;

        if(thread_id < num_streams) {
            for (int a = 0; a < num_packets; a++) {
                while(data_ready_flags[thread_id]) {}
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

                for (int i = 0; i < row; i++) {
                    for (int j = 0; j < col; j++) {
                        cpu_in0[thread_id][j+(col*i)] = distribution(gen);
                    }
                }

                printMatrix("GenMatrix", cpu_in0[thread_id], col, thread_id, a);
                data_ready_flags[thread_id] = true;
            }
        }
        else {

            for (int a = 0; a < num_packets; a++) {

                id = thread_id-num_streams;
                while(!data_ready_flags[id]) {}

                #pragma omp critical
                {
                engine.run(id);
                engine.run(num_streams+id);
                engine.run((num_streams*2)+id);
                }

                printMatrix("QMatrix", cpu_out0[id], col, id, a);
                printMatrix("RMatrix", cpu_out1[id], col, id, a);
                data_ready_flags[id] = false;
            }
        }
    }

    return;
}

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices) {

//}