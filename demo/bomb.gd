extends RigidBody3D

@onready var loader = $loader

var explosion_rad: int = 2

func _ready() -> void:
	loader.mesh_load_cube_rad = 2
	loader.collision_load_cube_rad = 2
	loader.setup()

func _physics_process(delta: float) -> void:
	loader.check_and_load()

func _on_timer_timeout() -> void:
	if (not multiplayer.is_server()): return
	var global_idx = TerrainModifications.get_global_idx(global_position)
	#print("EXPLODING AT GLOBAL IDX ", global_idx)
	TerrainModifications.uniform_dumb_overwrite_cube_set(global_idx - explosion_rad*Vector3i(1, 1, 1), explosion_rad*2 + 1, -1.0, 0)
	queue_free()
