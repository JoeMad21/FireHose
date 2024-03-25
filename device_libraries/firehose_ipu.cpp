#include "firehose_ipu.hpp"

enum Progs {
    STREAM_INPUTS,
    //ALIGN_INPUTS,
    CONSUMPTION_TASK,
    //ALIGN_OUTPUTS,
    STREAM_OUTPUTS,
    NUM_PROGRAMS
};

void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols) {
  std::cout << matrix_name << std::endl;

  for (int i = 0; i < matrix.size(); i++) {

    std::cout << std::fixed << matrix[i] << "\t";
    
    if ( (i+1)%cols == 0) {
      std::cout << std::endl;
    }

  }

  std::cout << std::endl;

}

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_streams) {

    /* Get an IPU Device */

    std::cout << "Getting Device..." << std::endl;

    auto manager = poplar::DeviceManager::createDeviceManager();
    auto hwDevices = manager.getDevices(poplar::TargetType::IPU, 1);
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
    std::vector<poplar::program::Program> progs(Progs::NUM_PROGRAMS);

    // Flags
    bool data_ready_flags[num_streams];

    for (int i = 0; i < num_streams; i++) {
        data_ready_flags[i] = false;
    }

    // Parameters
    long unsigned int packet_size = row*col;
    long unsigned int num_transfers = (row*col) / packet_size;
    long unsigned int exp_size = 1;

    // Variable Tensors
    std::cout << "Adding Tensors..." << std::endl;

    std::vector<poplar::Tensor> v_io_in0(num_streams);
    std::vector<poplar::Tensor> v_con0(num_streams);
    //std::vector<poplar::Tensor> v_con_in0(num_streams);
    //std::vector<poplar::Tensor> v_con_in_exp0(num_streams);
    //std::vector<poplar::Tensor> v_con_out0(num_streams);
    std::vector<poplar::Tensor> v_io_out0(num_streams);

    std::vector<poplar::Tensor> v_con1(num_streams);
    std::vector<poplar::Tensor> v_io_out1(num_streams);

    std::string db_name;

    for (int i = 0; i < num_streams; i++) {

        /* Input to QR Factorization */
        db_name = "Input Tensor " + std::to_string(i) + " of Set 0";
        v_io_in0[i] = graph.addVariable(poplar::FLOAT, {packet_size}, db_name);
        poputil::mapTensorLinearly(graph, v_io_in0[i]);

        db_name = "Consumption Tensor " + std::to_string(i) + " of Set 0";
        v_con0[i] = graph.addVariable(poplar::FLOAT, {packet_size}, db_name);
        poputil::mapTensorLinearly(graph, v_con0[i]);

        //db_name = "Consumption Task Input " + std::to_string(i);
        //v_con_in0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        //poputil::mapTensorLinearly(graph, v_con_in0[i]);

        //db_name = "Consumption Task Input Expanded " + std::to_string(i);
        //in_con_exp0[i] = graph.addVariable(poplar::FLOAT, {rows, cols}, db_name);
        //poputil::mapTensorLinearly(graph, in_con_exp0[i]);

        //db_name = "Consumption Tensor " + std::to_string(i) + " of Set 1";
        //v_con1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        //poputil::mapTensorLinearly(graph, v_con1[i]);

        db_name = "Output Tensor " + std::to_string(i) + " of Set 0";
        v_io_out0[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_io_out0[i]);

        /* Necessary Identity to QR Factorization */
        db_name = "Consumption Tensor" + std::to_string(i) + " of Set 1";
        v_con1[i] = graph.addVariable(poplar::FLOAT, {row, col}, db_name);
        poputil::mapTensorLinearly(graph, v_con1[i]);

        db_name = "Output Tensor " + std::to_string(i) + " of Set 1";
        v_io_out1[i] = graph.addVariable(poplar::FLOAT, {packet_size}, db_name);
        poputil::mapTensorLinearly(graph, v_io_out1[i]);
    }

    // Constant Tensors
    std::vector<float> temp_vec_id(col);
    std::vector<std::vector<float>> vec_id;

    for (int i = 0; i < row; i++) {
        vec_id.push_back(temp_vec_id);
    }


    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            if (i == j) {
                vec_id[i][j] = 1.0;
            }
            else {
                vec_id[i][j] = 0.0;
            }
        }
    }

    auto c_id = graph.addConstant<float>(poplar::FLOAT, {row, col}, vec_id);

    std::cout << "Added Tensors!" << std::endl;

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet_in.gp");
    graph.addCodelets("./device_libraries/io_codelet_out.gp");

    std::cout << "Added Codelets!" << std::endl;

    // Vertices
    std::cout << "Adding Vertices..." << std::endl;

    auto cps_io_in = graph.addComputeSet("IO in CS");
    auto cps_io_out = graph.addComputeSet("IO out CS");

    std::vector<poplar::VertexRef> vtx_in0(num_streams);
    std::vector<poplar::VertexRef> vtx_out0(num_streams);
    std::vector<poplar::VertexRef> vtx_out1(num_streams);

    for (int i = 0; i < num_streams; i++) {

        vtx_in0[i] = graph.addVertex(cps_io_in, "IOVertexIN");
        graph.setTileMapping(vtx_in0[i], i+5);

        vtx_out0[i] = graph.addVertex(cps_io_out, "IOVertexOUT");
        graph.setTileMapping(vtx_out0[i], i+7);
        vtx_out1[i] = graph.addVertex(cps_io_out, "IOVertexOUT");
        graph.setTileMapping(vtx_out1[i], i+9);
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

        db_name = "Output Stream " + std::to_string(i) + " in output 1";
        strm_out1[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, row*col);
    }

    std::cout << "Added Streams!" << std::endl;

    // Misc
    std::vector<std::size_t> dimShape = {row, col};
    std::vector<std::size_t> flatShape = {row*col};

    // CPU Vectors
    std::vector<std::vector<float>> cpu_in0(num_transfers);
    std::vector<std::vector<float>> cpu_out0(num_transfers);
    std::vector<std::vector<float>> cpu_out1(num_transfers);

    std::vector<float> temp_vec(packet_size);

    for (int i = 0; i < num_transfers; i++) {
        cpu_in0[i] = temp_vec;
        
        cpu_out0[i] = temp_vec;
        cpu_out1[i] = temp_vec;
    }

    /* Stream Inputs Program */

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add(poplar::program::Copy(strm_in0[i], v_io_in0[i]));
    }

    seq.add(poplar::program::Execute(cps_io_in));

    progs[Progs::STREAM_INPUTS] = seq;

    /* Align Consumption Inputs Program */

    //seq = poplar::program::Sequence();

    //seq.add(poplar::program::Copy(in_con.reshape(dimShape), in_con_exp0));
    //seq.add(poplar::program::Copy(in_con.reshape(dimShape), in_con_exp0));

    //progs[Progs::ALIGN_INPUTS] = seq;

    /* Consumption Task Program */

    seq = poplar::program::Sequence();

    poplin::addCodelets(graph);

    for(int i = 0; i < num_transfers; i++) {

        seq.add(poplar::program::Copy(c_id, v_con1));

        poplin::experimental::QRFactorization(graph, v_con0[i], v_con1[i], seq);

        //seq.add(poplar::program::Copy(v_con_in0[i], v_con_out0[i]));
        //seq.add(poplar::program::Copy(v_con_in0[i], v_con_out0[i]));
    }

    progs[Progs::CONSUMPTION_TASK] = seq;

    /* Stream Outputs Program */

    seq = poplar::program::Sequence();

    seq.add(poplar::program::Execute(cps_io_out));

    for(int i = 0; i < num_transfers; i++) {
        seq.add(poplar::program::Copy(v_io_out0[i], strm_out0[i]));
        seq.add(poplar::program::Copy(v_io_out1[i], strm_out1[i]));
    }

    progs[Progs::STREAM_OUTPUTS] = seq;

    /* Create and Load Engine */

    auto exe = poplar::compileGraph(graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(device);

    /* Connect Vertices (Edges)*/

    for(int i = 0; i < num_transfers; i++) {
        graph.connect(vtx_in0[0]["strm_in"], v_io_in0[0]);
        graph.connect(vtx_in0[0]["strm_out"], v_con0[0]);

        graph.connect(vtx_out0[0]["strm_in"], v_con0[0]);
        graph.connect(vtx_out0[0]["strm_out"], v_io_out0[0]);

        graph.connect(vtx_out1[0]["strm_in"], v_con1[0]);
        graph.connect(vtx_out1[0]["strm_out"], v_io_out1[0]);
    }

    /* Connect Streams */

    for (int i = 0; i < num_streams; i++) {
        engine.connectStream("Input Stream 0", cpu_in0[i].data(), cpu_in0[i].data() + cpu_in0[i].size());
        engine.connectStream("Output Stream 0", cpu_out0[i].data(), cpu_out0[i].data() + cpu_out0[i].size());
        engine.connectStream("Output Stream 1", cpu_out1[i].data(), cpu_out1[i].data() + cpu_out1[i].size());
    }

    std::cout << "Loaded Device" << std::endl;

    /* Run Parallel Threads for FireHose */

    for (int i = 0; i < 5; i++) {
    #pragma omp parallel sections
    {
        #pragma omp section
        {
            while(data_ready_flags[0]) {}
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

            for (int i = 0; i < row; i++) {
                for (int j = 0; j < col; j++) {
                    cpu_input0[j+(col*i)] = distribution(gen);
                }
            }
            printMatrix("GenMatrix", cpu_in0[0], col);
            data_ready_flags[0] = true;
        }

        #pragma omp section
        {
            while(!data_ready_flags[0]) {}
            data_ready_flags[0] = false;
            engine.run(Progs::STREAM_INPUTS);
            //engine.run(Progs::ALIGN_INPUTS);
            engine.run(Progs::CONSUMPTION_TASK);
            //engine.run(Progs::ALIGN_OUTPUTS);
            engine.run(Progs::STREAM_OUTPUTS);

            printMatrix("QMatrix", cpu_out0[0], col);
            printMatrix("RMatrix", cpu_out1[0], col);
        }
    }
    }
    return;
}