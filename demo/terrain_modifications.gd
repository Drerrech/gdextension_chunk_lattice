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
		round(global_pos.x / Main.c_cube_size.x),
		round(global_pos.y / Main.c_cube_size.y),
		round(global_pos.z / Main.c_cube_size.z)
	)
	return global_idx

func uniform_dumb_overwrite_cube_set(cube_corner: Vector3i, cube_side: int, fullness: float, material: int):
	if (not multiplayer.is_server()): return
	
	# create change data
	var global_idxs = PackedVector3Array(); global_idxs.resize((cube_side) ** 3)
	var fullness_values = PackedFloat32Array(); fullness_values.resize((cube_side) ** 3)
	var material_values = PackedByteArray(); material_values.resize((cube_side) ** 3)
	
	var i = 0
	for dx in range(cube_side):
		for dy in range(cube_side):
			for dz in range(cube_side):
				var _glob_idx = cube_corner + Vector3i(dx, dy, dz)
				
				global_idxs[i] = Vector3(_glob_idx)
				fullness_values[i] = fullness
				material_values[i] = material
				
				i += 1
	
	# apply changes on server side and get idx of affected chunks
	CL.set_points_and_update(global_idxs, fullness_values, material_values)

func uniform_dumb_overwrite_sphere_set(sphere_center: Vector3i, sphere_rad: int, fullness: float, material: int):
	if (not multiplayer.is_server()): return
	
	# create change data
	var global_idxs = PackedVector3Array();
	var fullness_values = PackedFloat32Array();
	var material_values = PackedByteArray();
	
	for dx in range(-sphere_rad, sphere_rad+1):
		for dy in range(-sphere_rad, sphere_rad+1):
			for dz in range(-sphere_rad, sphere_rad+1):
				var dist_sq = dx**2 + dy**2 + dz**2
				if dist_sq <= sphere_rad**2:
					var _glob_idx = sphere_center + Vector3i(dx, dy, dz)
					
					global_idxs.append(Vector3(_glob_idx))
					fullness_values.append(fullness)
					material_values.append(material)
	
	# apply changes on server side and get idx of affected chunks
	CL.set_points_and_update(global_idxs, fullness_values, material_values)
