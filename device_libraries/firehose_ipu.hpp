#pragma once

#include "ipu_support.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <unistd.h>
#include <omp.h>
#include <optional>

#include <popops/ElementWise.hpp>
#include <popops/codelets.hpp>
#include <popops/Rearrange.hpp>

#include <poplin/codelets.hpp>
#include <poplin/experimental/QRFactorization.hpp>
#include <poplin/MatMul.hpp>
#include <poplin/ConvParams.hpp>


void printMatrix(std::string matrix_name, std::vector<float> matrix, int cols, int id, int packet, int io);

void createIdentityMatrix(std::vector<float>& vec_id, int row, int col);

void tensorDecomp(boost::program_options::variables_map& vm);
void matMul(boost::program_options::variables_map& vm);
void matAdd(boost::program_options::variables_map& vm);
void transpose(boost::program_options::variables_map& vm);
void convolution(boost::program_options::variables_map& vm);

//void placeholder(long unsigned int row, long unsigned int col, long unsigned int num_streams, long unsigned int num_devices);