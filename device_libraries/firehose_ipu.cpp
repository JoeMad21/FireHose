

enum Progs {
  STREAM_INPUTS,
  CONSUMPTION_TASK,
  STREAM_RESULTS,
  NUM_PROGRAMS
};

//Add tensor arguments
void buildStreamPrograms(poplar::Graph &g, std::vector<poplar::program::Program>& progs) {

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < elements/packet_size; i++) {
        seq.add();
        seq.add();
    }

    progs[STREAM_INPUTS] = seq;

    seq = poplar::program::Sequence();

    for(int i = 0; i < elements/packet_size; i++) {
        seq.add();
        seq.add();
    }
    
    progs[READ_RESULTS] = seq;
}

//Add tensor arguments
void buildConsumptionTaskProgram(poplar::Graph &g, std::vector<poplar::program::Program>& progs) {

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

void executeIPUCode(poplar::Device &device, poplar::Executable &exe) {
  poplar::Engine engine(std::move(exe));
  engine.load(device);

  //engine.connectStream("Source_Stream", multiplicand.data());
  //engine.connectStream("Consumption_Stream", multiplier.data());
  //engine.connectStream("Write_Result_Stream", output_init.data());
  //engine.connectStream("Anomaly_Stream", anomalies.data());

  engine.run(WRITE_INPUTS);
  engine.run(CONSUMPTION_TASK);
  engine.run(READ_RESULTS);
  engine.run(CHECK_ANOMALY);
  engine.run(READ_ANOMALY);

}