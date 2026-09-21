#ifndef CHUNK_LOADER_H
#define CHUNK_LOADER_H

#include "chunk_lattice.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <vector>

namespace godot {
/*
ChunkLoader is responsible for:
requesting to load/unload chunks on server side (only active on server side)
*/
class ChunkLoader : public Node3D {
	GDCLASS(ChunkLoader, Node3D)

private:

protected:
	static void _bind_methods();

public:
	ChunkLattice* lattice_ptr;
	int peer_id;
	Vector3i global_idx;
	Vector3i central_chunk_idx;
	int mesh_rad;
	int collision_rad;
	HashSet<Vector3i> mesh_occupied;
	HashSet<Vector3i> collision_occupied;

	Vector3i stride;
	Vector3i box_half;
	Vector3i collision_half;

	ChunkLoader();
	~ChunkLoader();

	void _exit_tree() override;

	void compute_idxs();
	Vector3i hash_vec3i_to_range(Vector3i p);
	void setup(ChunkLattice* p_lattice_ptr, int p_peer_id, int p_mesh_rad, int p_collision_rad);

	void check_and_load();
};
}

#endif