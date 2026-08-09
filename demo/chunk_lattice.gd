extends ChunkLattice

@onready var Main = get_tree().root.get_node("main")

# loading and applying updates to chunks
@rpc("authority", "call_remote", "reliable")
func rpc_client_update_mesh_chunk(chunk_idx: Vector3i, changes_idxs: PackedInt32Array, changes_fullness_values: PackedFloat32Array, changes_material_values: PackedByteArray):
	client_update_mesh_chunk(chunk_idx, changes_idxs, changes_fullness_values, changes_material_values)

@rpc("authority", "call_remote", "reliable")
func rpc_client_update_collision_chunk(chunk_idx: Vector3i):
	client_update_collision_chunk(chunk_idx)

@rpc("authority", "call_remote", "reliable")
func rpc_client_delete_chunk(chunk_idx: Vector3i):
	client_delete_chunk(chunk_idx)
