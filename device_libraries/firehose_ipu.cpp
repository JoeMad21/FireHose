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

//Add tensor arguments
void buildStreamPrograms(poplar::Graph& g, std::vector<poplar::program::Program>& progs, int num_transfers, int packet_size) {

    // Add Tensors
    auto source_tensor = g.addVariable(poplar::FLOAT, {packet_size}, "Source Matrix")
    auto q_tensor = g.addVariable(poplar::FLOAT, {packet_size}, "QMatrix")
    auto r_tensor = g.addVariable(poplar::FLOAT, {packet_size}, "RMatrix")

    auto seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add(poplar::program::Copy(source_stream, source_tensor));
        seq.add();
    }

    progs[STREAM_INPUTS] = seq;

    seq = poplar::program::Sequence();

    for(int i = 0; i < num_transfers; i++) {
        seq.add(poplar::program::Copy(q_tensor, q_stream));
        seq.add(poplar::program::Copy(r_tensor, r_stream));
    }
    
    progs[READ_RESULTS] = seq;
}

//Add tensor arguments
void buildCTTensorDecompProgram(poplar::Graph& g, std::vector<poplar::program::Program>& progs) {

    poplin::addCodelets(g);
    auto qr_out = poplin::qr();
    seq.add(poplar::program::Copy(mult_out,tensor));
    //Connect to codely that does anomaly detection
    auto anomalyCS = g.addComputeSet("anomalyCS");

    for (unsigned i = 5; i < 100; ++i){
        auto v = g.addVertex(anomalyCS,);
        g.setTileMapping(v, i);
    }

}

poplar::Engine createEngine(poplar::Device& device, poplar::Executable& exe) {
  poplar::Engine engine(std::move(exe));
  engine.load(device);

  return engine;
}

void addStreamsToEngine(poplar::Engine& engine, std::vector<string>& names, std::vector<std::vector<float>>& cpu_vectors) {
    for(int i = 0; i < max; i++) {
        engine.connectStream(names[i], cpu_vectors[i].data());
    }
}

void runProgsOnEngine(poplar::Engine& engine) {
    engine.run(STREAM_INPUTS);
    engine.run(CONSUMPTION_TASK);
    engine.run(STREAM_RESULTS);
}

void executeTensorDecomp(poplar::Graph& g, std::vector<poplar::program::Program>& progs) {

    buildStreamPrograms(g, progs, num_transfers, packet_size);
    buildConsumptionTaskProgram(g, progs);

    poplar::compileGraph(g, progs);

}

void frontEnd_TensorDecomp(poplar::Graph& g, std::vector<poplar::program::Program>& progs, int rows, int cols, int packet_size, bool flag) {

    // Create data to input into back-end
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> distribution(0.0f, 100.0f);
    std::vector<float> source_matrix(rows*cols);

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            source_matrix[j+(cols*i)] = distribution(gen);
        }
    }

    // Create buffer to store results from back-end
    std::vector<float> qmatrix(rows*cols);
    std::vector<float> rmatrix(rows*cols)

    int num_transfers = rows*cols / packet_size;

    std::vector<string> names = {"source_matrix", "qmatrix", "rmatrix"};
    std::vector<std::vector<float>> cpu_vectors = {source_matrix, qmatrix, rmatrix};

    while(!be_flag) {}

    buildStreamPrograms(g, progs, num_transfers, packet_size);

    fe_flag = true;

    printMatrix("QMatrix", qmatrix, cols);
    printMatrix("RMatrix", rmatrix, cols);
}

void backEnd_TensorDecomp(int rows, int cols) {

    addStreamsToEngine(engine, names, cpu_vectors);

    be_flag = true;

    while(!fe_flag) {}

    buildCTTensorDecompProgram(g, progs);

    createEngine(device, exe);

    runProgsOnEngine(engine);

    executeTensorDecomp(g, progs);

}