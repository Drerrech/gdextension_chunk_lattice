extends MultiplayerSpawner

@onready var player_scene = preload("res://player.tscn")
@onready var ball_scene = preload("res://ball.tscn")
@onready var bomb_scene = preload("res://bomb.tscn")

func _ready() -> void:
	spawn_function = _spawn_function

func _spawn_function(data: Dictionary) -> Node:
	var entity: Node
	
	if data["type"] == "player":
		var id = data["id"]
		var glob_pos = data["glob_pos"]
		
		entity = player_scene.instantiate()
		entity.id = id
		entity.position = glob_pos # m_spawner is at origin
		entity.set_multiplayer_authority(id)
	if data["type"] == "ball":
		var glob_pos = data["glob_pos"]
		
		entity = ball_scene.instantiate()
		entity.position = glob_pos
		
		if data.has("linear_velocity"):
			entity.linear_velocity = data["linear_velocity"]
	
	if data["type"] == "bomb":
		var glob_pos = data["glob_pos"]
		
		entity = bomb_scene.instantiate()
		entity.position = glob_pos
		
		if data.has("linear_velocity"):
			entity.linear_velocity = data["linear_velocity"]
	
	return entity

@rpc("any_peer", "call_local", "reliable")
func rpc_spawn(data: Dictionary):
	if not multiplayer.is_server(): return
	spawn(data)
	print("server spawned: ", data)
