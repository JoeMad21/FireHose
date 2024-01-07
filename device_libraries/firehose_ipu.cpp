

enum Progs {
  STREAM_INPUTS,
  CONSUMPTION_TASK,
  STREAM_RESULTS,
  NUM_PROGRAMS
};

//Add tensor arguments
void buildStreamPrograms(poplar::Graph& g, std::vector<poplar::program::Program>& progs, int num_transfers) {

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add();
        seq.add();
    }

    progs[STREAM_INPUTS] = seq;

    seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add();
        seq.add();
    }
    
    progs[READ_RESULTS] = seq;
}

//Add tensor arguments
void buildConsumptionTaskProgram(poplar::Graph& g, std::vector<poplar::program::Program>& progs) {

    poplin::addCodelets(g);
    auto mult_out = poplin::matMul(g, gTensors.getTensor(0), gTensors.getTensor(1), seq, poplar::FLOAT);
    seq.add(poplar::program::Copy(mult_out,gTensors.getTensor(2)));
    //Connect to codely that does anomaly detection
    auto anomalyCS = g.addComputeSet("anomalyCS");
    auto num = matrix_dim;

    for (unsigned i = 0; i < num; ++i){
        auto v = g.addVertex(anomalyCS, "AnomalyVertex", {{"mult_out", mult_out[i]}, {"result",gTensors.getTensor(3)[i]}});
        g.setTileMapping(v, i);
    }

}

poplar::Engine createEngine(poplar::Device& device, poplar::Executable& exe) {
  poplar::Engine engine(std::move(exe));
  engine.load(device);

  return engine;
}

void addStreamsToEngine(poplar::Engine& engine) {
    //engine.connectStream("Source_Stream", multiplicand.data());
    //engine.connectStream("Consumption_Stream", multiplier.data());
    //engine.connectStream("Write_Result_Stream", output_init.data());
    //engine.connectStream("Anomaly_Stream", anomalies.data());
}

void runProgsOnEngine(poplar::Engine& engine) {
    //engine.run(WRITE_INPUTS);
    //engine.run(CONSUMPTION_TASK);
    //engine.run(READ_RESULTS);
    //engine.run(CHECK_ANOMALY);
    //engine.run(READ_ANOMALY);
}

void executeTensorDecomp(poplar::Graph& g) {

    try {
            //auto options = utils::parseOptions(argc, argv);
            //auto device = utils::getDeviceFromOptions(options);
            //poplar::Graph graph(device.getTarget());
            graph.addCodelets("anomaly_codelet.cpp");

        // If we are loading the graph program we do not need
        // to construct it (which can be time consuming
        // for large programs):
        std::vector<poplar::program::Program> progs;
        GraphTensors gTensors(graph, dim);
        GraphStreams gStreams(graph, dim);
        if (!options.loadExe) {
            
            progs = buildPrograms(graph, options, gTensors, gStreams, dim);
        }

        auto exe = utils::compileOrLoadExe(graph, progs, options);

        if (options.saveExe && !options.loadExe) {
        auto outf = std::ofstream(utils::getExeFileName(options));
        exe.serialize(outf);
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);
        std::vector<float> multiplicand(dim*dim);
        std::vector<float> multiplier(dim*dim);
        std::vector<float> output_init(dim*dim);
        std::vector<float> anomalies(dim*dim);

//get multiplicand from the gen file

         multiplicand = mult_matrix(dim);
         gTensors.addTensor(graph, "multiplicand", poplar::FLOAT, dim,dim); 


        for (int i = 0; i < dim*dim; i++) {
            multiplier[i] = distribution(gen);
            output_init[i] = -1.0f;
        }

        //printMatrix("Multiplicand", multiplicand, dim);
        //printMatrix("Multiplier", multiplier, dim);

        executeIPUCode(device, exe, multiplicand, multiplier, output_init, anomalies);

        //printMatrix("Result", output_init , dim);

        //printMatrix("Anomaly", anomalies, dim);

        std::ofstream mltnd_strm ("multiplicand.txt");

        for(int i = 0; i < multiplicand.size(); i++)
        {
          mltnd_strm << multiplicand[i];
          mltnd_strm << "\n";
        }

        mltnd_strm.close();

        std::ofstream mltr_strm ("multiplier.txt");

        for(int i = 0; i < multiplier.size(); i++)
        {
          mltr_strm << multiplier[i];
          mltr_strm << "\n";
        }

        mltr_strm.close();

        std::ofstream results_strm ("results.txt");

        for(int i = 0; i < output_init.size(); i++)
        {
          results_strm << output_init[i];
          results_strm << "\n";
        }

        results_strm.close();

        std::ofstream anom_strm ("anomalies.txt");

        for(int i = 0; i < anomalies.size(); i++)
        {
          anom_strm << anomalies[i];
          anom_strm << "\n";
        }

        anom_strm.close();

    } catch (const std::exception &e) {
         std::cerr << "Exception: " << e.what() << "\n";
    }

}

void launchOnIPU(long unsigned int dim, int argc, char **argv) {
    try {
         auto options = utils::parseOptions(argc, argv);
         auto device = utils::getDeviceFromOptions(options);
        poplar::Graph graph(device.getTarget());
        graph.addCodelets("anomaly_codelet.cpp");

        // If we are loading the graph program we do not need
        // to construct it (which can be time consuming
        // for large programs):
        std::vector<poplar::program::Program> progs;
        GraphTensors gTensors(graph, dim);
        GraphStreams gStreams(graph, dim);
        if (!options.loadExe) {
            
            progs = buildPrograms(graph, options, gTensors, gStreams, dim);
        }

        auto exe = utils::compileOrLoadExe(graph, progs, options);

        if (options.saveExe && !options.loadExe) {
        auto outf = std::ofstream(utils::getExeFileName(options));
        exe.serialize(outf);
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> distribution(0.0f, 100.0f);
        std::vector<float> multiplicand(dim*dim);
        std::vector<float> multiplier(dim*dim);
        std::vector<float> output_init(dim*dim);
        std::vector<float> anomalies(dim*dim);

//get multiplicand from the gen file

         multiplicand = mult_matrix(dim);
         gTensors.addTensor(graph, "multiplicand", poplar::FLOAT, dim,dim); 


        for (int i = 0; i < dim*dim; i++) {
            multiplier[i] = distribution(gen);
            output_init[i] = -1.0f;
        }

        //printMatrix("Multiplicand", multiplicand, dim);
        //printMatrix("Multiplier", multiplier, dim);

        executeIPUCode(device, exe, multiplicand, multiplier, output_init, anomalies);

        //printMatrix("Result", output_init , dim);

        //printMatrix("Anomaly", anomalies, dim);

        std::ofstream mltnd_strm ("multiplicand.txt");

        for(int i = 0; i < multiplicand.size(); i++)
        {
          mltnd_strm << multiplicand[i];
          mltnd_strm << "\n";
        }

        mltnd_strm.close();

        std::ofstream mltr_strm ("multiplier.txt");

        for(int i = 0; i < multiplier.size(); i++)
        {
          mltr_strm << multiplier[i];
          mltr_strm << "\n";
        }

        mltr_strm.close();

        std::ofstream results_strm ("results.txt");

        for(int i = 0; i < output_init.size(); i++)
        {
          results_strm << output_init[i];
          results_strm << "\n";
        }

        results_strm.close();

        std::ofstream anom_strm ("anomalies.txt");

        for(int i = 0; i < anomalies.size(); i++)
        {
          anom_strm << anomalies[i];
          anom_strm << "\n";
        }

        anom_strm.close();

    } catch (const std::exception &e) {
         std::cerr << "Exception: " << e.what() << "\n";
    }
}