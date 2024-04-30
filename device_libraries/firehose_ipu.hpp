#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <unistd.h>
#include <omp.h>


#include <poplar/DeviceManager.hpp>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplar/Tensor.hpp>

#include <popops/ElementWise.hpp>
#include <popops/codelets.hpp>

#include <poputil/TileMapping.hpp>
#include <poplar/Engine.hpp>
#include <poplar/ReplicatedStreamMode.hpp>

#include <poplin/codelets.hpp>
#include <poplin/experimental/QRFactorization.hpp>
#include <poplin/MatMul.hpp>

struct model {
    std::vector<layer> layers;
    model(int num_layers) {
        std::vector<layer> layers(num_layers);
    }
};

struct layer {
    std::vector<poplar::Tensor> tensors;
    layers(int num_tensors) {
        std::vector<layer> tensors(num_tensors);
    }
};

void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet, int io);

poplar::Device getDevice(int hw_mode, int num_devices);

void buildGraph(poplar::Graph& graph, int num_inputs, int num_outputs, int num_streams, int row, int col);

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

void matMul(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

void matAdd(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);