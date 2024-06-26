#pragma once

#include <boost/program_options.hpp>

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
#include <popops/Rearrange.hpp>

#include <poputil/TileMapping.hpp>
#include <poplar/Engine.hpp>
#include <poplar/ReplicatedStreamMode.hpp>

#include <poplin/codelets.hpp>
#include <poplin/experimental/QRFactorization.hpp>
#include <poplin/MatMul.hpp>
#include <poplin/ConvParams.hpp>

struct layer {
    std::vector<poplar::Tensor> tensors;
};

struct model {
    std::vector<layer> layers;
};

struct comPattern {

    struct {
        std::vector<poplar::ComputeSet> in;
        std::vector<poplar::ComputeSet> out;
    } cps;

    struct {
        std::vector<poplar::VertexRef> in0;
        std::vector<poplar::VertexRef> in1;
        std::vector<poplar::VertexRef> out0;
        std::vector<poplar::VertexRef> out1;
    } vtx;

    struct {
        std::vector<poplar::DataStream> in0;
        std::vector<poplar::DataStream> in1;
        std::vector<poplar::DataStream> out0;
        std::vector<poplar::DataStream> out1;
    } strm;
};


void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet, int io);

void createIdentityMatrix(std::vector<float>& vec_id, int row, int col);

poplar::Device getDevice(int hw_mode, int num_devices);

void buildLayer(poplar::Graph& graph, model& myModel, std::pair<int,int> params, int layer_id, int map, int num_tensors, int hw);

void addComputeSet(poplar::Graph& graph, std::vector<poplar::ComputeSet>& cps, int num_streams, int IO);

void addStream(poplar::Graph& graph, std::vector<poplar::DataStream>& strm, std::pair<int,int> params, int buf_depth, int num_port, int num_streams, int IO);

void addVertex(poplar::Graph& graph, std::vector<poplar::ComputeSet>& cps, std::vector<poplar::VertexRef>& vtx, int num_streams, int offset, int hw);

void connectVertex(poplar::Graph& graph, std::vector<poplar::VertexRef>& vtx, std::vector<model>& myModels, int num_streams, int top_layer, int bottom_layer, int top_tensor, int bottom_tensor, std::string in, std::string out);

void connectEngineStream(poplar::Graph& graph, std::vector<float>& cpu, int num_streams, int num_port, int IO);

void buildTensorTemplate(poplar::Graph& graph, std::vector<model>& myModels, std::pair<int,int> params, int num_streams, int mode, int hw);

void buildIOTemplate(poplar::Graph& graph, std::vector<model>& myModels, comPattern& comPat, std::pair<int,int> params, int num_streams, int mode);

void tensorDecomp(boost::program_options::variables_map& vm);

void matMul(boost::program_options::variables_map& vm);

void matAdd(boost::program_options::variables_map& vm);

void transpose(boost::program_options::variables_map& vm);

void convolution(boost::program_options::variables_map& vm);

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);