#pragma once

#include <iostream>
#include <vector>
#include <random>
#include <unistd.h>
#include <omp.h>


#include <poplar/DeviceManager.hpp>
#include <poplar/Engine.hpp>
#include <poplar/Graph.hpp>
#include <poplar/IPUModel.hpp>
#include <poplar/Tensor.hpp>

#include <popops/codelets.hpp>

#include <poputil/TileMapping.hpp>
#include <poplar/Engine.hpp>

#include <poplin/codelets.hpp>
#include <poplin/experimental/QRFactorization.hpp>

void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols);

void tensorDecomp(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);