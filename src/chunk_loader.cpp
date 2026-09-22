#include "chunk_loader.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void ChunkLoader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "lattice_ptr", "peer_id", "mesh_rad", "collision_rad"), &ChunkLoader::setup);
	ClassDB::bind_method(D_METHOD("check_and_load"), &ChunkLoader::check_and_load);
}

ChunkLoader::ChunkLoader() {
	lattice_ptr = nullptr;
}

ChunkLoader::~ChunkLoader() {
	
}

void ChunkLoader::_exit_tree() {
    if (lattice_ptr == nullptr) return;
	peer_id = -1;
    for (const Vector3i &idx : mesh_occupied) {
        lattice_ptr->loader_exits_chunk(this, idx);
    }
    mesh_occupied.clear();
    collision_occupied.clear();
}

void ChunkLoader::setup(ChunkLattice* p_lattice_ptr, int p_peer_id, int p_mesh_rad, int p_collision_rad) {
	lattice_ptr = p_lattice_ptr;
	peer_id = p_peer_id;
	mesh_rad = p_mesh_rad;
	collision_rad = p_collision_rad;

	stride = (lattice_ptr->chunk_shape - Vector3i(1, 1, 1));
	box_half = Vector3i(mesh_rad * stride.x / 2, mesh_rad * stride.y / 2, mesh_rad * stride.z / 2);
	collision_half = Vector3i(collision_rad * stride.x / 2, collision_rad * stride.y / 2, collision_rad * stride.z / 2);
}

void ChunkLoader::compute_idxs() {
	global_idx = Vector3i((get_global_position() / lattice_ptr->chunk_cube_size).floor());
	Vector3i stride(lattice_ptr->chunk_shape - Vector3i(1, 1, 1));
	central_chunk_idx = Vector3i(
        floordiv(global_idx.x, stride.x),
        floordiv(global_idx.y, stride.y),
        floordiv(global_idx.z, stride.z)
    );
}

// deterministic: same input always gives the same output
Vector3i ChunkLoader::hash_vec3i_to_range(Vector3i p) {
    int seed = 42;
	// no need to center as loader center chunk is also not cetn
	Vector3i min_inclusive = lattice_ptr->chunk_shape / 4;
	Vector3i max_exclusive = 3 * lattice_ptr->chunk_shape / 4 + Vector3i(1, 1, 1);
	
	auto h = [](uint32_t x) -> uint32_t {   // finalizer from MurmurHash3
        x ^= x >> 16;
        x *= 0x7feb352dU;
        x ^= x >> 15;
        x *= 0x846ca68bU;
        x ^= x >> 16;
        return x;
    };

    uint32_t base = h((uint32_t)p.x * 73856093u
                    ^ (uint32_t)p.y * 19349663u
                    ^ (uint32_t)p.z * 83492791u
                    ^ (uint32_t)seed);

    Vector3i span = max_exclusive - min_inclusive;
    return Vector3i(
        min_inclusive.x + (int)(h(base + 1u) % (uint32_t)span.x),
        min_inclusive.y + (int)(h(base + 2u) % (uint32_t)span.y),
        min_inclusive.z + (int)(h(base + 3u) % (uint32_t)span.z)
    );
}

void ChunkLoader::check_and_load() {
	compute_idxs();

	// mesh
	HashSet<Vector3i> new_mesh;
	for (int dx = 1 - mesh_rad; dx < mesh_rad; dx++) {
		for (int dy = 1 - mesh_rad; dy < mesh_rad; dy++) {
			for (int dz = 1 - mesh_rad; dz < mesh_rad; dz++) {
				Vector3i chunk_idx(central_chunk_idx + Vector3i(dx, dy, dz));
				Vector3i anchor = chunk_idx * stride + hash_vec3i_to_range(chunk_idx);

				Vector3i d = anchor - global_idx;
				bool in_range =
					-box_half.x <= d.x && d.x <= box_half.x &&
					-box_half.y <= d.y && d.y <= box_half.y &&
					-box_half.z <= d.z && d.z <= box_half.z;

				if (in_range) new_mesh.insert(chunk_idx);
			}
		}
	}

	// exit old chunks
	std::vector<Vector3i> exit;
	for (const Vector3i &idx : mesh_occupied) {
		if (!new_mesh.has(idx)) exit.push_back(idx);
	}
	for (const Vector3i &idx : exit) {
		lattice_ptr->loader_exits_chunk(this, idx);
		mesh_occupied.erase(idx);
		collision_occupied.erase(idx);
	}

	// enter new chunks
	for (const Vector3i &idx : new_mesh) {
		if (!mesh_occupied.has(idx)) {
			lattice_ptr->loader_enters_mesh_chunk(this, idx);
			mesh_occupied.insert(idx);
		}
	}

	// collision
	HashSet<Vector3i> new_collision;
	for (int dx = 1 - collision_rad; dx < collision_rad; dx++) {
		for (int dy = 1 - collision_rad; dy < collision_rad; dy++) {
			for (int dz = 1 - collision_rad; dz < collision_rad; dz++) {
				Vector3i chunk_idx(central_chunk_idx + Vector3i(dx, dy, dz));
				Vector3i anchor = chunk_idx * stride + hash_vec3i_to_range(chunk_idx);

				Vector3i d = anchor - global_idx;
				bool in_range =
					-box_half.x <= d.x && d.x <= box_half.x &&
					-box_half.y <= d.y && d.y <= box_half.y &&
					-box_half.z <= d.z && d.z <= box_half.z;

				if (in_range) new_collision.insert(chunk_idx);
			}
		}
	}

	// enter new chunks
	for (const Vector3i &idx : new_collision) {
		if (!collision_occupied.has(idx)) {
			lattice_ptr->loader_enters_collision_chunk(this, idx);
			collision_occupied.insert(idx);
		}
	}
}