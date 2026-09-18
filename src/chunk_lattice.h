#ifndef CHUNK_LATTICE_H
#define CHUNK_LATTICE_H

#include "chunk.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/dir_access.hpp>
// #include <godot_cpp/classes/thread.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>

#include <deque>

namespace godot {
/*
ChunkLattice is responsible for:
loading / unloading chunks - adding deleting as it's children, this requires: generating chunk data, io to disk
applying changes to chunks - modifying and regenarating the chunks
*/
class ChunkLattice : public Node3D {
	GDCLASS(ChunkLattice, Node3D)

private:

protected:
	static void _bind_methods();

public:
	String file_world_name;

	Vector3i chunk_shape;
	Vector3 chunk_cube_size;
	int lattice_type;
	int lattice_seed;

	int num_chunk_generation_threads;
	// std::vector<Ref<Thread>> chunk_generation_threads; TODO
	std::deque<Vector3i> chunk_data_queue;
	std::deque<Vector3i> chunk_mesh_queue;
	std::deque<Vector3i> chunk_collision_queue;

	HashMap<Vector3i, Chunk *> loaded_chunks;
	
	ChunkLattice();
	~ChunkLattice();

	void setup(String p_file_world_name, Vector3i p_chunk_shape, Vector3 p_chunk_size, int p_lattice_type, int p_lattice_seed);

	void loader_enters_mesh_chunk(Object *loader, Vector3i chunk_idx);
	void loader_enters_collision_chunk(Object *loader, Vector3i chunk_idx);
	void loader_exits_chunk(Object *loader, Vector3i chunk_idx);

	void client_update_mesh_chunk(Vector3i chunk_idx, PackedInt32Array changes_idxs, PackedFloat32Array changes_fullness_values, PackedByteArray changes_material_values);
	void client_update_collision_chunk(Vector3i chunk_idx);
	void client_delete_chunk(Vector3i chunk_idx);

	Dictionary get_points(PackedVector3Array global_idxs);
	PackedVector3Array set_points_and_add_for_update(PackedVector3Array global_idxs, PackedFloat32Array fullness_values, PackedByteArray material_values);

	void work_through_queues();

	Chunk *get_chunk(Vector3i chunk_idx);
};
}

#endif