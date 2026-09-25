class_name Player

extends CharacterBody3D

@onready var Main = get_tree().root.get_node("main")

var id: int
var seated = false
var seat = null

const SPEED = 8.0
const JUMP_VELOCITY = 5
const MOUSE_SENSITIVITY = 0.1

@onready var m_spawner = Main.m_spawner

# exposed variables
var pressed_buttons = {
	"1": false,
	"2": false,
	"3": false,
	"4": false,
	"5": false,
	"6": false,
	"7": false,
	"8": false,
	"9": false,
	"0": false,
	"w": false,
	"a": false,
	"s": false,
	"d": false,
	"e": false,
	"q": false,
	"m1": false,
	"m2": false
}

@onready var loader = $ChunkLoader

@onready var torso = $Torso
@onready var head = $Torso/Head
@onready var camera = $Torso/Head/Camera3D
@onready var text_mesh = $TextMesh

@onready var drill_ray = $Torso/Head/RayCast3D
@onready var drill_indicator = $Torso/Head/indicator

func _ready() -> void:
	name = str(id)
	text_mesh.mesh.text = str(id)
	
	rpc_server_player_custom_loader_setup(3, 2)
	#if multiplayer.is_server(): # IMPORTANT: guard loaders with this
		#loader.setup(Main.CL, id, 3, 2)
	
	# owner only
	if is_multiplayer_authority():
		camera.make_current()
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
		
		# as client delete wall
		#if !multiplayer.is_server():
#			get_parent().get_parent().find_child("StaticBody3D").find_child("wall").queue_free()


func _physics_process(delta: float) -> void:
	# owner only - synced
	if is_multiplayer_authority():
		# Add the gravity.
		if not is_on_floor():
			velocity += get_gravity() * delta

		# Handle jump.
		if Input.is_action_pressed("space"):
			if is_on_floor():
				velocity.y = JUMP_VELOCITY
			if Input.is_action_pressed("shift"):
				velocity.y += delta * (10 + JUMP_VELOCITY)
			

		# Get the input direction and handle the movement/deceleration.
		# As good practice, you should replace UI actions with custom gameplay actions.
		var input_dir := Input.get_vector("a", "d", "w", "s")
		var direction = (torso.transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
		if direction:
			velocity.x = direction.x * SPEED
			velocity.z = direction.z * SPEED
		else:
			velocity.x = move_toward(velocity.x, 0, SPEED)
			velocity.z = move_toward(velocity.z, 0, SPEED)

		move_and_slide()


func _process(delta: float) -> void:
	if !seated:
		var colliding = drill_ray.is_colliding()
		drill_indicator.visible = colliding 
	
		if colliding:
			drill_indicator.global_position = drill_ray.get_collision_point()
			var col = drill_ray.get_collider()
			
			if pressed_buttons["e"] and col.is_in_group("seat"):
				col.seat(self)
				seated = true
				seat = col
	else: # seated
		drill_indicator.visible = false
		
		global_position = seat.global_position
		
		if pressed_buttons["q"]:
			seat.unseat()
			seated = false
			seat = null
	
	if multiplayer.is_server():
		#var time = Time.get_ticks_msec()
		loader.check_and_load()
		#if (Time.get_ticks_msec() - time > 10): print("passed: ", Time.get_ticks_msec() - time)
		server_side_update(delta)

var _drill_interval = 0.1
var _drill_delta = 0
var drill_diam = 2
var _drill_fullness_delta = 0.2
func server_side_update(delta: float) -> void:
	var colliding = drill_ray.is_colliding()
	if colliding and !seated:
		# soil gun
		if _drill_delta <= 0:
			_drill_delta = _drill_interval
			var pos = drill_ray.get_collision_point()
			var global_idx = TerrainModifications.get_global_idx(pos + Main.c_cube_size * (1 - drill_diam%2)*Vector3(0.5, 0.5, 0.5))
			if pressed_buttons["m1"]:
				@warning_ignore("integer_division")
				TerrainModifications.uniform_cube_add(global_idx - drill_diam/2 * Vector3i(1, 1, 1), drill_diam, -_drill_fullness_delta, 0)
			if pressed_buttons["m2"]:
				@warning_ignore("integer_division")
				TerrainModifications.uniform_cube_add(global_idx - drill_diam/2 * Vector3i(1, 1, 1), drill_diam, _drill_fullness_delta, 1)
		if _drill_delta > 0: _drill_delta -= delta

func _input(event):
	if not is_multiplayer_authority(): return
	
	# buttons
	for k in pressed_buttons.keys():
		var pressed = Input.is_action_pressed(k)
		if pressed != pressed_buttons[k]:
			rpc_server_player_set_button.rpc_id(1, k, pressed)
			pressed_buttons[k] = pressed
	
	
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		torso.rotate_y(deg_to_rad(event.relative.x * MOUSE_SENSITIVITY * -1))
		head.rotate_x(deg_to_rad(event.relative.y * MOUSE_SENSITIVITY * -1))
		head.rotation_degrees.x = clamp(head.rotation_degrees.x, -89, 89)
	
	if !seated:
		if Input.is_action_just_pressed("1"):
			m_spawner.rpc_spawn.rpc_id(1, {"type": "ball", "glob_pos": head.global_position + -3.0 * head.global_basis.z})
		if Input.is_action_just_pressed("2"):
			m_spawner.rpc_spawn.rpc_id(1, {"type": "bomb", "glob_pos": head.global_position + -3.0 * head.global_basis.z, "linear_velocity": -20.0 * head.global_basis.z})
		if Input.is_action_just_pressed("3"):
			m_spawner.rpc_spawn.rpc_id(1, {"type": "light", "glob_pos": head.global_position + -3.0 * head.global_basis.z})
		if Input.is_action_just_pressed("4"):
			m_spawner.rpc_spawn.rpc_id(1, {"type": "cannon", "glob_pos": head.global_position + -6.0 * head.global_basis.z})

@rpc("any_peer", "call_remote", "reliable")
func rpc_server_player_set_button(key: String, pressed: bool):
	if !multiplayer.is_server(): return
	pressed_buttons[key] = pressed

@rpc("any_peer", "call_remote", "reliable")
func rpc_server_player_custom_loader_setup(mesh_rad: int, collision_rad: int):
	if !multiplayer.is_server(): return
	loader.setup(Main.CL, id, mesh_rad, collision_rad)
