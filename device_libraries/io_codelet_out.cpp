#include <poplar/Vertex.hpp>

using namespace poplar;

class IOVertexOUT : public Vertex {
public:

Vector<Input<Vector<float>>> strm_in;
Output<Vector<float>> strm_out;

// Compute function
    bool compute() {
        for (unsigned i = 0; i < strm_in.size(); i++) {
            for (unsigned j = 0; j < strm_in.size(); j++) {
                strm_out[j + (i*strm_out.size())] = strm_in[i][j];
            }
        }
        return true;
    }
};