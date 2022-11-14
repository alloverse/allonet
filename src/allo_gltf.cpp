#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
// __has_warning was introduced in clang 16ish, we only have clang10 in mainstream ubuntu :/
#define __has_warning(x) false 
#include <tiny_gltf.h>
#include <string>
#include <allonet/math.h>
extern "C" {
#include "mathc.h"
}

using namespace tinygltf;

TinyGLTF loader;

extern "C" allo_m4x4 allo_gltf_get_node_transform(const unsigned char *bytes, uint64_t size, const char *node_name)
{
    Model model;
    std::string err, warn;
    allo_m4x4 m = allo_m4x4_identity(); m.c1r1 = m.c2r2 = m.c3r3 = m.c4r4 = -1; // invalid
    bool ok = loader.LoadBinaryFromMemory(&model, &err, &warn, bytes, size);
    if(!ok) return m;

    for(auto it = model.nodes.begin(); it != model.nodes.end(); it++)
    {
        if(it->name == node_name) {
            if(it->matrix.size() != 16)
            {
                m = allo_m4x4_identity();
                if(it->translation.size() == 3)
                {
                    allo_vector translation({it->translation[0], it->translation[1], it->translation[2]});
                    m = allo_m4x4_translate(translation);
                }
                if(it->rotation.size() == 4)
                {
                    allo_m4x4 rot;
                    double q[4];
                    for(int i = 0; i < 4; i++)
                        q[i] = it->rotation[i];
                    mat4_rotation_quat(rot.v, q);
                    m = allo_m4x4_concat(m, rot);
                }
                if(it->scale.size() == 3)
                {
                    double s[3];
                    for(int i = 0; i < 3; i++)
                        s[i] = it->scale[i];
                    mat4_scale(m.v, m.v, s);
                }
            }
            else
            {
                for(int i = 0; i < 16; i++)
                    m.v[i] = it->matrix[i];
            }
            return m; // yay!
        }
    }

    return m; // invalid
    
}
