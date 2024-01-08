#include <iostream>
#include <vector>
#include <random>
#include <unistd.h>
#include <omp.h>

#include <poplar/DeviceManager.hpp>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>

#include <poputil/TileMapping.hpp>
#include <poplar/Engine.hpp>

#include <poplin/codelets.hpp>
#include <poplin/experimental/QRFactorization.hpp>

void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols);

void frontEnd_TensorDecomp(bool& flag, long unsigned int& rows, long unsigned int& cols, long unsigned int& exp_size, std::vector<float>& cpu_input0, std::vector<float>& cpu_output0, std::vector<float>& cpu_output1);

void backEnd_TensorDecomp(poplar::Engine& engine, bool& flag, long unsigned int& exp_size) {

    for (int i = 0; i < exp_size; i++) {
        while(!flag) {}
        flag = false;
        engine.run(STREAM_INPUTS);
        engine.run(CONSUMPTION_TASK);
        engine.run(STREAM_RESULTS);
    }
}

void tensorDecomp() 