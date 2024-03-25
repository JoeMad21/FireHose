#include <poplar/Vertex.hpp>

using namespace poplar;

class IOVertexIN : public Vertex {
public:

Input<Vector<float>> strm_in;
Vector<Output<Vector<float>>> strm_out;

// Compute function
    bool compute() {
        for (unsigned i = 0; i < strm_out.size(); i++) {
            for (unsigned j = 0; j < strm_out.size(); j++) {
                strm_out[i][j] = strm_in[j + (i*strm_out.size())];
            }
        }
        return true;
    }
};