extends Node

@onready var Main
@onready var CL: ChunkLattice

func _ready():
	call_deferred("_grab_references")

func _grab_references():
	if (not multiplayer.is_server()): return
	Main = get_tree().root.get_node("main")
	CL = Main.get_node("ChunkLattice")

func get_global_idx(global_pos):
	var global_idx = Vector3i(
		floori(global_pos.x / Main.c_cube_size.x),
		floori(global_pos.y / Main.c_cube_size.y),
		floori(global_pos.z / Main.c_cube_size.z)
	)
	return global_idx

#func send_updates(affected_chunk_idxs):
	#for _f_idx in affected_chunk_idxs:
		#var chunk_idx = Vector3i(_f_idx.round())
		#var chunk: Chunk = CL.get_chunk(chunk_idx)
		#var occupants_arr = chunk.get_occupants()
		#for occ_d in occupants_arr:
			#var loader_instance: Loader = occ_d["loader"]
			#var collision = occ_d["collision"]
			#
			#if loader_instance.player_client_id != -1:
				#var chunk_changes_d = chunk.get_point_changes()
				#CL.rpc_client_update_mesh_chunk.rpc_id(loader_instance.player_client_id, chunk_idx, chunk_changes_d["changes_idx"], chunk_changes_d["changes_fullness"], chunk_changes_d["changes_material"])
				#if collision:
					#CL.rpc_client_update_collision_chunk.rpc_id(loader_instance.player_client_id, chunk_idx)


func uniform_dumb_overwrite_cube_set(cube_corner: Vector3i, cube_side: int, fullness: float, material: int):
	if (not multiplayer.is_server()): return
	
	# create change data
	var global_idxs = PackedVector3Array(); global_idxs.resize((cube_side-1) ** 3)
	var fullness_values = PackedFloat32Array(); fullness_values.resize((cube_side-1) ** 3)
	var material_values = PackedByteArray(); material_values.resize((cube_side-1) ** 3)
	
	var i = 0
	for dx in range(cube_side - 1):
		for dy in range(cube_side - 1):
			for dz in range(cube_side - 1):
				var _glob_idx = cube_corner + Vector3i(dx, dy, dz)
				
				global_idxs[i] = Vector3(_glob_idx)
				fullness_values[i] = fullness
				material_values[i] = material
				
				i += 1
	
	# apply changes on server side and get idx of affected chunks
	CL.set_points_and_add_for_update(global_idxs, fullness_values, material_values)
