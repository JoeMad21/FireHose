#include <poplar/Vertex.hpp>

using namespace poplar;

class IOVertexIN : public Vertex {
public:

Input<Vector<float>> strm_in;
//Input<Vector<int>> ready_flag;
//Input<Vector<int>> num_elements;
Output<Vector<Vector<float>>> strm_out;

// Compute function
    bool compute() {
        //if (ready_flag[0]) {
            int row = 0;
            int col = 0;
            int dim = 3;
            for (unsigned i = 0; i < strm_in.size(); i++) {
                strm_out[row][col] = strm_in[i];

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