extends Node3D

@onready var Main = get_tree().root.get_node("main")

@onready var loader = $ChunkLoader
@onready var ray = $RayCast3D

var travel_dir: Vector3

var explosion_rad: int = 3

func _ready() -> void:
	if multiplayer.is_server():
		loader.setup(Main.CL, -1, 2, 2)

func _physics_process(delta: float) -> void:
	if multiplayer.is_server():
		loader.check_and_load()
		
		travel_dir.y -= 10 * delta
		ray.target_position = 0.1 * travel_dir
		
		global_position += travel_dir * delta
		
		if ray.is_colliding():
			var col = ray.get_collider()
			if col.is_in_group("cannon"):
				col.queue_free()
			
			Main.m_spawner.rpc_spawn.rpc_id(1, {"type": "explosion", "glob_pos": global_position, "rad": explosion_rad})
			var global_idx = TerrainModifications.get_global_idx(ray.get_collision_point())
			#TerrainModifications.uniform_dumb_overwrite_cube_set(global_idx - explosion_rad*Vector3i(1, 1, 1), explosion_rad*2 + 1, -1.0, 0)
			TerrainModifications.uniform_dumb_overwrite_sphere_set(global_idx, explosion_rad, -1.0, 0)
			queue_free()
