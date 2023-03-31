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

typedef struct {
    float x, y, z;
} allo_gltf_point;

typedef struct {
    allo_gltf_point min;
    allo_gltf_point max;
} allo_gltf_bb;

TinyGLTF loader;
class AlloModelManager {
    std::unordered_map<std::string, Model*> models;

public:
    bool load(std::string name, const unsigned char *bytes, uint32_t size) {
        if (models.find(name) != models.end()) {
            return true;
        }
        
        auto model = new Model();
        std::string err, warn;
        bool ok = loader.LoadBinaryFromMemory(model, &err, &warn, bytes, size);
        if (ok) {
            models[std::string(name)] = model;
        } else {
            delete model;
            fprintf(stderr, "AlloModelManager: Failed to load model. err: %s. warn: %s", err.c_str(), warn.c_str());
            return false;
        }
        return true;
    }
  
    bool unload(std::string name) {
        auto itr = models.find(name);
        if (itr != models.end()) {
            Model *model = itr->second;
            delete model;
            models.erase(itr);
            return true;
        }
        return false;
    }
    
    bool get_aabb(std::string name, allo_gltf_bb *bb) {
        auto it = models.find(name);
        if (it == models.end()) {
            return false;
        }
        Model *model = it->second;
        allo_gltf_point min = {FLT_MAX, FLT_MAX, FLT_MAX}, max = {FLT_MIN, FLT_MIN, FLT_MIN};
        for (auto node = model->nodes.begin(); node < model->nodes.end(); node++) {
            auto mesh = &model->meshes[node->mesh];
            auto scale = node->scale;
            if (scale.size() < 3) {
                scale = std::vector<double>(3);
                scale[0] = scale[1] = scale[2] = 1;
            }
            for (auto prim = mesh->primitives.begin(); prim < mesh->primitives.end(); prim++) {
                auto accessor = model->accessors[prim->attributes["POSITION"]];
                auto bufferview = model->bufferViews[accessor.bufferView];
                auto buffer = model->buffers[bufferview.buffer];
                auto positions = reinterpret_cast<const float*>(&buffer.data[bufferview.byteOffset + accessor.byteOffset]);
                
                for (size_t i = 0; i < accessor.count; i++) {
                    auto p = positions + i*3;
                    auto s = scale.begin();
                    min.x = MIN(min.x, *p * *s);
                    max.x = MAX(max.x, *p * *s);
                    ++p; ++s;
                    min.y = MIN(min.y, *p * *s);
                    max.y = MAX(max.y, *p * *s);
                    ++p; ++s;
                    min.z = MIN(min.z, *p * *s);
                    max.z = MAX(max.z, *p * *s);
                }
            }
        }
        bb->min = min;
        bb->max = max;
        return true;
    }
};

AlloModelManager modelManager;

extern "C" bool allo_gltf_load(const char *name_, const unsigned char *bytes, uint32_t size) {
    auto name = std::string(name_);
    return modelManager.load(name, bytes, size);
}

extern "C" bool allo_gltf_unload(const char *name_) {
    auto name = std::string(name_);
    return modelManager.unload(name);
}

extern "C" bool allo_gltf_get_aabb(const char *name_, allo_gltf_bb *bb) {
    auto name = std::string(name_);
    return modelManager.get_aabb(name, bb);
}

extern "C" allo_m4x4 allo_gltf_get_node_transform(const unsigned char *bytes, uint32_t size, const char *node_name)
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
