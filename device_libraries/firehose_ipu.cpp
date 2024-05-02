#include "firehose_ipu.hpp"

#define NUM_PROGRAMS 3
#define PRODUCER 0
#define CONSUMER 1

enum HARDWARE {IPU, MODEL, CPU};
enum MAPPING {LINEAR, SET};
enum LAYERS {INPUT, CONSUMPTION, OUTPUT};
enum IO {IN, OUT};



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

void createIdentityMatrix(std::vector<float>& vec_id, int row, int col) {
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
}


poplar::Device getDevice(int hw_mode, int num_devices) {
    std::cout << "Getting Device..." << std::endl;

    auto manager = poplar::DeviceManager::createDeviceManager();
    std::vector<poplar::Device> hwDevices;

    switch(hw_mode) {
        case HARDWARE::IPU:
            hwDevices = manager.getDevices(poplar::TargetType::IPU, num_devices);
            break;

        case HARDWARE::MODEL:
            hwDevices = manager.getDevices(poplar::TargetType::IPU_MODEL, num_devices);
            break;

        case HARDWARE::CPU:
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

void buildLayer(poplar::Graph& graph, model& myModel, std::pair<int,int> params, int layer_id, int map, int num_tensors) {

    std::string db_name;
    layer myLayer;

    for(int i = 0; i < num_tensors; i++) {
        db_name = "Layer " + std::to_string(layer_id) + " Tensor " + std::to_string(i);
        myLayer.tensors.push_back(graph.addVariable(poplar::FLOAT, {params.first, params.second}, db_name));

        switch(map) {
        case MAPPING::LINEAR:
            poputil::mapTensorLinearly(graph, myLayer.tensors[i]);
            break;
        case MAPPING::SET:
            graph.setTileMapping(myLayer.tensors[i], i);
            break;
        default:
            poputil::mapTensorLinearly(graph, myLayer.tensors[i]);
            std::cout << "WARNING: DEFAULTED IN buildLayer()" << std::endl;
            break;
        }
    }

    // TO DO: Overload assignment operator
    myModel.layers.push_back(myLayer);

    return;
}

void addComputeSet(poplar::Graph& graph, std::vector<poplar::ComputeSet>& cps, int num_streams, int IO) {

    std::string db_name;
    std::string title;
    
    switch(IO) {
        case 0:
            title = "Input Compute Set for Pipeline ";
            break;
        case 1:
            title = "Output Compute Set for Pipeline ";
            break;
        default:
            title = "Input Compute Set for Pipeline ";
            std::cout << "WARNING: DEFAULTED IN addComputeSet()" << std::endl;
            break;
    }

    for (int i = 0; i < num_streams; i++) {
        db_name = title + std::to_string(i);
        cps[i] = graph.addComputeSet(db_name);
    }
}

void addStream(poplar::Graph& graph, std::vector<poplar::DataStream>& strm, std::pair<int,int> params, int buf_depth, int num_port, int num_streams, int IO) {

    std::string db_name;
    std::string title;
    std::string port;

    poplar::OptionFlags streamOpts {
      {"bufferingDepth", std::to_string(buf_depth)},
    };
    
    switch(IO) {
        case IO::IN:
            title = "Input Stream ";
            port = "for input ";
            break;
        case IO::OUT:
            title = "Output Stream ";
            port = "for output  ";
            break;
        default:
            title = "Input Stream ";
            port = "for input ";
            std::cout << "WARNING: DEFAULTED IN addStream()" << std::endl;
            break;
    }

    switch(IO) {
        case IO::IN:
            
            for (int i = 0; i < num_streams; i++) {
                db_name = title + std::to_string(i) + port + std::to_string(num_port);
                strm[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, params.first*params.second, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);
            }
            break;

        case IO::OUT:

            for (int i = 0; i < num_streams; i++) {
                db_name = title + std::to_string(i) + port + std::to_string(num_port);
                strm[i] = graph.addDeviceToHostFIFO(db_name, poplar::FLOAT, params.first*params.second, streamOpts);
            }
            break;

    
    }

    for (int i = 0; i < num_streams; i++) {
        db_name = title + std::to_string(i) + port + std::to_string(num_port);
        strm.in0[i] = graph.addHostToDeviceFIFO(db_name, poplar::FLOAT, row*col, poplar::ReplicatedStreamMode::REPLICATE, streamOpts);
    }
}

void addVertex(poplar::Graph& graph, std::vector<poplar::VertexRef>& vtx, int offset) {

    std::string vtx_name = "IOVertex";

    for (int i = 0; i < num_streams; i++) {

        vtx[i] = graph.addVertex(vtx[i], "IOVertex");
        graph.setTileMapping(vtx[i], i+offset);
    }

}

void connectVertex(poplar::Graph& graph, std::vector<poplar::VertexRef>& vtx, std::vector<model>& myModels, int top_layer, int bottom_layer, int top_tensor, int bottom_tensor, std::string in, std::string out) {
    for(int i = 0; i < num_streams; i++) {
        graph.connect(vtx[i][in], myModels[i].layers[top_layer].tensors[top_tensor]);
        graph.connect(vtx[i][out], myModels[i].layers[bottom_layer].tensors[bottom_tensor]);
    }
}

void connectEngineStream(poplar::Graph& graph, std::vector<float>& cpu, int num_streams, int num_port, int IO) {

    std::string db_name;
    std::string title;
    
    switch(IO) {
        case IO::IN:
            title = "Input Stream ";
            port = "for input ";
            break;
        case IO::OUT:
            title = "Output Stream ";
            port = "for output  ";
            break;
        default:
            title = "Input Stream ";
            port = "for input ";
            std::cout << "WARNING: DEFAULTED IN addStream()" << std::endl;
            break;
    }

    for (int i = 0; i < num_streams; i++) {
        db_name = title + std::to_string(i) + port + std::to_string(num_port);
        engine.connectStream(db_name, cpu[i].data(), cpu[i].data() + cpu[i].size());
    }
}

void buildTensorTemplateTRIANGLEUP(poplar::Graph& graph, std::vector<model>& myModels, std::pair<int,int> params, int num_streams) {
    std::cout << "Building Model..." << std::endl;
    model myModel;

    buildLayer(graph, myModel, params, 0, MAPPING::LINEAR, 1);
    buildLayer(graph, myModel, params, 1, MAPPING::LINEAR, 2);
    buildLayer(graph, myModel, params, 2, MAPPING::LINEAR, 2);

    // Duplicate Model (Still copied to vector even if there is no copy)
    // TO DO: Overload assignment operator
    for(int i = 0; i < num_streams; i++) {
        myModels[i] = myModel;
    }
    std::cout << "Built Model!" << std::endl;

    return myModels;
}

void buildIOTemplateTRIANGLEUP(poplar::Graph& graph, std::vector<model>& myModels, comPatternTriangleUP& comPat, std::pair<int,int> params, int num_streams) {

    std::cout << "Adding Vertices..." << std::endl;
    
    comPat.cps.in = std::vector<poplar::ComputeSet> tempCS(num_streams);
    comPat.cps.out = std::vector<poplar::ComputeSet> tempCS(num_streams);

    comPat.vtx.in0 = std::vector<poplar::VertexRef> tempVTX(num_streams);
    comPat.vtx.out0 = std::vector<poplar::VertexRef> tempVTX(num_streams);
    comPat.vtx.out1 = std::vector<poplar::VertexRef> tempVTX(num_streams);

    comPat.strm.in0 = std::vector<poplar::DataStream> tempDS(num_streams);
    comPat.strm.out0 = std::vector<poplar::DataStream> tempDS(num_streams);
    comPat.strm.out1 = std::vector<poplar::DataStream> tempDS(num_streams);


    addComputeSet(graph, comPat.cps.in, num_streams, IO::IN);
    addComputeSet(graph, comPat.cps.out, num_streams, IO::OUT);

    addVertex(graph, comPat.vtx.in0, num_streams, 5);
    addVertex(graph, comPat.vtx.out0, num_streams, 7);
    addVertex(graph, comPat.vtx.out1, num_streams, 9)

    std::string in = "strm_in";
    std::string out = "strm_out";

    connectVertex(graph, comPat.vtx.in0, myModels, LAYERS::INPUT, LAYERS::CONSUMPTION, 0, 0, in, out);
    connectVertex(graph, comPat.vtx.out0, myModels, LAYERS::CONSUMPTION, LAYERS::OUTPUT, 0, 0, in, out);
    connectVertex(graph, comPat.vtx.out1, myModels, LAYERS::CONSUMPTION, LAYERS::OUTPUT, 1, 1, in, out);

    std::cout << "Added Vertices!" << std::endl;

    // Streams
    std::cout << "Adding Streams..." << std::endl;

    addStream(graph, comPat.strm.in0, params, 2, 0, num_streams, IO::IN);
    addStream(graph, comPat.strm.out0, params, 2, 0, num_streams, IO::OUT);
    addStream(graph, comPat.strm.out1, params, 2, 1, num_streams, IO::OUT);

    std::cout << "Added Streams!" << std::endl;

}

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file) {

    /* Create Shared Memory */

    // Strings
    std::string db_name;

    // Programs
    std::vector<poplar::program::Program> progs(num_streams);

    // Flags
    bool data_ready_flags[num_streams];

    for (int i = 0; i < num_streams; i++) {
        data_ready_flags[i] = false;
    }

    /* Get Program Context */
    
    // Get Device
    std::cout << "Getting Device..." << std::endl;
    poplar::Device device = getDevice(0, num_devices);
    std::cout << "Got Device!" << std::endl;

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    /* Build Graph */

    // Build Model
    std::vector<model> myModels;
    std::pair<int,int> myParams = std::make_pair(row, col);

    std::vector<model> myModels = buildTensorTemplateTRIANGLEUP(graph, myModels, myParams, num_streams);

    // Add Variable Tensors
    // Not necessary

    // Constant Tensors
    std::cout << "Adding Constant Tensors..." << std::endl;

    std::vector<float> vec_id;
    createIdentityMatrix(vec_id, row, col);

    poplar::Tensor c_id = graph.addConstant<float>(poplar::FLOAT, {row, col}, vec_id.data(), "Constant Identity Tensor");
    poputil::mapTensorLinearly(graph, c_id);

    std::cout << "Added Constant Tensors!" << std::endl;

    /* Add Codelets */

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);
    poplin::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    comPatternTriangleUP comPat;

    buildIOTemplateTRIANGLEUP(graph, myModels, comPat, myParams, num_streams)

    /* Programs */

    std::cout << "Adding Programs..." << std::endl;

    int prog_idx = 0;

    poplar::program::Sequence seq;

    for(int i = 0; i < num_streams; i++) {

        // Begin Sequence 
        seq = poplar::program::Sequence();

        // Stream Inputs Programs

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(comPat.strm.in0[i], myModels[i].layers[LAYERS::INPUT].tensors[0]));

        seq.add(poplar::program::Execute(comPat.cps.in[i]));

        // Consumption Task Programs

        seq.add(poplar::program::Copy(c_id, myModels[i].layers[LAYERS::CONSUMPTION].tensors[1]));

        poplin::experimental::QRFactorization(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0], myModels[i].layers[LAYERS::CONSUMPTION].tensors[1], seq);

        // Stream Outputs Programs

        seq.add(poplar::program::Execute(comPat.cps.out[i]));

        seq.add(poplar::program::Copy(myModels[i].layers[LAYERS::OUTPUT].tensors[0], comPat.strm.out0[i]));
        seq.add(poplar::program::Copy(myModels[i].layers[LAYERS::OUTPUT].tensors[1], comPat.strm.out1[i]));

        // End Sequence
        progs[prog_idx++] = seq;
    }

    std::cout << "Added Programs!" << std::endl;

    /* Create and Load Engine */

    std::cout << "Loading Device..." << std::endl;

    auto exe = poplar::compileGraph(graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(device);

    std::cout << "Loaded Device!" << std::endl;

    /* CPU Memory */

    // CPU Vectors
    struct {
        std::vector<std::vector<float>> in0(num_streams, std::vector<float> (row*col, 5.0));
        std::vector<std::vector<float>> out0(num_streams, std::vector<float> (row*col, 5.0));
        std::vector<std::vector<float>> out1(num_streams, std::vector<float> (row*col, 5.0));
    } cpu;

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    connectEngineStream(graph, cpu.in0, num_streams, 0, IO::IN);
    connectEngineStream(graph, cpu.out0, num_streams, 0, IO::OUT);
    connectEngineStream(graph, cpu.out1, num_streams, 1, IO::OUT);

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        
        std::mt19937 gen(seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < row*col; i++) {
                        cpu.in0[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    printMatrix("GenMatrix", cpu.in0[rel_id], col, rel_id, packet, 0);

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("QMatrix", cpu.out0[rel_id], col, rel_id, packet, 1);
                        printMatrix("RMatrix", cpu.out1[rel_id], col, rel_id, packet, 1);
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
    std::vector<poplar::program::Program> progs(num_streams);

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