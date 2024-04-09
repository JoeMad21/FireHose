#include "firehose_ipu.hpp"

#define num_programs 3  

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
  //std::cout << matrix_name << " THREAD " << id << " PACKET " << packet << std::endl;

  for (int i = 0; i < matrix.size(); i++) {

    fileStream << std::fixed << matrix[i] << "\t";
    //std::cout << std::fixed << matrix[i] << "\t";
    
    if ( (i+1)%cols == 0) {
      fileStream << std::endl;
      //std::cout << std::endl;
    }

  }

  fileStream << std::endl;
  fileStream.close();
  //std::cout << std::endl;

}

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, bool get_from_file) {

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
    std::vector<poplar::program::Program> progs(num_streams*num_programs);

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

        db_name = "v_io_in[" + std::to_string(i) + "]";
        //seq.add(poplar::program::PrintTensor(db_name, v_io_in0[i]));

        db_name = "v_con0[" + std::to_string(i) + "]";
        //seq.add(poplar::program::PrintTensor(db_name, v_con0[i]));

        progs[prog_idx++] = seq;
    }

    /* Consumption Task Programs */

    poplin::addCodelets(graph);

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Copy(c_id, v_con1[i]));

        db_name = "v_con1[" + std::to_string(i) + "]";
        //seq.add(poplar::program::PrintTensor(db_name, v_con1[i]));

        poplin::experimental::QRFactorization(graph, v_con0[i], v_con1[i], seq);

        db_name = "v_con0[" + std::to_string(i) + "] (After)";
        //seq.add(poplar::program::PrintTensor(db_name, v_con0[i]));

        db_name = "v_con1[" + std::to_string(i) + "] (After)";
        //seq.add(poplar::program::PrintTensor(db_name, v_con0[i]));

        progs[prog_idx++] = seq;
    }

    //progs[Progs::CONSUMPTION_TASK] = seq;

    /* Stream Outputs Programs */

    for(int i = 0; i < num_streams; i++) {

        seq = poplar::program::Sequence();

        seq.add(poplar::program::Execute(cps_io_out[i]));

        db_name = "v_io_out0[" + std::to_string(i) + "]";
        //seq.add(poplar::program::PrintTensor(db_name, v_io_out0[i]));

        db_name = "v_io_out1[" + std::to_string(i) + "]";
        //seq.add(poplar::program::PrintTensor(db_name, v_io_out1[i]));

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

    //if(get_from_file) {
        //auto source = distribution;
    //}


    //}

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int gbl_id = (int) thread_id;
        int snd_id = (int) thread_id;
        int rcv_id = thread_id-num_streams;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        std::string fileName = "/home/jomad21/myFiles/FireHose/input" + std::to_string(snd_id) + ".mtx";

        std::ifstream file(fileName);
        std::string line;


        if(gbl_id < num_streams) {
            for (int a = 0; a < num_packets; a++) {
                while(data_ready_flags[snd_id]) {}

                if (!get_from_file) {
                    std::cout << "AAA" << std::endl;
                    for (int i = 0; i < row*col; i++) {
                        cpu_in0[snd_id][i] = distribution(gen);
                    }
                    std::cout << "BBB" << std::endl;
                }
                else {
                    std::getline(file, line);
                    std::istringstream iss(line);
                    float value;
        
                    // Split the line into floats
                    int i = 0;
                    while (iss >> value) {
                        cpu_in0[snd_id][i++] = value;
                    }
                }

                #pragma omp critical(print)
                printMatrix("GenMatrix", cpu_in0[snd_id], col, snd_id, a, 0);

                data_ready_flags[snd_id] = true;

            }
        }
        else {

            for (int a = 0; a < num_packets; a++) {

                while(!data_ready_flags[rcv_id]) {}

                #pragma omp critical(ipu_work)
                {
                    engine.run(rcv_id);
                    engine.run(num_streams+rcv_id);
                    engine.run((num_streams*2)+rcv_id);
                }

                #pragma omp critical(print)
                {
                    printMatrix("QMatrix", cpu_out0[rcv_id], col, rcv_id, a, 1);
                    printMatrix("RMatrix", cpu_out1[rcv_id], col, rcv_id, a, 1);
                }

                data_ready_flags[rcv_id] = false;
            }
        }

        file.close();
    }

    return;
}

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices) {

//}