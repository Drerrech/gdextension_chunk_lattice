extends Node3D

var seated_player = null

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


func seat(caller):
	seated_player = caller

func unseat():
	seated_player = null
	
