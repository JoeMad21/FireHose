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

struct layer {
    std::vector<poplar::Tensor> tensors;
};

struct model {
    std::vector<layer> layers;
};

struct comPatternTriangleUP {

    struct {
        std::vector<poplar::ComputeSet> in;
        std::vector<poplar::ComputeSet> out;
    } cps;

    struct {
        std::vector<poplar::VertexRef> in0(num_streams);
        std::vector<poplar::VertexRef> out0(num_streams);
        std::vector<poplar::VertexRef> out1(num_streams);
    } vtx;

    struct {
        std::vector<poplar::DataStream> in0(num_streams);
        std::vector<poplar::DataStream> out0(num_streams);
        std::vector<poplar::DataStream> out1(num_streams);
    } strm;
}


void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet, int io);

void createIdentityMatrix(std::vector<float>& vec_id, int row, int col);

poplar::Device getDevice(int hw_mode, int num_devices);

void buildLayer(poplar::Graph& graph, model& myModel, std::pair<int,int> params, int layer_id, int map, int num_tensors);

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

void matMul(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

void matAdd(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);