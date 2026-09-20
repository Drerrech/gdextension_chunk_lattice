#ifndef CHUNK_LATTICE_H
#define CHUNK_LATTICE_H

#include "chunk.h"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>

namespace godot {

class ChunkLoader;

static inline int floordiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}
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

	HashMap<Vector3i, Chunk *> loaded_chunks;
	
	ChunkLattice();
	~ChunkLattice();

	void setup(String p_file_world_name, Vector3i p_chunk_shape, Vector3 p_chunk_size, int p_lattice_type, int p_lattice_seed);

	Chunk *loader_enters_mesh_chunk(ChunkLoader *loader, Vector3i chunk_idx);
	Chunk *loader_enters_collision_chunk(ChunkLoader *loader, Vector3i chunk_idx);
	void loader_exits_chunk(ChunkLoader *loader, Vector3i chunk_idx);

	void client_update_mesh_chunk(Vector3i chunk_idx, PackedInt32Array changes_idxs, PackedFloat32Array changes_fullness_values, PackedByteArray changes_material_values);
	void client_update_collision_chunk(Vector3i chunk_idx);
	void client_delete_chunk(Vector3i chunk_idx);

	Dictionary get_points(PackedVector3Array global_idxs);
	void set_points_and_update(PackedVector3Array global_idxs, PackedFloat32Array fullness_values, PackedByteArray material_values);

	Chunk *get_chunk(Vector3i chunk_idx);
};
}

#endif