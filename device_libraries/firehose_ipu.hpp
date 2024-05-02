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
        std::vector<poplar::VertexRef> in0;
        std::vector<poplar::VertexRef> out0;
        std::vector<poplar::VertexRef> out1;
    } vtx;

    struct {
        std::vector<poplar::DataStream> in0;
        std::vector<poplar::DataStream> out0;
        std::vector<poplar::DataStream> out1;
    } strm;
};


void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet, int io);

void createIdentityMatrix(std::vector<float>& vec_id, int row, int col);

poplar::Device getDevice(int hw_mode, int num_devices);

void buildLayer(poplar::Graph& graph, model& myModel, std::pair<int,int> params, int layer_id, int map, int num_tensors);

void addComputeSet(poplar::Graph& graph, std::vector<poplar::ComputeSet>& cps, int num_streams, int IO);

void addStream(poplar::Graph& graph, std::vector<poplar::DataStream>& strm, std::pair<int,int> params, int buf_depth, int num_port, int num_streams, int IO);

void addVertex(poplar::Graph& graph, std::vector<poplar::ComputeSet>& cps, std::vector<poplar::VertexRef>& vtx, int num_streams, int offset);

void connectVertex(poplar::Graph& graph, std::vector<poplar::VertexRef>& vtx, std::vector<model>& myModels, int top_layer, int bottom_layer, int top_tensor, int bottom_tensor, std::string in, std::string out);

void connectEngineStream(poplar::Graph& graph, std::vector<float>& cpu, int num_streams, int num_port, int IO);

void buildTensorTemplateTRIANGLEUP(poplar::Graph& graph, std::vector<model>& myModels, std::pair<int,int> params, int num_streams);

void buildIOTemplateTRIANGLEUP(poplar::Graph& graph, std::vector<model>& myModels, comPatternTriangleUP& comPat, std::pair<int,int> params, int num_streams);

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

void matMul(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

void matAdd(long unsigned int row, long unsigned int col, long unsigned int num_packets, long unsigned int num_streams, long unsigned int num_devices, long unsigned int seed, bool get_from_file);

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);