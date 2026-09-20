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
}

void ChunkLoader::compute_central_chunk_idx() {
	Vector3i global_idx((get_global_position() / lattice_ptr->chunk_cube_size).floor());
	Vector3i stride(lattice_ptr->chunk_shape - Vector3i(1, 1, 1));
	central_chunk_idx = Vector3i(
        floordiv(global_idx.x, stride.x),
        floordiv(global_idx.y, stride.y),
        floordiv(global_idx.z, stride.z)
    );
}

void ChunkLoader::check_and_load() {
	Vector3i old_central_chunk_idx = central_chunk_idx;
	compute_central_chunk_idx();
	if (central_chunk_idx == old_central_chunk_idx && initialised) return;
	else if (!initialised) initialised = true;

	// mesh
	HashSet<Vector3i> new_mesh;
	for (int dx = 1 - mesh_rad; dx < mesh_rad; dx++)
		for (int dy = 1 - mesh_rad; dy < mesh_rad; dy++)
			for (int dz = 1 - mesh_rad; dz < mesh_rad; dz++)
				new_mesh.insert(central_chunk_idx + Vector3i(dx, dy, dz));

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
	for (int dx = 1 - collision_rad; dx < collision_rad; dx++)
		for (int dy = 1 - collision_rad; dy < collision_rad; dy++)
			for (int dz = 1 - collision_rad; dz < collision_rad; dz++)
				new_collision.insert(central_chunk_idx + Vector3i(dx, dy, dz));

	// enter new chunks
	for (const Vector3i &idx : new_collision) {
		if (!collision_occupied.has(idx)) {
			lattice_ptr->loader_enters_collision_chunk(this, idx);
			collision_occupied.insert(idx);
		}
	}
}