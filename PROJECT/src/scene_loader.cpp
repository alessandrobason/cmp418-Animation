#include "scene_loader.h"

#include <system/file.h>
#include <system/memory_stream_buffer.h>
#include <assets/png_loader.h>
#include <graphics/texture.h>
#include <graphics/image_data.h>
#include <animation/skeleton.h>
#include <animation/animation.h>

#include <system/allocator.h>

SceneLoader::~SceneLoader() {
	for (auto &mesh : meshes)
		g_alloc->destroy(mesh);
}

bool SceneLoader::loadScene(const gef::Platform &platform, const char *filename) {
	PushAllocInfo("SceneLoader");
	bool success = true;
	void *file_data = NULL;
	gef::ptr<gef::File> file = gef::File::Create();
	Int32 file_size;

	success = file->Open(filename);
	if (success) {
		success = file->GetSize(file_size);
		if (success) {
			file_data = g_alloc->allocDebug(file_size, "scene loader file data");
			success = file_data != NULL;
			if (success) {
				Int32 bytes_read;
				success = file->Read(file_data, file_size, bytes_read);
				if (success)
					success = bytes_read == file_size;
			}

			if (success) {
				gef::MemoryStreamBuffer stream_buffer((char *)file_data, file_size);

				std::istream input_stream(&stream_buffer);
				success = loadScene(input_stream);

				// don't need the font file data any more
				g_alloc->dealloc(file_data);
				file_data = NULL;
			}

		}

		file->Close();
	}

	PopAllocInfo();
	return success;
}

bool SceneLoader::loadScene(std::istream &stream) {
	bool success = true;

	Int32 mesh_count;
	Int32 material_count;
	Int32 skeleton_count;
	Int32 animation_count;
	Int32 string_count;

	stream.read((char *)&mesh_count, sizeof(Int32));
	stream.read((char *)&material_count, sizeof(Int32));
	stream.read((char *)&skeleton_count, sizeof(Int32));
	stream.read((char *)&animation_count, sizeof(Int32));
	stream.read((char *)&string_count, sizeof(Int32));

	// string table
	for (Int32 string_num = 0; string_num < string_count; ++string_num) {
		std::string the_string = "";

		char string_character;
		do {
			stream.read(&string_character, 1);
			if (string_character != 0)
				the_string.push_back(string_character);
		} while (string_character != 0);

		string_id_table.Add(the_string);
	}

	// materials
	for (Int32 material_num = 0; material_num < material_count; ++material_num) {
		material_data.emplace_back(gef::MaterialData());

		gef::MaterialData &material = material_data.back();

		material.Read(stream);

		//material_data_map[material.name_id] = &material;
	}

	// mesh_data
	for (Int32 mesh_num = 0; mesh_num < mesh_count; ++mesh_num) {
		mesh_data.emplace_back(gef::MeshData());

		gef::MeshData &mesh = mesh_data.back();

		mesh.Read(stream);
	}

	// skeletons
	for (Int32 skeleton_num = 0; skeleton_num < skeleton_count; ++skeleton_num) {
		gef::Skeleton skeleton;
		skeleton.Read(stream);
		skeletons.emplace_back(std::move(skeleton));
	}

	// animations
	for (Int32 animation_num = 0; animation_num < animation_count; ++animation_num) {
		gef::Animation animation;
		animation.Read(stream);
		animations[animation.name_id()] = std::move(animation);
	}

	return success;
}

void SceneLoader::createMaterials(const gef::Platform &platform) {
	// go through all the materials and create new textures for them
	data.materials.reserve(material_data.size());
	data.textures.reserve(material_data.size());
	for (auto &mat : material_data) {
		gef::Material material;

		// colour
		material.set_colour(mat.colour);

		// texture
		if (mat.diffuse_texture != "") {
			gef::StringId texture_name_id = gef::GetStringId(mat.diffuse_texture);
			auto find_result = textures_map.find(texture_name_id);
			if (find_result == textures_map.end()) {
				string_id_table.Add(mat.diffuse_texture);

				gef::ImageData image_data;
				gef::PNGLoader png_loader;
				png_loader.Load(mat.diffuse_texture.c_str(), platform, image_data);
				if (image_data.image() != NULL) {
					data.textures.emplace_back(gef::Texture::Create(platform, image_data));
					gef::Texture *texture = data.textures.back().get();

					textures_map[texture_name_id] = texture;
					material.set_texture(texture);
				}
			}
			else {
				material.set_texture(find_result->second);
			}
		}

		data.materials.emplace_back(material);
		materials_map[mat.name_id] = &data.materials.back();
	}
}

void SceneLoader::createMeshes(gef::Platform &platform) {
	for (auto &data : mesh_data) {
		gef::Mesh *mesh = gef::Mesh::Create(platform);
		mesh->set_aabb(data.aabb);
		mesh->set_bounding_sphere(gef::Sphere(mesh->aabb()));

		mesh->InitVertexBuffer(platform, data.vertex_data.vertices, data.vertex_data.num_vertices, data.vertex_data.vertex_byte_size, true);
		mesh->AllocatePrimitives((Int32)data.primitives.size());

		Int32 prim_index = 0;
		for (const auto &prim : data.primitives) {
			gef::Primitive *primitive = mesh->GetPrimitive(prim_index++);
			primitive->set_type(prim->type);
			primitive->InitIndexBuffer(platform, prim->indices, prim->num_indices, prim->index_byte_size, true);

			if (prim->material_name_id != 0) {
				primitive->set_material(materials_map[prim->material_name_id]);
			}
		}

		meshes.emplace_back(mesh);
	}
}

gef::Mesh *SceneLoader::popFirstMesh() {
	if (mesh_data.size() > 0) {
		gef::Mesh *mesh = meshes[0];
		meshes.erase(0);
		return mesh;
	}
	return nullptr;
}

gef::Skeleton SceneLoader::popFirstSkeleton() {
	if (skeletons.size() > 0) {
		gef::Skeleton skeleton = std::move(skeletons[0]);
		skeletons.erase(0);
		return skeleton;
	}
	return {};
}

gef::Vec<gef::ptr<gef::Texture>> &&SceneLoader::moveTextures() {
	return std::move(data.textures);
}

gef::Vec<gef::Material> &&SceneLoader::moveMaterials() {
	return std::move(data.materials);
}
