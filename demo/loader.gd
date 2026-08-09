class_name Loader
extends Node3D

@onready var Main = get_tree().root.get_node("main")

 # 0 - no collision loaded, 1 - one chunk 2 - 3x3x3
@export var mesh_load_cube_rad = 3
var currently_mesh_loaded = []
@export var collision_load_cube_rad = 2
var currently_collision_loaded = []
@export var player_client_id = -1 # -1 if not client

var global_idx: Vector3i;
var stride: Vector3i;
var chunk_idx: Vector3i;
var local_idx: int;

# server:
# loaders call loader entered/exit functions -> chunks locally spawn, get collision and get deleted (if no loaders)
# in addition if loader is player_type: send rpc mesh/collision/deletion
func compute_idxs():
	global_idx = Vector3i(
		floori(global_position.x / Main.c_cube_size.x),
		floori(global_position.y / Main.c_cube_size.y),
		floori(global_position.z / Main.c_cube_size.z)
	)
	stride = Main.c_shape - Vector3i(1, 1, 1)
	chunk_idx = Vector3i(
		floori(global_idx.x / float(stride.x)),
		floori(global_idx.y / float(stride.y)),
		floori(global_idx.z / float(stride.z))
	)
	var _v = global_idx - chunk_idx * stride
	local_idx = _v.x * Main.c_shape.y * Main.c_shape.z + _v.y * Main.c_shape.z + _v.z

func enter_mesh_chunk(p_chunk_idx: Vector3i):
	var c: Chunk = Main.CL.loader_enters_mesh_chunk(self, p_chunk_idx)
	if c == null:
		printerr("C IS NULL!!!")
	if player_client_id != -1: # player type
		var d = c.get_point_changes()
		Main.CL.rpc_client_update_mesh_chunk.rpc_id(player_client_id, p_chunk_idx, d["changes_idx"], d["changes_fullness"], d["changes_material"])

func enter_collision_chunk(p_chunk_idx: Vector3i):
	Main.CL.loader_enters_collision_chunk(self, p_chunk_idx)
	if player_client_id != -1: # player type
		Main.CL.rpc_client_update_collision_chunk.rpc_id(player_client_id, p_chunk_idx)

func exit_chunk(p_chunk_idx: Vector3i):
	Main.CL.loader_exits_chunk(self, p_chunk_idx)
	if player_client_id != -1: # player type
		Main.CL.rpc_client_delete_chunk.rpc_id(player_client_id, p_chunk_idx)

func setup() -> void:
	if (not multiplayer.is_server()): return
	compute_idxs()
	
	# mesh-enter all
	for dx in range(1 - mesh_load_cube_rad, mesh_load_cube_rad):
		for dy in range(1 - mesh_load_cube_rad, mesh_load_cube_rad):
			for dz in range(1 - mesh_load_cube_rad, mesh_load_cube_rad):
				var _offset = Vector3i(dx, dy, dz)
				var _chunk_to_load_idx = chunk_idx + _offset
				currently_mesh_loaded.append(_chunk_to_load_idx)
				enter_mesh_chunk(_chunk_to_load_idx)
	
	# collision-enter all
	for dx in range(1 - collision_load_cube_rad, collision_load_cube_rad):
		for dy in range(1 - collision_load_cube_rad, collision_load_cube_rad):
			for dz in range(1 - collision_load_cube_rad, collision_load_cube_rad):
				var _offset = Vector3i(dx, dy, dz)
				var _chunk_to_load_idx = chunk_idx + _offset
				currently_collision_loaded.append(_chunk_to_load_idx)
				enter_collision_chunk(_chunk_to_load_idx)

func _exit_tree() -> void:
	if (not multiplayer.is_server()): return
	
	# exit all
	for _idx in currently_mesh_loaded:
		Main.CL.loader_exits_chunk(self, _idx)
		# ignoring client as godot already deleted it
	currently_mesh_loaded.clear() # in case of reparanting
	currently_collision_loaded.clear()

func check_and_load() -> void:
	if (not multiplayer.is_server()): return
	
	var old_chunk_idx = chunk_idx
	compute_idxs()
	if (old_chunk_idx == chunk_idx): return
	
	var old_mesh_loaded = currently_mesh_loaded.duplicate()
	var old_collision_loaded = currently_collision_loaded.duplicate()
	currently_mesh_loaded.clear()
	currently_collision_loaded.clear()
	
	# mesh
	for dx in range(1 - mesh_load_cube_rad, mesh_load_cube_rad):
		for dy in range(1 - mesh_load_cube_rad, mesh_load_cube_rad):
			for dz in range(1 - mesh_load_cube_rad, mesh_load_cube_rad):
				var _offset = Vector3i(dx, dy, dz)
				var _chunk_to_load_idx = chunk_idx + _offset
				currently_mesh_loaded.append(_chunk_to_load_idx)
	
	# collision
	for dx in range(1 - collision_load_cube_rad, collision_load_cube_rad):
		for dy in range(1 - collision_load_cube_rad, collision_load_cube_rad):
			for dz in range(1 - collision_load_cube_rad, collision_load_cube_rad):
				var _offset = Vector3i(dx, dy, dz)
				var _chunk_to_load_idx = chunk_idx + _offset
				currently_collision_loaded.append(_chunk_to_load_idx)
	
	# mesh enter new ones
	for new_idx in currently_mesh_loaded:
		if not (new_idx in old_mesh_loaded):
			enter_mesh_chunk(new_idx)
	
	# collision enter new ones
	for new_idx in currently_collision_loaded:
		if not (new_idx in old_collision_loaded):
			enter_collision_chunk(new_idx)
	
	# exit old that did not make it
	for old_idx in old_mesh_loaded:
		if not (old_idx in currently_mesh_loaded):
			exit_chunk(old_idx)
