#include "firehose_ipu.hpp"

enum Progs {
    STREAM_INPUTS,
    CONSUMPTION_TASK,
    STREAM_RESULTS,
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

//void buildFlagProgram(poplar::Graph& g, std::vector<poplar::program::Program>& progs) {
    //auto seq = poplar::program::Sequence();

    //seq.add(poplar::program::Copy(flag_strm, ready_flag));
    //seq.add(poplar::program::Copy(elements_strm, num_elements));

    //progs[INIT_FLAGS] = seq;
//}

//void buildIOInProgram(poplar::Graph& g, std::vector<poplar::program::Program>& progs, int num_transfers) {

    //auto seq = poplar::program::Sequence();

    //for(int i = 0; i < num_transfers; i++) {
        //seq.add(poplar::program::Copy(source_stream, source_tensor));
    //}

    //progs[STREAM_INPUTS] = seq;

    //seq = poplar::program::Sequence();

    //graph.connect(v["strm_in"], source_tensor);
    //graph.connect(v["ready_flag"], ready_flag);
    //graph.connect(v["num_elements"], )
    //graph.connect(v["strm_out"], result);
//}

//Add tensor arguments
//void buildCTTensorDecompProgram(poplar::Graph& g, std::vector<poplar::program::Program>& progs) {

    //poplin::addCodelets(g);
    //auto qr_out = poplin::qr();
    //seq.add(poplar::program::Copy(mult_out,tensor));
    //Connect to codely that does anomaly detection
    //auto anomalyCS = g.addComputeSet("anomalyCS");

    //for (unsigned i = 5; i < 100; ++i){
        //auto v = g.addVertex(anomalyCS,);
        //g.setTileMapping(v, i);
    //}

//}

//poplar::Engine createEngine(poplar::Device& device, poplar::Executable& exe) {
  //poplar::Engine engine(std::move(exe));
  //engine.load(device);

  //return engine;
//}

//void addStreamToEngine(poplar::Engine& engine, std::string name, std::vector<std::vector<float>>& cpu_vectors) {
    //for(int i = 0; i < max; i++) {
        //engine.connectStream(names[i], cpu_vectors[i].data());
    //}
//}

//void runProgsOnEngine(poplar::Engine& engine) {
    //engine.run(STREAM_INPUTS);
    //engine.run(CONSUMPTION_TASK);
    //engine.run(STREAM_RESULTS);
//}

//void executeTensorDecomp(poplar::Graph& g, std::vector<poplar::program::Program>& progs) {

    //buildStreamPrograms(g, progs, num_transfers, packet_size);
    //buildConsumptionTaskProgram(g, progs);

    //poplar::compileGraph(g, progs);

//}

void frontEnd_TensorDecomp(bool& flag, int& rows, int& cols, int& exp_size, std::vector<float>& cpu_input0, std::vector<float>& cpu_output0, std::vector<float>& cpu_output1) {

    /* Create data to input into back-end */
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            cpu_input0[j+(cols*i)] = distribution(gen);
        }
    }

    flag = true;
    /* Loop to create multiple matrices and decompose */
    for (int i = 0; i < exp_size; i++) {
        
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                cpu_input0[j+(cols*i)] = distribution(gen);
            }
        }

        while(flag) {}
        printMatrix("QMatrix", cpu_output0, cols);
        printMatrix("RMatrix", cpu_output1, cols);
        sleep(1);
    }
}

void backEnd_TensorDecomp(poplar::Engine& engine, bool& flag, int& exp_size) {

    for (int i = 0; i < exp_size; i++) {
        while(!flag) {}
        flag = false;
        engine.run(STREAM_INPUTS);
        engine.run(CONSUMPTION_TASK);
        engine.run(STREAM_RESULTS);
    }
}

void tensorDecomp() {
    
    // Get an IPU Device
    auto manager = poplar::DeviceManager::createDeviceManager();
    auto device = manager.acquireAvailableDevice(1);

    /* Expose Shared Memory */

    // Graph
    poplar::Graph graph(device.getTarget());

    // Programs
    std::vector<poplar::program::Program> progs;

    // Flags
    bool flag = false;

    // Parameters
    long unsigned int rows = 3;
    long unsigned int cols = 3;
    long unsigned int packet_size = 9;
    long unsigned int num_transfers = (rows*cols) /packet_size;
    long unsigned int exp_size = 3;

    // Tensors
    auto input_tensor0 = graph.addVariable(poplar::FLOAT, {packet_size}, "Input Tensor 0");
    auto consumption_tensor_in0 = graph.addVariable(poplar::FLOAT, {rows, cols}, "Consumption Task Input 0");
    auto consumption_tensor_out0 = graph.addVariable(poplar::FLOAT, {rows, cols}, "Consumption Task Output 0");
    auto consumption_tensor_out1 = graph.addVariable(poplar::FLOAT, {rows, cols}, "Consumption Task Output 1");
    auto output_tensor0 = graph.addVariable(poplar::FLOAT, {packet_size}, "Output Tensor 0");
    auto output_tensor1 = graph.addVariable(poplar::FLOAT, {packet_size}, "Output Tensor 1");

    auto identity_tensor = graph.addConstant(poplar::FLOAT, {rows, cols}, {1, 0, 0, 0, 1, 0, 0, 0, 1}, "Output Tensor 1");

    poputil::mapTensorLinearly(graph, input_tensor0);
    poputil::mapTensorLinearly(graph, consumption_tensor_in0);
    poputil::mapTensorLinearly(graph, consumption_tensor_out0);
    poputil::mapTensorLinearly(graph, consumption_tensor_out1);
    poputil::mapTensorLinearly(graph, output_tensor0);
    poputil::mapTensorLinearly(graph, output_tensor1);

    // Vertices
    auto input_io0 = graph.addVertex(graph.getDefaultComputeSet(), "IO Input Vertex 0");
    auto output_io0 = graph.addVertex(graph.getDefaultComputeSet(), "IO Output Vertex 0");
    auto output_io1 = graph.addVertex(graph.getDefaultComputeSet(), "IO Output Vertex 1");

    graph.setTileMapping(input_io0, 3);
    graph.setTileMapping(output_io0, 4);
    graph.setTileMapping(output_io1, 5);



    // Streams
    auto input_strm0 = graph.addHostToDeviceFIFO("Input Stream 0", poplar::FLOAT, packet_size);
    auto output_strm0 = graph.addDeviceToHostFIFO("Output Stream 0", poplar::FLOAT, packet_size);
    auto output_strm1 = graph.addDeviceToHostFIFO("Output Stream 1", poplar::FLOAT, packet_size);

    // Misc
    //auto ready_flag = graph.addVariable(poplar::INT, {1}, "Ready Flag");
    //auto num_elements = graph.addVariable(poplar::INT, {1}, "Number of elements");

    //poputil::mapTensorLinearly(graph, ready_flag);
    //poputil::mapTensorLinearly(graph, num_elements);

    // CPU Vectors
    std::vector<float> cpu_input0(rows*cols);
    std::vector<float> cpu_output0(rows*cols);
    std::vector<float> cpu_output1(rows*cols);

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add(poplar::program::Copy(input_strm0, input_tensor0));
    }

    progs[STREAM_INPUTS] = seq;

    graph.connect(input_io0["strm_in"], input_tensor0);
    graph.connect(input_io0["strm_out"], consumption_tensor_in);

    /* Consumption Task Program */

    seq = poplar::program::Sequence();

    poplin::addCodelets(graph);

    auto con_out = poplin::experimental::QRFactorization(graph, consumption_tensor_in, identity_tensor, seq);

    progs[CONSUMPTION_TASK] = seq;

    /* Stream Outputs Program */

    seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add(poplar::program::Copy(output_tensor0, output_strm0));
        seq.add(poplar::program::Copy(output_tensor1, output_strm1));
    }

    progs[STREAM_INPUTS] = seq;

    graph.connect(output_io0["strm_in"], consumption_tensor_out0);
    graph.connect(output_io0["strm_out"], output_tensor0);

    graph.connect(output_io0["strm_in"], consumption_tensor_out1);
    graph.connect(output_io0["strm_out"], output_tensor1);

    auto exe = poplar::compileGraph(graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(device);

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            frontEnd_TensorDecomp(flag, rows, cols, exp_size, cpu_input0, cpu_output0, cpu_output1);
        }

        #pragma omp section
        {
            backEnd_TensorDecomp(engine, flag, exp_size);
        }
    }
}