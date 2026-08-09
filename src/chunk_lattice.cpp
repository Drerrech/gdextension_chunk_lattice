#include "chunk_lattice.h"

#include "constants.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void ChunkLattice::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "file_world_name", "chunk_shape", "chunk_cube_size", "lattice_type", "lattice_seed"), &ChunkLattice::setup);
	ClassDB::bind_method(D_METHOD("loader_enters_mesh_chunk", "loader", "chunk_idx"), &ChunkLattice::loader_enters_mesh_chunk);
	ClassDB::bind_method(D_METHOD("loader_enters_collision_chunk", "loader", "chunk_idx"), &ChunkLattice::loader_enters_collision_chunk);
	ClassDB::bind_method(D_METHOD("loader_exits_chunk", "loader", "chunk_idx"), &ChunkLattice::loader_exits_chunk);
	ClassDB::bind_method(D_METHOD("client_update_mesh_chunk", "chunk_idx", "changes_idxs", "changes_fullness_values", "changes_material_values"), &ChunkLattice::client_update_mesh_chunk);
	ClassDB::bind_method(D_METHOD("client_update_collision_chunk", "chunk_idx"), &ChunkLattice::client_update_collision_chunk);
	ClassDB::bind_method(D_METHOD("client_delete_chunk", "chunk_idx"), &ChunkLattice::client_delete_chunk);
	ClassDB::bind_method(D_METHOD("get_points", "global_idxs"), &ChunkLattice::get_points);
	ClassDB::bind_method(D_METHOD("set_points_and_update", "global_idxs", "fullness_values", "material_values"), &ChunkLattice::set_points_and_update);
	ClassDB::bind_method(D_METHOD("get_chunk", "chunk_idx"), &ChunkLattice::get_chunk);
}

void make_world_dir(String p_file_world_name) {
	if (p_file_world_name == client_file_world_name_flag) return;
	
	DirAccess::make_dir_recursive_absolute(String("user://").path_join(p_file_world_name).path_join("chunk_changes"));
	// other stuff
}

ChunkLattice::ChunkLattice() {
	file_world_name = default_file_world_name;
	chunk_shape = default_chunk_shape;
	chunk_cube_size = default_chunk_cube_size;
	lattice_type = default_lattice_type;
	lattice_seed = default_lattice_seed;
	// need to call setup to use
}

ChunkLattice::~ChunkLattice() {
	
}

void ChunkLattice::setup(String p_file_world_name, Vector3i p_chunk_shape, Vector3 p_chunk_cube_size, int p_lattice_type, int p_lattice_seed) {
	file_world_name = p_file_world_name;
	chunk_shape = p_chunk_shape;
	chunk_cube_size = p_chunk_cube_size;
	lattice_type = p_lattice_type;
	lattice_seed = p_lattice_seed;

	make_world_dir(file_world_name);
}

Chunk *ChunkLattice::loader_enters_mesh_chunk(Object *loader, Vector3i chunk_idx) {
	ObjectID loader_obj_id = ObjectID(loader->get_instance_id());

	if (!loaded_chunks.has(chunk_idx)) {
		// brand new load
		Chunk *c = memnew(Chunk); // fills in raw points, loads changes, constructs mesh
		add_child(c); // voodoo, somehow placing this before setting the position fixed a bug
		c->setup(file_world_name, chunk_shape, chunk_cube_size, lattice_type, lattice_seed);
		c->set_position(Vector3(chunk_idx * (chunk_shape - Vector3i(1, 1, 1))) * chunk_cube_size);
		c->initial_build(); // depends on global position
		loaded_chunks[chunk_idx] = c;
	}

	// below chunk might have been already loaded (or not)
	Chunk *c = loaded_chunks[chunk_idx];

	if (c->occupants.has(loader_obj_id)) {
		UtilityFunctions::printerr("loader tried to call loader_enters_mesh_chunk while being in the loaders list");
		return nullptr;
	}
	
	c->occupants[loader_obj_id] = {false}; // collision, not yet

	return c;
}

Chunk *ChunkLattice::loader_enters_collision_chunk(Object *loader, Vector3i chunk_idx) {
	ObjectID loader_obj_id = ObjectID(loader->get_instance_id());

	if (!loaded_chunks.has(chunk_idx)) {
		UtilityFunctions::printerr("loader tried to call loader_enters_collision_chunk on a non-loaded chunk");
		return nullptr;
	}

	Chunk *c = loaded_chunks[chunk_idx];
	if (!c->set_collision) { // loader doesn't change data so this is fine, client can't make this assumption however
		c->set_generated_collision();
	}

	c->occupants[loader_obj_id] = {true}; // setting the flag for collision

	return c;
}

void ChunkLattice::loader_exits_chunk(Object *loader, Vector3i chunk_idx) {
	ObjectID loader_obj_id = ObjectID(loader->get_instance_id());

	if (!loaded_chunks.has(chunk_idx)) {
		// this should not happen
		UtilityFunctions::printerr("tried to call a loader_exits_chunk on a non-loaded chunk");
		return;
	}

	Chunk *c = loaded_chunks[chunk_idx];
	c->occupants.erase(loader_obj_id);
	if (c->occupants.is_empty()) {
		// unloading this chunk as no one is in it
		c->queue_free();
		loaded_chunks.erase(chunk_idx);
	}
}

// two following functions are only called on clients
void ChunkLattice::client_update_mesh_chunk(Vector3i chunk_idx, PackedInt32Array changes_idxs, PackedFloat32Array changes_fullness_values, PackedByteArray changes_material_values) {
	// loaders arent added as they only matter on server side
	if (!loaded_chunks.has(chunk_idx)) {
		// brand new load
		Chunk *c = memnew(Chunk); // fills in raw points, loads changes, constructs mesh
		add_child(c);
		c->setup(file_world_name, chunk_shape, chunk_cube_size, lattice_type, lattice_seed);
		c->set_position(Vector3(chunk_idx * (chunk_shape - Vector3i(1, 1, 1))) * chunk_cube_size);
		c->set_raw_generation_points(); // depends on global position, and no initial build needed
		c->apply_point_changes(changes_idxs, changes_fullness_values, changes_material_values); // setting the change arrays first
		c->set_generated_mesh(); // no initial build used so manually
		loaded_chunks[chunk_idx] = c;
	} else {
		// chunk is already loaded
		Chunk *c = loaded_chunks[chunk_idx];
		c->apply_point_changes(changes_idxs, changes_fullness_values, changes_material_values);
		c->set_generated_mesh();
	}
}

void ChunkLattice::client_update_collision_chunk(Vector3i chunk_idx) { // called after mesh, if client was also in occupants_collision
	if (!loaded_chunks.has(chunk_idx)) {
		UtilityFunctions::printerr("client tried to call client_set_collision_chunk on a non-loaded chunk");
		return;
	}
	
	Chunk *c = loaded_chunks[chunk_idx];
	c->set_generated_collision();
}

void ChunkLattice::client_delete_chunk(Vector3i chunk_idx) {
	if (!loaded_chunks.has(chunk_idx)) {
		UtilityFunctions::printerr("tried to call a loader_exits_chunk on a non-loaded chunk");
		return;
	}

	Chunk *c = loaded_chunks[chunk_idx];
	// unloading this chunk as we were told by rpc that we left
	c->queue_free();
	loaded_chunks.erase(chunk_idx);
}

int floordiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

Dictionary ChunkLattice::get_points(PackedVector3Array global_idxs) {
	uint64_t _size = global_idxs.size();
	PackedFloat32Array fullness_values; fullness_values.resize(_size);
	PackedByteArray material_values; material_values.resize(_size);
	
	// iterate through the global indexes, calculate what chunk it is (on bound any chunk matters), calculate local index, get point values
	Vector3i stride = chunk_shape - Vector3i(1, 1, 1);
	for (int64_t i = 0; i < (int64_t)_size; i++) {
		Vector3i global_idx(global_idxs[i]);

		Vector3i chunk_idx(
			floordiv(global_idx.x, stride.x),
			floordiv(global_idx.y, stride.y),
			floordiv(global_idx.z, stride.z)
		);
		Vector3i local_idx = global_idx - chunk_idx * stride;

		// raise error if chunk is not loaded
		if (!loaded_chunks.has(chunk_idx)) {
			UtilityFunctions::printerr("tried getting a value of a non-loaded chunk");
			return Dictionary();
		}
		
		// get the point values
		Chunk *c = loaded_chunks[chunk_idx];
		int _flat_idx = c->get_idx(local_idx.x, local_idx.y, local_idx.z);
		fullness_values.set(i, c->point_fullness_values[_flat_idx]);
		material_values.set(i, c->point_material_values[_flat_idx]);
	}

	Dictionary points;
	points[String("fullness")] = fullness_values;
	points[String("material")] = material_values;

	return points;
}

PackedVector3Array ChunkLattice::set_points_and_update(PackedVector3Array global_idxs, PackedFloat32Array fullness_values, PackedByteArray material_values) {
	// iterate through each global index, add to appropriate chunk bucket(s), after that update each chunk
	uint64_t _size = global_idxs.size();
	struct Modifications {
		PackedInt32Array   local_idxs;
		PackedFloat32Array fullness;
		PackedByteArray    materials;
	};
	HashMap<Vector3i, Modifications> chunk_buckets;

	Vector3i stride = chunk_shape - Vector3i(1, 1, 1);
	for (int64_t i = 0; i < (int64_t)_size; i++) {
		Vector3i global_idx(global_idxs[i]);

		Vector3i center_chunk_idx(
			floordiv(global_idx.x, stride.x),
			floordiv(global_idx.y, stride.y),
			floordiv(global_idx.z, stride.z)
		);
		Vector3i center_chunk_local_idx = global_idx - center_chunk_idx * stride;

		// area_chunks
		bool lower_bound_x = (center_chunk_local_idx.x == 0);
		bool lower_bound_y = (center_chunk_local_idx.y == 0);
		bool lower_bound_z = (center_chunk_local_idx.z == 0);
		for (int dx = (lower_bound_x ? -1 : 0); dx <= 0; dx++) {
			for (int dy = (lower_bound_y ? -1 : 0); dy <= 0; dy++) {
				for (int dz = (lower_bound_z ? -1 : 0); dz <= 0; dz++) {
					Vector3i area_chunk_idx = center_chunk_idx + Vector3i(dx, dy, dz);
					Vector3i area_chunk_local_idx(
						dx == -1 ? stride.x : center_chunk_local_idx.x,
						dy == -1 ? stride.y : center_chunk_local_idx.y,
						dz == -1 ? stride.z : center_chunk_local_idx.z
					);
					int _area_chunk_flat_idx = area_chunk_local_idx.x * chunk_shape.y * chunk_shape.z + area_chunk_local_idx.y * chunk_shape.z + area_chunk_local_idx.z;
					Modifications &m = chunk_buckets[area_chunk_idx];
					m.local_idxs.append(_area_chunk_flat_idx);
					m.fullness.append(fullness_values[i]);
					m.materials.append(material_values[i]);
				}
			}
		}
	}

	// iterate throgh the buckets and update chunks
	PackedVector3Array updated_chunk_idxs; updated_chunk_idxs.resize(chunk_buckets.size());
	int64_t i = 0;
	for (const KeyValue<Vector3i, Modifications> &kv : chunk_buckets) {
		Vector3i _chunk_idx = kv.key;

		// raise error if chunk is not loaded
		if (!loaded_chunks.has(_chunk_idx)) {
			UtilityFunctions::printerr("tried setting a value of a non-loaded chunk");
			return PackedVector3Array();
		}

		updated_chunk_idxs.set(i, Vector3(_chunk_idx));

		Chunk *c = loaded_chunks[_chunk_idx];

		// apply changes and regenerate mesh, if collision was set regenerate it as well
		c->apply_point_changes(kv.value.local_idxs, kv.value.fullness, kv.value.materials);
		c->set_generated_mesh();
		if (c->set_collision) c->set_generated_collision();

		i++;
	}

	// return the chunk idxs that were influenced
	return updated_chunk_idxs;
}

Chunk *ChunkLattice::get_chunk(Vector3i chunk_idx) {
	Chunk **cp = loaded_chunks.getptr(chunk_idx);
    if (cp) {
		return *cp;
	} else {
		UtilityFunctions::printerr("get_chunk called on a non-existent chunk");
		return nullptr;
	}
}