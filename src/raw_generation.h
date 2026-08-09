#ifndef RAW_GENERATION_H
#define RAW_GENERATION_H

#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cstdint>
#include <FastNoiseLite.h>
#include <math.h>

using namespace godot;

static inline Dictionary get_raw_point(int lattice_type, int seed, Vector3 global_point_pos) {
    static fnl_state simplex2_plain = []{
        fnl_state s = fnlCreateState();
        s.noise_type = FNL_NOISE_OPENSIMPLEX2;
        // frequency, octaves, etc. — all the unchanging config, set once
        return s;
    }();


    simplex2_plain.seed = seed;

    Dictionary fullness_and_material;
    
    // fullness_and_material[String("fullness")] = fnlGetNoise3D(&simplex2_plain, 16.f * global_point_pos.x, 16.f * global_point_pos.y, 16.f * global_point_pos.z);
    fullness_and_material[String("fullness")] = (global_point_pos.y > 0.f + 2.f * fnlGetNoise3D(&simplex2_plain, 2.f * global_point_pos.x, 2.f * global_point_pos.y, 2.f * global_point_pos.z)) ? -1.f : 1.f;
    fullness_and_material[String("material")] = 1;

    return fullness_and_material;
}


#endif