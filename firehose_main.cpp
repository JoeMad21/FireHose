#include <omp.h>

void tensorDecomp() {
    
    //Expose shared memory
    poplar::Graph graph;
    std::vector<poplar::program::Program> progs;
    bool fe_flag = false;
    bool be_flag = false;

    #pragma omp parallel sections
    {
        #pragma omp section
        {
            frontEnd_TensorDecomp(rows, cols, packet_size, graph, progs, fe_flag, be_flag);
        }

        #pragma omp section
        {
            backEnd_TensorDecomp(rows, cols, packet_size, graph, progs, fe_flag, be_flag);
        }
    }
}

int main() {

    if (arg == tensordecomp) {
        tensorDecomp()
        return 0;
    }
    else {
        return 0;
    }

    return 0;  
}