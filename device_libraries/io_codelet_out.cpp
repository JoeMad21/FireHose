#include <poplar/Vertex.hpp>

using namespace poplar;

class IOVertexOUT : public Vertex {
public:

Input<Vector<Vector<float>>> strm_in;
Output<Vector<float>> strm_out;

// Compute function
    bool compute() {
        for (unsigned i = 0; i < strm_in.size(); i++) {
            for (unsigned j = 0; j < strm_in.size(); j++) {
                strm_out[i][j] = strm_in[j + (i*strm_out.size())];
            }
        }
        return true;
    }
};