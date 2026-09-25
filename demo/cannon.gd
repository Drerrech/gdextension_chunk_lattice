extends Node3D

var rotation_speed = 0.25

@onready var seat = $torso/seat

@onready var torso = $torso
@onready var head = $torso/head
@onready var projectile_spawn = $torso/head/projectile_spawn

@onready var Main = get_tree().root.get_node("main")

var shoot_delta_time = 5
var shoot_delta = 0

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	if !multiplayer.is_server(): return
	
	if seat.seated_player != null:
		# rotation
		var player: Player = seat.seated_player
		if player.pressed_buttons["w"]:
			print(player, "pressing w")
			head.rotation.z = clamp(head.rotation.z + rotation_speed * delta, -0.5, 0.7)
		if player.pressed_buttons["s"]:
			head.rotation.z = clamp(head.rotation.z - rotation_speed * delta, -0.5, 0.7)
		
		if player.pressed_buttons["a"]:
			torso.rotation.y += rotation_speed * delta
		if player.pressed_buttons["d"]:
			torso.rotation.y -= rotation_speed * delta
		
		# shoot
		if player.pressed_buttons["m1"] and shoot_delta <= 0:
			shoot_delta = shoot_delta_time
			Main.m_spawner.rpc_spawn.rpc_id(1, {"type": "explosion", "glob_pos": projectile_spawn.global_position, "rad": 1})
			Main.m_spawner.rpc_spawn.rpc_id(1, {"type": "cannon_projectile", "glob_pos": projectile_spawn.global_position, "travel_dir": -40.0 * projectile_spawn.global_basis.z})
	if shoot_delta > 0:
		shoot_delta -= delta
		
