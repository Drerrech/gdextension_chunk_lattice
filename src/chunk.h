#ifndef CHUNK_H
#define CHUNK_H

#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/classes/file_access.hpp>

namespace godot {
/*
Chunk

*/
class Chunk : public StaticBody3D{
	GDCLASS(Chunk, StaticBody3D)

private:


protected:
	static void _bind_methods();

public:
	String file_world_name;
	String abstract_file_path;

	Vector3i chunk_shape;
	Vector3 chunk_cube_size;
	int lattice_type;
	int lattice_seed;

	struct LoaderAttributes {
        bool collision;
    };
	HashMap<ObjectID, LoaderAttributes> occupants;

	PackedFloat32Array point_fullness_values; // [-1, 1]
	PackedByteArray point_material_values; // {0, 1, ... 255}
	struct PointChange {
        float fullness;
        uint8_t material;
    };
    HashMap<int32_t, PointChange> point_changes;

	// children and their resources
	PackedVector3Array vertex_positions; // needed for collision
	MeshInstance3D *mesh_instance_ptr;
	Ref<ArrayMesh> mesh_resource;

	bool set_collision = false;
	CollisionShape3D *collision_shape_instance_ptr;
	Ref<ConcavePolygonShape3D> collision_shape_resource;

	Chunk();
	~Chunk();

	void setup(String p_file_world_name, Vector3i p_chunk_shape, Vector3 p_chunk_cube_size, int p_lattice_type, int p_lattice_seed);

	void initial_build();

	int get_idx(int i, int j, int k);

	void set_raw_generation_points(); // fills in the arrays according to how the planet should have been generated

	void read_and_apply_point_changes();

	void write_changes();
	
	void apply_point_changes(PackedInt32Array idxs, PackedFloat32Array fullness_values, PackedByteArray material_values); // can be used to apply saved changes or new ones during game

	int get_triangulation_idx(int x, int y, int z);
	
	void set_generated_mesh(); // marches cubes, and assigns ArrayMesh to the child

	void set_generated_collision(); // creates a ConcavePolygonShape3D with the array_vertex and assigns it to the child

	Dictionary get_point_changes();
	
	Array get_occupants();
};

}

#endif