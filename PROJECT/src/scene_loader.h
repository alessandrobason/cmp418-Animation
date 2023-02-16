#pragma once

#include <memory>
#include <system/vec.h>
#include <unordered_map>
#include <istream>

#include <system/string_id.h>
#include <graphics/mesh_data.h>
#include <graphics/mesh.h>
#include <graphics/material.h>
#include <animation/animation.h>
#include <animation/skeleton.h>

namespace gef {
	class Platform;
	class Texture;
}

struct SceneData {
	gef::Vec<gef::ptr<gef::Texture>> textures;
	gef::Vec<gef::Material> materials;
};

class SceneLoader {
public:
	~SceneLoader();
	
	bool loadScene(const gef::Platform &platform, const char *filename);
	bool loadScene(std::istream &stream);

	void createMaterials(const gef::Platform &platform);
	void createMeshes(gef::Platform &platform);

	gef::Mesh *popFirstMesh();
	gef::Skeleton popFirstSkeleton();

	gef::Vec<gef::ptr<gef::Texture>> &&moveTextures();
	gef::Vec<gef::Material> &&moveMaterials();

	gef::StringIdTable &getStringTable() { return string_id_table; }
	const gef::StringIdTable &getStringTable() const { return string_id_table; }

private:
	SceneData data;

	gef::Vec<gef::MeshData> mesh_data;
	gef::Vec<gef::MaterialData> material_data;

	gef::Vec<gef::Mesh *> meshes;
	gef::Vec<gef::Skeleton> skeletons;

	std::unordered_map<gef::StringId, gef::Animation> animations;
	gef::StringIdTable string_id_table;

	std::unordered_map<gef::StringId, gef::Material *> materials_map;
	std::unordered_map<gef::StringId, gef::Texture *> textures_map;

	gef::Vec<gef::StringId> skin_cluster_name_ids;
};