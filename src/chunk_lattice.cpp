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
	ClassDB::bind_method(D_METHOD("set_points_and_add_for_update", "global_idxs", "fullness_values", "material_values"), &ChunkLattice::set_points_and_add_for_update);
	ClassDB::bind_method(D_METHOD("work_through_queues"), &ChunkLattice::work_through_queues);
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
	for (int i = 0; i < num_chunk_generation_threads; i++) {
		chunk_generation_threads[i]->wait_to_finish(); // TODO do i need this?
	}
}

void ChunkLattice::setup(String p_file_world_name, Vector3i p_chunk_shape, Vector3 p_chunk_cube_size, int p_lattice_type, int p_lattice_seed) {
	file_world_name = p_file_world_name;
	chunk_shape = p_chunk_shape;
	chunk_cube_size = p_chunk_cube_size;
	lattice_type = p_lattice_type;
	lattice_seed = p_lattice_seed;

	make_world_dir(file_world_name);

	num_chunk_generation_threads = 8; // for now a fixed number
	cap_chunk_generation_passes = 1;
	for (int i = 0; i < num_chunk_generation_threads; i++) {
		Ref<Thread> w;
		chunk_generation_threads.push_back(w);
		chunk_generation_threads[i].instantiate();
	}

	Dictionary _cfg;
	_cfg["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	_cfg["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	_cfg["call_local"] = false;
	_cfg["channel"] = 0;
	rpc_config("client_update_mesh_chunk", _cfg);
	rpc_config("client_update_collision_chunk", _cfg);
    rpc_config("client_delete_chunk", _cfg);
}

void ChunkLattice::loader_enters_mesh_chunk(Object *loader, Vector3i chunk_idx) {
	ObjectID loader_obj_id = ObjectID(loader->get_instance_id());
	int peer_id = loader->get("player_client_id");

	bool chunk_loaded = loaded_chunks.has(chunk_idx);

	// isn't loaded -> add as child add to raw data and mesh queue
	if (!chunk_loaded) {
		// brand new load
		Chunk *c = memnew(Chunk);
		add_child(c); // voodoo, somehow placing this before setting the position fixed a bug
		c->set_position(Vector3(chunk_idx * (chunk_shape - Vector3i(1, 1, 1))) * chunk_cube_size);
		c->setup(file_world_name, chunk_idx, chunk_shape, chunk_cube_size, lattice_type, lattice_seed);
		loaded_chunks[chunk_idx] = c;

		chunk_data_queue.push_front(chunk_idx); // this chunk will get its' points filled in
		chunk_mesh_queue.push_front(chunk_idx); // this chunk will generate the mesh with the points from above and then set it
	}
	
	Chunk *c = loaded_chunks[chunk_idx];

	if (c->occupants.has(loader_obj_id)) {
		UtilityFunctions::printerr("loader tried to call loader_enters_mesh_chunk while being in the loaders list");
		return;
	}

	if (chunk_loaded && peer_id != -1 && c->mesh_resource_ready) { // checking for mesh_resource_ready because if it is not ready it is still in the queue and thus rpc will be sent on pop
		Dictionary d = c->get_point_changes();
		rpc_id(peer_id, StringName("client_update_mesh_chunk"), chunk_idx, d["changes_idx"], d["changes_fullness"], d["changes_material"]);
	}

	// mark self as occupant
	c->occupants[loader_obj_id] = {false, peer_id}; // collision, not yet TODO: ideally loader should be a cpp class, later...

	// rpc sent in queue handling
}

void ChunkLattice::loader_enters_collision_chunk(Object *loader, Vector3i chunk_idx) {
	ObjectID loader_obj_id = ObjectID(loader->get_instance_id());
	int peer_id = loader->get("player_client_id");

	if (!loaded_chunks.has(chunk_idx)) {
		UtilityFunctions::printerr("loader tried to call loader_enters_collision_chunk on a non-loaded chunk", chunk_idx);
		return;
	}

	Chunk *c = loaded_chunks[chunk_idx];
	if (!c->set_collision) { // loader doesn't change data so this is fine, client can't make this assumption however
		chunk_collision_queue.push_front(chunk_idx); // this chunk will get its' collision resource set from the mesh
	} else if (peer_id != -1) {
		rpc_id(peer_id, StringName("client_update_collision_chunk"), chunk_idx);
	}

	// mark self as occupant
	c->occupants[loader_obj_id] = {true, peer_id}; // setting the flag for collision TODO: ideally loader should be a cpp class, later...

	// rpc sent in queue handling
}

void ChunkLattice::loader_exits_chunk(Object *loader, Vector3i chunk_idx) {
	ObjectID loader_obj_id = ObjectID(loader->get_instance_id());
	int peer_id = loader->get("player_client_id");

	if (!loaded_chunks.has(chunk_idx)) {
		// this should not happen
		UtilityFunctions::printerr("server: tried to call a loader_exits_chunk on a non-loaded chunk", chunk_idx);
		return;
	}

	Chunk *c = loaded_chunks[chunk_idx];
	// remove self from occupants
	c->occupants.erase(loader_obj_id);
	// if no occupants left
	if (c->occupants.is_empty()) {
		// unloading this chunk as no one is in it
		// even if it is left in the queues, when it is reached lattice will see that it is not loaded, meaning it didn't have enough time to load and will skip it
		c->queue_free();
		loaded_chunks.erase(chunk_idx);
	}

	// deletion is not threaded and thus not queued so send rpc right away
	if (peer_id != -1) rpc_id(peer_id, StringName("client_delete_chunk"), chunk_idx);
}

// two following functions are only called on clients
void ChunkLattice::client_update_mesh_chunk(Vector3i chunk_idx, PackedInt32Array changes_idxs, PackedFloat32Array changes_fullness_values, PackedByteArray changes_material_values) {
	// loaders arent added as they only matter on server side
	if (!loaded_chunks.has(chunk_idx)) {
		// brand new load
		Chunk *c = memnew(Chunk);
		add_child(c);
		c->set_position(Vector3(chunk_idx * (chunk_shape - Vector3i(1, 1, 1))) * chunk_cube_size);
		c->setup(file_world_name, chunk_idx, chunk_shape, chunk_cube_size, lattice_type, lattice_seed);
		loaded_chunks[chunk_idx] = c;
		

		// unlike server side we have to manually set the changes hash
		c->add_point_hash_changes(changes_idxs, changes_fullness_values, changes_material_values);
		chunk_data_queue.push_front(chunk_idx); // this chunk will get its' points filled in
		chunk_mesh_queue.push_front(chunk_idx); // this chunk will generate the mesh with the points from above and then set it
	} else {
		// chunk is already loaded
		Chunk *c = loaded_chunks[chunk_idx];
		c->add_point_hash_changes(changes_idxs, changes_fullness_values, changes_material_values); // no need to thread
		c->apply_point_hash_changes(); // also need to apply it manually as raw points are already loaded
		c->mesh_resource_ready = false; // in case it gets popped earlier than collision
		chunk_mesh_queue.push_front(chunk_idx); // just need to regenerate the mesh
	}
}

void ChunkLattice::client_update_collision_chunk(Vector3i chunk_idx) { // called after mesh, if client was also in occupants_collision
	if (!loaded_chunks.has(chunk_idx)) {
		UtilityFunctions::printerr("client tried to call client_set_collision_chunk on a non-loaded chunk", chunk_idx);
		return;
	}
	
	chunk_collision_queue.push_front(chunk_idx); // set the collision
}

void ChunkLattice::client_delete_chunk(Vector3i chunk_idx) {
	if (!loaded_chunks.has(chunk_idx)) { // server did not finish chunk, did not tell us to make it yet also rpcs are reliable and ordered so thankfully it is not possible to get a mesh rpc for a deleted chunk
		// UtilityFunctions::printerr("client: tried to call a loader_exits_chunk on a non-loaded chunk", chunk_idx);
		return;
	}

	Chunk *c = loaded_chunks[chunk_idx];
	// unloading this chunk as we were told by rpc that we left
	loaded_chunks.erase(chunk_idx); // as mentioned this is safe with threads
	c->queue_free();
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
			UtilityFunctions::printerr("tried getting a value of a non-loaded chunk", chunk_idx);
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

void ChunkLattice::set_points_and_add_for_update(PackedVector3Array global_idxs, PackedFloat32Array fullness_values, PackedByteArray material_values) {
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
	for (const KeyValue<Vector3i, Modifications> &kv : chunk_buckets) {
		Vector3i _chunk_idx = kv.key;

		// raise error if chunk is not loaded
		if (!loaded_chunks.has(_chunk_idx)) {
			UtilityFunctions::printerr("tried setting a value of a non-loaded chunk", _chunk_idx);
			return;
		}

		Chunk *c = loaded_chunks[_chunk_idx];

		// apply changes and regenerate mesh, if collision was set regenerate it as well
		c->add_point_hash_changes(kv.value.local_idxs, kv.value.fullness, kv.value.materials);
		// don't apply them, will be applied when popped, applying now used to crash if chunk was still being processed
		
		// add to queue only if chunk already went through the process of mesh, because otherwise it is currently doing it
		if (c->mesh_resource_ready) {
			c->mesh_resource_ready = false; // in case it gets popped earlier than collision
			chunk_mesh_queue.push_front(_chunk_idx); // request mesh is recalculated and re-applied
		}
		
		if (c->set_collision) chunk_collision_queue.push_front(_chunk_idx); // and then reapply collision
	}
}

void ChunkLattice::work_through_queues() {
	// NOTE: data load can't happen without mesh load, however mesh can happen alone, so apply_point_hash_changes is run in mesh
	int _max_len = MAX(MAX(chunk_data_queue.size(), chunk_mesh_queue.size()), chunk_collision_queue.size());
	int num_passes = MIN(cap_chunk_generation_passes, (_max_len + num_chunk_generation_threads - 1) / num_chunk_generation_threads);

	for (int pass = 0; pass < num_passes; pass++) {
		int n;
		int chunk_i;
		
		// data queue
		int _data_n = chunk_data_queue.size();
		n = num_chunk_generation_threads < _data_n ? num_chunk_generation_threads : _data_n;

		std::vector<Chunk*> _modified_chunks;
		// start threads
		for (chunk_i = 0; chunk_i < n;) {
			if (chunk_data_queue.empty()) break;
			Vector3i _chunk_idx = chunk_data_queue.back();
			chunk_data_queue.pop_back();
			if (!loaded_chunks.has(_chunk_idx)) continue; // chunk was deleted, skip without incrementing i
			Chunk* c = loaded_chunks[_chunk_idx];

			chunk_generation_threads[chunk_i]->start(callable_mp(c, &Chunk::set_data));
			_modified_chunks.push_back(c);

			chunk_i++;
		}

		// wait for threads to finish
		for (int t = 0; t < chunk_i; t++) {
			chunk_generation_threads[t]->wait_to_finish(); // joining started threads
		}
		
		
		_modified_chunks.clear();
		// mesh data
		int _mesh_n = chunk_mesh_queue.size();
		n = num_chunk_generation_threads < _mesh_n ? num_chunk_generation_threads : _mesh_n;

		// start threads
		for (chunk_i = 0; chunk_i < n;) {
			if (chunk_mesh_queue.empty()) break;
			Vector3i _chunk_idx = chunk_mesh_queue.back();
			chunk_mesh_queue.pop_back();
			if (!loaded_chunks.has(_chunk_idx)) continue; // chunk was deleted, skip without incrementing i
			
			Chunk* c = loaded_chunks[_chunk_idx];

			// chunk is here either because it's brand new or an update happened, so need to apply changes
			// obviously we need this before we make the mesh
			c->apply_point_hash_changes();
			
			chunk_generation_threads[chunk_i]->start(callable_mp(c, &Chunk::set_mesh_data));
			_modified_chunks.push_back(c);

			chunk_i++;
		}

		// wait for threads to finish and apply resource changes
		for (int t = 0; t < chunk_i; t++) {
			chunk_generation_threads[t]->wait_to_finish(); // joining started threads
			Chunk* c = _modified_chunks[t];
			
			// send rpc
			Dictionary d = c->get_point_changes(); // TODO this could be optimised but it's not significant (tested)
			for (const KeyValue<ObjectID, Chunk::LoaderAttributes> &kv : c->occupants) {
				if (kv.value.peer_id == -1) continue;
				rpc_id(kv.value.peer_id, StringName("client_update_mesh_chunk"), c->chunk_idx, d["changes_idx"], d["changes_fullness"], d["changes_material"]);
			}
			
			c->assign_mesh();
		}

		// collision (non-threaded)
		int _collision_n = chunk_collision_queue.size();
		n = num_chunk_generation_threads < _collision_n ? num_chunk_generation_threads : _collision_n;

		std::vector<Vector3i> _deferred;          // not ready this pass
		int examined = 0;
		for (chunk_i = 0; chunk_i < n && examined < _collision_n; ) {
			if (chunk_collision_queue.empty()) break;
			Vector3i _chunk_idx = chunk_collision_queue.back();
			chunk_collision_queue.pop_back();
			examined++;

			if (!loaded_chunks.has(_chunk_idx)) continue;   // deleted, drop it
			Chunk *c = loaded_chunks[_chunk_idx];

			if (!c->mesh_resource_ready) { // mesh still pending
				_deferred.push_back(_chunk_idx);
				continue;
			}

			if (!c->set_collision) { // if two loaders at the same time request, will ahve same chunk in one queue
				c->assign_generated_collision();
			}

			for (const KeyValue<ObjectID, Chunk::LoaderAttributes> &kv : c->occupants) {
				if (kv.value.peer_id == -1) continue;
				rpc_id(kv.value.peer_id, StringName("client_update_collision_chunk"), c->chunk_idx);
			}

			chunk_i++;
		}

		// put the not-ready ones back for a later pass
		for (const Vector3i &idx : _deferred) chunk_collision_queue.push_front(idx);
	}
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