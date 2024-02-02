#include <poplar/Vertex.hpp>

using namespace poplar;

class IOVertex : public Vertex {
public:

Input<Vector<float>> strm_in;
//Input<Vector<int>> ready_flag;
//Input<Vector<int>> num_elements;
Output<Vector<float>> strm_out;

// Compute function
    bool compute() {
        //if (ready_flag[0]) {
            for (unsigned i = 0; i < strm_in.size(); ++i) {
                for (unsigned j = 0; j < strm_in[i].size(); ++j) {
                    strm_in[i][j] = strm_out[i][j];
            }
        }
        //}
        return true;
    }
};