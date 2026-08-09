extends CharacterBody3D

@onready var Main = get_tree().root.get_node("main")

var id: int

const SPEED = 20.0
const JUMP_VELOCITY = 4.5
const MOUSE_SENSITIVITY = 0.1

@onready var m_spawner = Main.m_spawner

@onready var loader = $loader

@onready var torso = $Torso
@onready var head = $Torso/Head
@onready var camera = $Torso/Head/Camera3D
@onready var text_mesh = $TextMesh

func _ready() -> void:
	name = str(id)
	text_mesh.mesh.text = str(id)
	loader.player_client_id = id
	loader.setup()
	
	# owner only
	if is_multiplayer_authority():
		camera.make_current()
		Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
		
		# as client delete wall
		#if !multiplayer.is_server():
#			get_parent().get_parent().find_child("StaticBody3D").find_child("wall").queue_free()


func _physics_process(delta: float) -> void:
	loader.check_and_load()
	# owner only - synced
	if is_multiplayer_authority():
		# Add the gravity.
		if not is_on_floor():
			velocity += get_gravity() * delta

		# Handle jump.
		if Input.is_action_just_pressed("space"):
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

func _input(event):
	if not is_multiplayer_authority(): return
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		torso.rotate_y(deg_to_rad(event.relative.x * MOUSE_SENSITIVITY * -1))
		head.rotate_x(deg_to_rad(event.relative.y * MOUSE_SENSITIVITY * -1))
		head.rotation_degrees.x = clamp(head.rotation_degrees.x, -89, 89)
	
	if Input.is_action_just_pressed("1"):
		m_spawner.rpc_spawn.rpc_id(1, {"type": "ball", "glob_pos": head.global_position + -4.0 * head.global_basis.z})
	if Input.is_action_just_pressed("2"):
		m_spawner.rpc_spawn.rpc_id(1, {"type": "bomb", "glob_pos": head.global_position + -4.0 * head.global_basis.z, "linear_velocity": -10.0 * head.global_basis.z})
