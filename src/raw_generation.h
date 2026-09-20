#ifndef RAW_GENERATION_H
#define RAW_GENERATION_H

#include "chunk.h"

#include <constants.h>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <cstdint>
#include <FastNoiseLite.h>
#include <math.h>

using namespace godot;

/*
TEST FLATS
TERRAIN OUTLINE
flat with height defined by 4.f * simplex(2.0 * x)
*/

// raw_generation.h
static inline std::vector<Ref<Thread>> &worker_threads() {
    static std::vector<Ref<Thread>> threads;   // constructed on first call, not at load
    return threads;
}
const int NUM_THREADS = 1;
static inline void init_worker_threads(NUM_THREADS) {
    auto &t = worker_threads();
    if (!t.empty()) return;                    // already initialised
    t.resize(n);
    for (int i = 0; i < n; i++) t[i].instantiate();
}

static inline fnl_state make_noise_state(int lattice_type, int seed) {
    fnl_state s = fnlCreateState();
    switch (lattice_type) {
        default:
            s.noise_type = FNL_NOISE_OPENSIMPLEX2;
            break;
    }
    s.seed = seed;
    return s;
}

static inline PointValue get_raw_point(fnl_state &state, Vector3 global_point_pos) {
    PointValue v;
    v.fullness = (global_point_pos.y > 0.f + 2.f * fnlGetNoise3D(&state, 2.f*global_point_pos.x, 2.f*global_point_pos.y, 2.f*global_point_pos.z)) ? -1.f : 1.f;
    v.material = 1;
    return v;
}

static inline ChunkChanges get_structure_changes(Vector3 chunk_pos, Vector3i chunk_shape, Vector3 chunk_cube_size, Vector3 cell_size, Vector3i struct_shape, const float *structure_lattice, Vector2i lattice_shape_yz) {
    // get covered cells
	PackedVector3Array cell_positions;
    cell_positions.append(Vector3(floorf(chunk_pos.x / cell_size.x), floorf(chunk_pos.y / cell_size.y), floorf(chunk_pos.z / cell_size.z)) * cell_size);

	// initialise changes arrays
	PackedInt32Array changes_idxs;
	PackedFloat32Array changes_fullness;
	PackedByteArray changes_material;

	// iterate over cells
	for (int64_t cell_idx = 0; cell_idx < cell_positions.size(); cell_idx++) {
		// determine if structure is placed in the cell
		if (true) {
			// get structure position and rotation
			Vector3 struct_pos = cell_positions[cell_idx] + 0.5f * cell_size;
			Basis domain_basis;
			domain_basis.set_columns(Vector3(0, 16, 0), Vector3(8, 0, 0), Vector3(0, 0, 4));
			Basis domain_inverse_basis = domain_basis.inverse();

			// iterate over all cells of chunk, check if global position is in the structure domain, fill in
			for (int i = 0; i < chunk_shape.x; i++) {
				for (int j = 0; j < chunk_shape.y; j++) {
					for (int k = 0; k < chunk_shape.z; k++) {
						Vector3 world_point_pos = chunk_pos + Vector3(i, j, k) * chunk_cube_size;

						Vector3 struct_domain_point_pos = domain_inverse_basis.xform(world_point_pos - struct_pos);

						// check if the index is in range
                        Vector3 inner_idx = 0.5f*(struct_domain_point_pos+Vector3(1, 1, 1)) * Vector3(struct_shape - Vector3(1, 1, 1));
                        Vector3i lower_idx(inner_idx.floor());
                        // if right on the edge still consider
                        if (struct_domain_point_pos.x == 1) lower_idx.x--;
                        if (struct_domain_point_pos.y == 1) lower_idx.y--;
                        if (struct_domain_point_pos.z == 1) lower_idx.z--;
						if (0 <= lower_idx.x && lower_idx.x < struct_shape.x-1 &&
						    0 <= lower_idx.y && lower_idx.y < struct_shape.y-1 &&
						    0 <= lower_idx.z && lower_idx.z < struct_shape.z-1) {
							// TODO trilinear interpolation, for now just nearest index
							// TODO add clamp
							int index_in_chunk = i * chunk_shape.y * chunk_shape.z + j * chunk_shape.z + k;

                            float dx = inner_idx.x - lower_idx.x;
                            float dy = inner_idx.y - lower_idx.y;
                            float dz = inner_idx.z - lower_idx.z;

                            const int sy = lattice_shape_yz.x, sz = lattice_shape_yz.y;
                            #define L(I,J,K) structure_lattice[(I) * sy * sz + (J) * sz + (K)]
                            float c000 = L(lower_idx.x,   lower_idx.y,   lower_idx.z  );
                            float c001 = L(lower_idx.x,   lower_idx.y,   lower_idx.z+1);
                            float c010 = L(lower_idx.x,   lower_idx.y+1, lower_idx.z  );
                            float c011 = L(lower_idx.x,   lower_idx.y+1, lower_idx.z+1);
                            float c100 = L(lower_idx.x+1, lower_idx.y,   lower_idx.z  );
                            float c101 = L(lower_idx.x+1, lower_idx.y,   lower_idx.z+1);
                            float c110 = L(lower_idx.x+1, lower_idx.y+1, lower_idx.z  );
                            float c111 = L(lower_idx.x+1, lower_idx.y+1, lower_idx.z+1);
                            #undef L

							float fullness_val = 
                                c000 * (1 - dx) * (1 - dy) * (1 - dz) + 
                                c100 * (dx) * (1 - dy) * (1 - dz) + 
                                c010 * (1 - dx) * (dy) * (1 - dz) + 
                                c110 * (dx) * (dy) * (1 - dz) + 
                                c001 * (1 - dx) * (1 - dy) * (dz) + 
                                c101 * (dx) * (1 - dy) * (dz) + 
                                c011 * (1 - dx) * (dy) * (dz) + 
                                c111 * (dx) * (dy) * (dz);
                            
							int material = 1;

							changes_idxs.append(index_in_chunk);
							changes_fullness.append(fullness_val);
							changes_material.append(material);
						}
					}
				}
			}
		}
	}

	ChunkChanges c;
	c.idxs = changes_idxs;
	c.fullness = changes_fullness;
	c.material = changes_material;

	return c;
}

static inline ChunkChanges get_chunk_structure_changes(Vector3 chunk_pos, Vector3i chunk_shape, Vector3 chunk_cube_size) {
    ChunkChanges banana_changes = get_structure_changes(chunk_pos, chunk_shape, chunk_cube_size, STRUCTURE_BANANA_CELL_SIZE, STRUCTURE_BANANA_SHAPE, &STRUCTURE_BANANA_LATTICE[0][0][0], Vector2i(18, 9));
    // TODO other changes
    return banana_changes;
}

static inline void set_chunk_raw_data(Chunk* chunk) {
    Vector3 chunk_pos = chunk->get_global_position();
    fnl_state noise_state = make_noise_state(chunk->lattice_type, chunk->lattice_seed);

    // terrain
	for (int i = 0; i < chunk->chunk_shape.x; i++) {
		for (int j = 0; j < chunk->chunk_shape.y; j++) {
			for (int k = 0; k < chunk->chunk_shape.z; k++) {
				int idx = chunk->get_idx(i, j, k);
				Vector3 global_pos(chunk_pos + chunk->chunk_cube_size*Vector3(i, j, k));

                PointValue p = get_raw_point(noise_state, global_pos);
				chunk->point_fullness_values.set(idx, p.fullness);
				chunk->point_material_values.set(idx, p.material);
			}
		}
	}

    // structures
    ChunkChanges c = get_chunk_structure_changes(chunk_pos, chunk->chunk_shape, chunk->chunk_cube_size);
    // apply changes without writing to the changes list (this is a part of the terrain)
    PackedInt32Array changes_idxs = c.idxs;
    PackedFloat32Array changes_fullness = c.fullness;
    PackedByteArray changes_material = c.material;
    for (int64_t i = 0; i < (int64_t)changes_idxs.size(); i++) {
        // apply changes to the array
        int32_t _idx = changes_idxs[i];
        chunk->point_fullness_values.set(_idx, changes_fullness[i]);
        chunk->point_material_values.set(_idx, changes_material[i]);
    }
}

#endif