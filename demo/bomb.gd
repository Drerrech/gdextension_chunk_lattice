extends RigidBody3D

@onready var Main = get_tree().root.get_node("main")

@onready var loader = $ChunkLoader

var explosion_rad: int = 5

func _ready() -> void:
	if multiplayer.is_server():
		loader.setup(Main.CL, -1, 2, 2)

func _physics_process(delta: float) -> void:
	if multiplayer.is_server():
		loader.check_and_load()

func _on_timer_timeout() -> void:
	if (not multiplayer.is_server()): return
	
	Main.m_spawner.rpc_spawn.rpc_id(1, {"type": "explosion", "glob_pos": global_position, "rad": explosion_rad})
	
	var global_idx = TerrainModifications.get_global_idx(global_position)
	#TerrainModifications.uniform_dumb_overwrite_cube_set(global_idx - explosion_rad*Vector3i(1, 1, 1), explosion_rad*2 + 1, -1.0, 0)
	TerrainModifications.uniform_dumb_overwrite_sphere_set(global_idx, explosion_rad, -1.0, 0)
	queue_free()
