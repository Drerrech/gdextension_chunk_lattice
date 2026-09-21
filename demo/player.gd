class_name Player

extends CharacterBody3D

@onready var Main = get_tree().root.get_node("main")

var id: int

const SPEED = 16.0
const JUMP_VELOCITY = 5
const MOUSE_SENSITIVITY = 0.1

@onready var m_spawner = Main.m_spawner

# exposed variables
var pressed_buttons = {
	"1": false,
	"2": false,
	"3": false,
	"w": false,
	"a": false,
	"s": false,
	"d": false,
	"m1": false,
	"m2": false
}

@onready var loader = $ChunkLoader

@onready var torso = $Torso
@onready var head = $Torso/Head
@onready var camera = $Torso/Head/Camera3D
@onready var text_mesh = $TextMesh

func _ready() -> void:
	name = str(id)
	text_mesh.mesh.text = str(id)
	
	if multiplayer.is_server(): # IMPORTANT: guard loaders with this
		loader.setup(Main.CL, id, 7, 2)
	
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
			velocity.y = JUMP_VELOCITY

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
	if multiplayer.is_server():
		#var time = Time.get_ticks_msec()
		loader.check_and_load()
		#if (Time.get_ticks_msec() - time > 10): print("passed: ", Time.get_ticks_msec() - time)
		server_side_update(delta)

var _drill_interval = 0.2
var _drill_delta = 0
func server_side_update(delta: float) -> void:
	# soil gun
	var drill_rad = 1
	if pressed_buttons["m1"] or pressed_buttons["m2"]:
		if _drill_delta <= 0:
			_drill_delta = _drill_interval
			if pressed_buttons["m1"]:
				var pos = head.global_position + -3.0 * head.global_basis.z
				var global_idx = TerrainModifications.get_global_idx(pos)
				TerrainModifications.uniform_dumb_overwrite_cube_set(global_idx - 0*drill_rad*Vector3i(1, 1, 1), drill_rad*2 + 1, -1.0, 0)
			if pressed_buttons["m2"]:
				var pos = head.global_position + -3.0 * head.global_basis.z
				var global_idx = TerrainModifications.get_global_idx(pos)
				TerrainModifications.uniform_dumb_overwrite_cube_set(global_idx - 0*drill_rad*Vector3i(1, 1, 1), drill_rad*2 + 1, 1.0, 1)
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
	
	if Input.is_action_just_pressed("1"):
		m_spawner.rpc_spawn.rpc_id(1, {"type": "ball", "glob_pos": head.global_position + -3.0 * head.global_basis.z})
	if Input.is_action_just_pressed("2"):
		m_spawner.rpc_spawn.rpc_id(1, {"type": "bomb", "glob_pos": head.global_position + -3.0 * head.global_basis.z, "linear_velocity": -20.0 * head.global_basis.z})
	if Input.is_action_just_pressed("3"):
		m_spawner.rpc_spawn.rpc_id(1, {"type": "light", "glob_pos": head.global_position + -3.0 * head.global_basis.z})

@rpc("any_peer", "call_remote", "reliable")
func rpc_server_player_set_button(key: String, pressed: bool):
	if !multiplayer.is_server(): return
	pressed_buttons[key] = pressed
