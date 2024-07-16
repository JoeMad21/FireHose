#include "firehose_ipu.hpp"

#define NUM_PROGRAMS 3
#define PRODUCER 0
#define CONSUMER 1

//#define VM_DEVICE vm["device"].as<int>()
//#define VM_ROW vm["row"].as<int>()
//#define VM_COL vm["col"].as<int>()
//#define VM_NUM_PACKETS vm["num_packets"].as<int>()
//#define VM_NUM_STREAMS vm["num_streams"].as<int>()
//#define VM_NUM_DEVICES vm["num_devices"].as<int>()
//#define vm_seed vm["seed"].as<int>()
//#define vm_get_from_file vm["get_from_file"].as<int>()
//#define vm_con_task vm["con_task"].as<int>()



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

void tensorDecomp(boost::program_options::variables_map& vm) {

    /* Create Shared Memory */

    // Programs
    std::vector<poplar::program::Program> progs(GET("streams"));

    // Flags
    //bool data_ready_flags[GET("streams")];

    //for (int i = 0; i < GET("streams"); i++) {
        //data_ready_flags[i] = false;
    //}

    
    graphConfig myGraphConfig(vm);
    graphConfig.allocateCompat(1, 2);

    /* Add Codelets */

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    myGraphConfig.addCodelet(CODELETS::POPLIN);
    myGraphConfig.addCodelet(CODELETS::POPOPS);

    // Add custom codelets
    myGraphConfig.addCustomCodelet("io_codelet.gp")

    std::cout << "Added Codelets!" << std::endl;

    /* Build Graph */

    // Build Model
    std::cout << "Building Model..." << std::endl;

    myGraphConfig.buildTemplate();

    //buildTensorTemplate(graph, myModels, myGraphConfig);

    //comPattern comPat;

    //buildIOTemplate(graph, myModels, comPat, myParams, GET("streams"), COMPATSHAPE::TRIANGLEQR, GET("hw"));
    std::cout << "Built Model!" << std::endl;

    // Constant Tensors
    std::cout << "Adding Constant Tensors..." << std::endl;

    std::vector<float> vec_id;
    createIdentityMatrix(vec_id, vm["dim"].as<long unsigned int>());

    poplar::Tensor c_id = graph.addConstant<float>(poplar::FLOAT, {vm["dim"].as<long unsigned int>(), vm["dim"].as<long unsigned int>()}, vec_id.data(), "Constant Identity Tensor");
    poputil::mapTensorLinearly(graph, c_id);

    std::cout << "Added Constant Tensors!" << std::endl;

    /* Programs */

    std::cout << "Adding Programs..." << std::endl;

    int prog_idx = 0;

    poplar::program::Sequence seq;

    for(int i = 0; i < vm["streams"].as<long unsigned int>(); i++) {

        seq = poplar::program::Sequence();

        myGraphConfig.addPipeline(seq, IO::IN);
        poplin::experimental::QRFactorization(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0], myModels[i].layers[LAYERS::CONSUMPTION].tensors[1], seq);
        myGraphConfig.addPipeline(seq, IO::OUT);

        // End Sequence
        progs[prog_idx++] = seq;
    }

    std::cout << "Added Programs!" << std::endl;

    /* Create and Load Engine */

    std::cout << "Loading Device..." << std::endl;

    auto exe = poplar::compileGraph(myGraphConfig.graph, progs);
    poplar::Engine engine(std::move(exe));
    engine.load(myGraphConfig.device);

    std::cout << "Loaded Device!" << std::endl;

    /* CPU Memory */

    // CPU Vectors
    //std::vector<std::vector<float>> cpu_in0(vm["streams"].as<long unsigned int>(), std::vector<float> (vm["dim"].as<long unsigned int>()*vm["dim"].as<long unsigned int>(), 5.0));
    //std::vector<std::vector<float>> cpu_out0(vm["streams"].as<long unsigned int>(), std::vector<float> (vm["dim"].as<long unsigned int>()*vm["dim"].as<long unsigned int>(), 5.0));
    //std::vector<std::vector<float>> cpu_out1(vm["streams"].as<long unsigned int>(), std::vector<float> (vm["dim"].as<long unsigned int>()*vm["dim"].as<long unsigned int>(), 5.0));

    std::vector< std::vector< std::vector<float>  > > cpu(2, (dim*dim, (vm["streams"].as<long unsigned int>(), 0.0)));

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    myGraphConfig.connectEngineStream(cpu, 0, IO::IN);
    myGraphConfig.connectEngineStream(cpu, 0, IO::OUT);
    myGraphConfig.connectEngineStream(cpu, 1, IO::OUT);
    
    //connectEngineStream(graph, engine, cpu_in0, vm["streams"].as<long unsigned int>(), 0, IO::IN);
    //connectEngineStream(graph, engine, cpu_out0, vm["streams"].as<long unsigned int>(), 0, IO::OUT);
    //connectEngineStream(graph, engine, cpu_out1, vm["streams"].as<long unsigned int>(), 1, IO::OUT);

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    std::mt19937 gen(GET("seed")+0);
    std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

    for (int packet = 0; packet < GET("packets"); packet++) {

        for (int i = 0; i < GET("row")*GET("col"); i++) {
            cpu[IO::IN][0][i] = distribution(gen);
        }

        printMatrix("Gen Matrix", cpu[IO::IN][0][i], GET("col"), 0, packet, 0);

        engine.run(0);

        printMatrix("QMatrix", cpu[IO::OUT][0][i], GET("col"), 0, packet, 1);
        printMatrix("RMatrix", cpu[IO::OUT][1][i], GET("col"), 0, packet, 1);
    }

    return;

    // omp_set_num_threads(GET("stream")*2);

    // #pragma omp parallel
    // {
    //     int thread_id = omp_get_thread_num();
    //     int pc_id = thread_id % 2;
    //     int rel_id = thread_id / 2;
        
    //     std::mt19937 gen(GET("seed")+rel_id);
    //     std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

    //     switch(pc_id) {
    //         case PRODUCER:
    //             for(int packet = 0; packet < GET("packets"); packet++) {
    //                 while(data_ready_flags[rel_id]);


    //                 for (int i = 0; i < GET("row")*get("col"); i++) {
    //                     cpu_in0[rel_id][i] = distribution(gen);
    //                 }

    //                 #pragma omp critical(print)
    //                 printMatrix("GenMatrix", cpu_in0[rel_id], vm_col, rel_id, packet, 0);

    //                 data_ready_flags[rel_id] = true;
    //             }
    //             break;
            
    //         case CONSUMER:
    //             for(int packet = 0; packet < vm_num_packets; packet++) {
    //                 while(!data_ready_flags[rel_id]);

    //                 #pragma omp critical(ipu_work)
    //                 {
    //                     engine.run(rel_id);
    //                 }

    //                 #pragma omp critical(print)
    //                 {
    //                     printMatrix("QMatrix", cpu_out0[rel_id], vm_col, rel_id, packet, 1);
    //                     printMatrix("RMatrix", cpu_out1[rel_id], vm_col, rel_id, packet, 1);
    //                 }

    //                 data_ready_flags[rel_id] = false;
    //             }
    //             break;
    //     }
    // }

    // return;
}

void matMul(boost::program_options::variables_map& vm) {

    /* Create Shared Memory */

    // Strings
    std::string db_name;

    // Programs
    std::vector<poplar::program::Program> progs(vm_num_streams);

    // Flags
    bool data_ready_flags[vm_num_streams];

    for (int i = 0; i < vm_num_streams; i++) {
        data_ready_flags[i] = false;
    }

    /* Get Program Context */
    
    // Get Device
    std::cout << "Getting Device..." << std::endl;
    poplar::Device device = getDevice(vm_device, vm_num_devices);
    std::cout << "Got Device!" << std::endl;

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    /* Build Graph */

    // Build Model
    std::vector<model> myModels;
    std::pair<int,int> myParams = std::make_pair(vm_row, vm_col);

    buildTensorTemplate(graph, myModels, myParams, vm_num_streams, COMPATSHAPE::TRIANGLEDOWN, vm_device);

    // Add Variable Tensors
    // Not necessary

    // Constant Tensors
    // Not necessary

    /* Add Codelets */

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);
    poplin::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    comPattern comPat;

    buildIOTemplate(graph, myModels, comPat, myParams, vm_num_streams, COMPATSHAPE::TRIANGLEDOWN, vm_device);

    /* Programs */

    std::cout << "Adding Programs..." << std::endl;

    int prog_idx = 0;

    poplar::program::Sequence seq;

    for(int i = 0; i < vm_num_streams; i++) {

        // Begin Sequence 
        seq = poplar::program::Sequence();

        // Stream Inputs Programs

        seq.add(poplar::program::Copy(comPat.strm.in0[i], myModels[i].layers[LAYERS::INPUT].tensors[0]));
        seq.add(poplar::program::Copy(comPat.strm.in1[i], myModels[i].layers[LAYERS::INPUT].tensors[1]));

        seq.add(poplar::program::Execute(comPat.cps.in[i]));

        // Consumption Task Programs

        poplar::Tensor matmul_out = poplin::matMul(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0], myModels[i].layers[LAYERS::CONSUMPTION].tensors[1], seq, "MatMul");

        seq.add(poplar::program::Copy(matmul_out, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0]));

        // Stream Outputs Programs

        seq.add(poplar::program::Execute(comPat.cps.out[i]));

        seq.add(poplar::program::Copy(myModels[i].layers[LAYERS::OUTPUT].tensors[0], comPat.strm.out0[i]));

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
    std::vector<std::vector<float>> cpu_in0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));
    std::vector<std::vector<float>> cpu_in1(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));
    std::vector<std::vector<float>> cpu_out0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    connectEngineStream(graph, engine, cpu_in0, vm_num_streams, 0, IO::IN);
    connectEngineStream(graph, engine, cpu_in1, vm_num_streams, 1, IO::IN);
    connectEngineStream(graph, engine, cpu_out0, vm_num_streams, 0, IO::OUT);

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    std::mt19937 gen(vm_seed+0);
    std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

    for (int packet = 0; packet < vm_num_packets; packet++) {

        for (int i = 0; i < vm_row*vm_col; i++) {
            cpu_in0[0][i] = distribution(gen);
            cpu_in1[0][i] = distribution(gen);
        }

        printMatrix("Matrix A", cpu_in0[0], vm_col, 0, packet, 0);
        printMatrix("Matrix B", cpu_in1[0], vm_col, 0, packet, 0);

        engine.run(0);

        printMatrix("Result Matrix", cpu_out0[0], vm_col, 0, packet, 1);
    }

    //omp_set_num_threads(vm_num_streams*2);

    // #pragma omp parallel
    // {
    //     int thread_id = omp_get_thread_num();
    //     int pc_id = thread_id % 2;
    //     int rel_id = thread_id / 2;
        
    //     std::mt19937 gen(vm_seed+rel_id);
    //     std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

    //     switch(pc_id) {
    //         case PRODUCER:
    //             for(int packet = 0; packet < vm_num_packets; packet++) {
    //                 while(data_ready_flags[rel_id]);


    //                 for (int i = 0; i < vm_row*vm_col; i++) {
    //                     cpu_in0[rel_id][i] = distribution(gen);
    //                     cpu_in1[rel_id][i] = distribution(gen);
    //                 }

    //                 #pragma omp critical(print)
    //                 {
    //                     printMatrix("Matrix A", cpu_in0[rel_id], vm_col, rel_id, packet, 0);
    //                     printMatrix("Matrix B", cpu_in1[rel_id], vm_col, rel_id, packet, 0);
    //                 }

    //                 data_ready_flags[rel_id] = true;
    //             }
    //             break;
            
    //         case CONSUMER:
    //             for(int packet = 0; packet < vm_num_packets; packet++) {
    //                 while(!data_ready_flags[rel_id]);

    //                 #pragma omp critical(ipu_work)
    //                 {
    //                     engine.run(rel_id);
    //                 }

    //                 #pragma omp critical(print)
    //                 {
    //                     printMatrix("Result Matrix", cpu_out0[rel_id], vm_col, rel_id, packet, 1);
    //                 }

    //                 data_ready_flags[rel_id] = false;
    //             }
    //             break;
    //     }
    // }

    return;
}

void matAdd(boost::program_options::variables_map& vm) {

    /* Create Shared Memory */

    // Strings
    std::string db_name;

    // Programs
    std::vector<poplar::program::Program> progs(vm_num_streams);

    // Flags
    bool data_ready_flags[vm_num_streams];

    for (int i = 0; i < vm_num_streams; i++) {
        data_ready_flags[i] = false;
    }

    /* Get Program Context */
    
    // Get Device
    

    /* Build Graph */

    buildGraph(graph, myModels, myParams, vm_num_streams, COMPATSHAPE::TRIANGLEDOWN, vm_device);

    std::cout << "Added Constant Tensors!" << std::endl;

    /* Add Codelets */

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);
    poplin::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    /* Programs */

    std::cout << "Adding Programs..." << std::endl;

    int prog_idx = 0;

    poplar::program::Sequence seq;

    for(int i = 0; i < vm_num_streams; i++) {

        // Begin Sequence 
        seq = poplar::program::Sequence();

        // Stream Inputs Programs

        seq.add(poplar::program::Copy(comPat.strm.in0[i], myModels[i].layers[LAYERS::INPUT].tensors[0]));
        seq.add(poplar::program::Copy(comPat.strm.in1[i], myModels[i].layers[LAYERS::INPUT].tensors[1]));

        seq.add(poplar::program::Execute(comPat.cps.in[i]));

        // Consumption Task Programs

        poplar::Tensor matadd_out = popops::add(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0], myModels[i].layers[LAYERS::CONSUMPTION].tensors[1], seq, "MatAdd");

        seq.add(poplar::program::Copy(matadd_out, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0]));

        // Stream Outputs Programs

        seq.add(poplar::program::Execute(comPat.cps.out[i]));

        seq.add(poplar::program::Copy(myModels[i].layers[LAYERS::OUTPUT].tensors[0], comPat.strm.out0[i]));

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
    std::vector<std::vector<float>> cpu_in0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));
    std::vector<std::vector<float>> cpu_in1(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));
    std::vector<std::vector<float>> cpu_out0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    connectEngineStream(graph, engine, cpu_in0, vm_num_streams, 0, IO::IN);
    connectEngineStream(graph, engine, cpu_in1, vm_num_streams, 1, IO::IN);
    connectEngineStream(graph, engine, cpu_out0, vm_num_streams, 0, IO::OUT);

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(vm_num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        
        std::mt19937 gen(vm_seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < vm_num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < vm_row*vm_col; i++) {
                        cpu_in0[rel_id][i] = distribution(gen);
                        cpu_in1[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("Matrix A", cpu_in0[rel_id], vm_col, rel_id, packet, 0);
                        printMatrix("Matrix B", cpu_in1[rel_id], vm_col, rel_id, packet, 0);
                    }

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < vm_num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("Result Matrix", cpu_out0[rel_id], vm_col, rel_id, packet, 1);
                    }

                    data_ready_flags[rel_id] = false;
                }
                break;
        }
    }

    return;
}

void transpose(boost::program_options::variables_map& vm) {

    /* Create Shared Memory */

    // Strings
    std::string db_name;

    // Programs
    std::vector<poplar::program::Program> progs(vm_num_streams);

    // Flags
    bool data_ready_flags[vm_num_streams];

    for (int i = 0; i < vm_num_streams; i++) {
        data_ready_flags[i] = false;
    }

    /* Get Program Context */
    
    // Get Device
    std::cout << "Getting Device..." << std::endl;
    poplar::Device device = getDevice(vm_device, vm_num_devices);
    std::cout << "Got Device!" << std::endl;

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    /* Build Graph */

    // Build Model
    std::vector<model> myModels;
    std::pair<int,int> myParams = std::make_pair(vm_row, vm_col);

    buildTensorTemplate(graph, myModels, myParams, vm_num_streams, COMPATSHAPE::LINE, vm_device);

    // Add Variable Tensors
    // Not necessary

    // Constant Tensors
    // Not necessary

    std::cout << "Added Constant Tensors!" << std::endl;

    /* Add Codelets */

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);
    poplin::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");
    graph.addCodelets("./device_libraries/transpose.gp");

    std::cout << "Added Codelets!" << std::endl;

    comPattern comPat;

    buildIOTemplate(graph, myModels, comPat, myParams, vm_num_streams, COMPATSHAPE::LINE, vm_device);

    /* Programs */

    std::cout << "Adding Programs..." << std::endl;

    int prog_idx = 0;

    poplar::program::Sequence seq;

    std::vector<poplar::ComputeSet> cps(vm_num_streams);
    std::vector<poplar::VertexRef> vtx(vm_num_streams);

    for (int i = 0; i < vm_num_streams; i++) {
        db_name = "Compute Set for Pipeline " + std::to_string(i);
        cps[i] = graph.addComputeSet(db_name);
    }

    for (int i = 0; i < vm_num_streams; i++) {

        vtx[i] = graph.addVertex(cps[i], "transposeVertex");
        graph.setTileMapping(vtx[i], !vm_device ? i+15 : 0);
        graph.setPerfEstimate(vtx[i], 40, 40);
    }

    for(int i = 0; i < vm_num_streams; i++) {
        graph.connect(vtx[i]["strm_in"], myModels[i].layers[LAYERS::CONSUMPTION].tensors[0]);
        graph.connect(vtx[i]["strm_out"], myModels[i].layers[LAYERS::OUTPUT].tensors[0]);
    }

    for(int i = 0; i < vm_num_streams; i++) {

        // Begin Sequence 
        seq = poplar::program::Sequence();

        // Stream Inputs Programs

        seq.add(poplar::program::Copy(comPat.strm.in0[i], myModels[i].layers[LAYERS::INPUT].tensors[0]));

        seq.add(poplar::program::Execute(comPat.cps.in[i]));

        // Consumption Task Programs

        //poplar::Tensor transpose_out = popops::rearrange::partialTranspose(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0], cps[i], "Tranpose");

        //seq.add(poplar::program::Copy(transpose_out, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0]));

        seq.add(poplar::program::Execute(cps[i]));

        // Stream Outputs Programs

        //seq.add(poplar::program::Execute(comPat.cps.out[i]));

        seq.add(poplar::program::Copy(myModels[i].layers[LAYERS::OUTPUT].tensors[0], comPat.strm.out0[i]));

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
    std::vector<std::vector<float>> cpu_in0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));
    std::vector<std::vector<float>> cpu_out0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    connectEngineStream(graph, engine, cpu_in0, vm_num_streams, 0, IO::IN);
    connectEngineStream(graph, engine, cpu_out0, vm_num_streams, 0, IO::OUT);

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(vm_num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        
        std::mt19937 gen(vm_seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < vm_num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < vm_row*vm_col; i++) {
                        cpu_in0[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("Input Matrix", cpu_in0[rel_id], vm_col, rel_id, packet, 0);
                    }

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < vm_num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("Transposed Matrix", cpu_out0[rel_id], vm_col, rel_id, packet, 1);
                    }

                    data_ready_flags[rel_id] = false;
                }
                break;
        }
    }

    return;
}

void convolution(boost::program_options::variables_map& vm) {

    /* Create Shared Memory */

    // Strings
    std::string db_name;

    // Programs
    std::vector<poplar::program::Program> progs(vm_num_streams);

    // Flags
    bool data_ready_flags[vm_num_streams];

    for (int i = 0; i < vm_num_streams; i++) {
        data_ready_flags[i] = false;
    }

    /* Get Program Context */
    
    // Get Device
    std::cout << "Getting Device..." << std::endl;
    poplar::Device device = getDevice(vm_device, vm_num_devices);
    std::cout << "Got Device!" << std::endl;

    // Graph
    std::cout << "Creating Graph..." << std::endl;
    poplar::Graph graph(device.getTarget());
    std::cout << "Created Graph!" << std::endl;

    /* Build Graph */

    // Build Model
    std::vector<model> myModels;
    std::pair<int,int> myParams = std::make_pair(vm_row, vm_col);

    buildTensorTemplate(graph, myModels, myParams, vm_num_streams, COMPATSHAPE::TRIANGLEDOWN, vm_device);

    // Add Variable Tensors
    // Not necessary

    // Constant Tensors
    // Not necessary

    std::cout << "Added Constant Tensors!" << std::endl;

    /* Add Codelets */

    // Add standard codelets
    std::cout << "Adding Codelets..." << std::endl;

    popops::addCodelets(graph);
    poplin::addCodelets(graph);

    // Add custom codelets
    graph.addCodelets("./device_libraries/io_codelet.gp");

    std::cout << "Added Codelets!" << std::endl;

    comPattern comPat;

    buildIOTemplate(graph, myModels, comPat, myParams, vm_num_streams, COMPATSHAPE::TRIANGLEDOWN, vm_device);

    /* Programs */

    std::cout << "Adding Programs..." << std::endl;

    int prog_idx = 0;

    poplar::program::Sequence seq;

    auto convp = poplin::ConvParams(poplar::FLOAT, 1, {3,3}, {2,2}, 1, 1, 1);

    poplar::OptionFlags streamOpts {
      {"bufferingDepth", "8"},
    };

    for(int i = 0; i < vm_num_streams; i++) {
        
        myModels[i].layers[LAYERS::CONSUMPTION].tensors[0] = graph.addVariable(poplar::FLOAT, {1, 1, 3, 3}, "placeholder input tensor");
        myModels[i].layers[LAYERS::CONSUMPTION].tensors[1] = graph.addVariable(poplar::FLOAT, {1, 1, 2, 2}, "placeholder weight tensor");
        myModels[i].layers[LAYERS::OUTPUT].tensors[0] = graph.addVariable(poplar::FLOAT, {1, 1, 2, 2}, "placeholder output tensor");
        poputil::mapTensorLinearly(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0]);
        poputil::mapTensorLinearly(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[1]);
        poputil::mapTensorLinearly(graph, myModels[i].layers[LAYERS::OUTPUT].tensors[0]);

        comPat.strm.out0[i] = graph.addDeviceToHostFIFO("whatever", poplar::FLOAT, 2*2, streamOpts);
    }

    for(int i = 0; i < vm_num_streams; i++) {

        // Begin Sequence 
        seq = poplar::program::Sequence();

        // Stream Inputs Programs

        seq.add(poplar::program::Copy(comPat.strm.in0[i], myModels[i].layers[LAYERS::INPUT].tensors[0]));
        seq.add(poplar::program::Copy(comPat.strm.in1[i], myModels[i].layers[LAYERS::INPUT].tensors[1]));

        seq.add(poplar::program::Execute(comPat.cps.in[i]));

        // Consumption Task Programs

        poplar::Tensor conv_out = poplin::convolution(graph, myModels[i].layers[LAYERS::CONSUMPTION].tensors[0], myModels[i].layers[LAYERS::CONSUMPTION].tensors[1], convp, false, seq, "Convolution"); // PROBLEM LINE
        poputil::mapTensorLinearly(graph, conv_out);

        seq.add(poplar::program::Copy(conv_out, myModels[i].layers[LAYERS::OUTPUT].tensors[0]));

        // Stream Outputs Programs

        //seq.add(poplar::program::Execute(comPat.cps.out[i]));

        seq.add(poplar::program::Copy(myModels[i].layers[LAYERS::OUTPUT].tensors[0], comPat.strm.out0[i]));

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
    std::vector<std::vector<float>> cpu_in0(vm_num_streams, std::vector<float> (vm_row*vm_col, 5.0));
    std::vector<std::vector<float>> cpu_in1(vm_num_streams, std::vector<float> (2*2, 5.0));
    std::vector<std::vector<float>> cpu_out0(vm_num_streams, std::vector<float> (2*2, 5.0));

    /* Connect Streams */

    std::cout << "Connecting Streams..." << std::endl;

    connectEngineStream(graph, engine, cpu_in0, vm_num_streams, 0, IO::IN);
    connectEngineStream(graph, engine, cpu_in1, vm_num_streams, 1, IO::IN);
    connectEngineStream(graph, engine, cpu_out0, vm_num_streams, 0, IO::OUT);

    std::cout << "Connected Streams!" << std::endl << std::endl;

    /* Run Parallel Threads for FireHose */

    omp_set_num_threads(vm_num_streams*2);

    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int pc_id = thread_id % 2;
        int rel_id = thread_id / 2;
        
        std::mt19937 gen(vm_seed+rel_id);
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);

        switch(pc_id) {
            case PRODUCER:
                for(int packet = 0; packet < vm_num_packets; packet++) {
                    while(data_ready_flags[rel_id]);


                    for (int i = 0; i < vm_row*vm_col; i++) {
                        cpu_in0[rel_id][i] = distribution(gen);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("Input Matrix", cpu_in0[rel_id], vm_col, rel_id, packet, 0);
                    }

                    data_ready_flags[rel_id] = true;
                }
                break;
            
            case CONSUMER:
                for(int packet = 0; packet < vm_num_packets; packet++) {
                    while(!data_ready_flags[rel_id]);

                    #pragma omp critical(ipu_work)
                    {
                        engine.run(rel_id);
                    }

                    #pragma omp critical(print)
                    {
                        printMatrix("Transposed Matrix", cpu_out0[rel_id], vm_col, rel_id, packet, 1);
                    }

                    data_ready_flags[rel_id] = false;
                }
                break;
        }
    }

    return;
}

//void placeholder(long unsigned int vm_row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices) {

//}