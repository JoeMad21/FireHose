#include <vector>
#include <random>
#include "utils.h"
#include <unistd.h>

#include <poputil/TileMapping.hpp>

#include <poplin/codelets.hpp>
//#include <poplin/MatMul.hpp>

//#include <poprand/codelets.hpp>
//#include <poprand/RandomGen.hpp>


void buildStreamPrograms(poplar::Graph& g, std::vector<poplar::program::Program>& progs, int num_transfers, int packet_size);

void buildConsumptionTaskProgram(poplar::Graph& g, std::vector<poplar::program::Program>& progs);

poplar::Engine createEngine(poplar::Device& device, poplar::Executable& exe);

void addStreamsToEngine(poplar::Engine& engine);

void runProgsOnEngine(poplar::Engine& engine);

void executeTensorDecomp(poplar::Graph& g);