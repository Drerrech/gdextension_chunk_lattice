extends Node3D

@onready var smoke_particles = $GPUParticles3D

var explosion_rad: int = 5
var rad: float = 8

func _ready() -> void:
	smoke_particles.amount = min(128, int(4 * rad**2))
	smoke_particles.process_material.initial_velocity_max = min(8, rad+4)
	smoke_particles.emitting = true

func _on_timer_timeout() -> void:
	if (not multiplayer.is_server()): return
	queue_free()
