#include <poplar/Vertex.hpp>

using namespace poplar;

class IOVertexOUT : public Vertex {
public:

Input<Vector<Vector<float>>> strm_in;
//Input<Vector<int>> ready_flag;
//Input<Vector<int>> num_elements;
Output<Vector<float>> strm_out;

// Compute function
    bool compute() {
        //if (ready_flag[0]) {
            int row = 0;
            int col = 0;
            int dim = 3;
            for (unsigned i = 0; i < strm_out.size(); i++) {
                strm_out[i] = strm_in[row][col];

                if (col == dim-1) {
                    col = 0;
                    row++;
                }

                if (row == dim-1) {
                    row = 0;
                }

                col++;

            }
        //}
        return true;
    }
};