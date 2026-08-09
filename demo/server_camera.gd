extends Node3D

const SPEED = 40.0
const MOUSE_SENSITIVITY = 0.1

func _physics_process(delta: float) -> void:
	# owner only - synced
	if is_multiplayer_authority():
		# Get the input direction and handle the movement/deceleration.
		# As good practice, you should replace UI actions with custom gameplay actions.
		var input_dir := Input.get_vector("a", "d", "w", "s")
		var direction = (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
		if direction:
			position += delta * direction * SPEED * abs((90 - $Camera3D.rotation_degrees.x) / 90.0)
			position.y += delta * -input_dir.y * ($Camera3D.rotation_degrees.x / 90.0) * SPEED
			#print(position.y)

func _input(event):
	if not is_multiplayer_authority(): return
	if event is InputEventMouseMotion and Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
		rotate_y(deg_to_rad(event.relative.x * MOUSE_SENSITIVITY * -1))
		$Camera3D.rotate_x(deg_to_rad(event.relative.y * MOUSE_SENSITIVITY * -1))
		$Camera3D.rotation_degrees.x = clamp($Camera3D.rotation_degrees.x, -89, 89)
