#pragma once

#include <system/vec.h>
#include <animation/skeleton.h>
#include <animation/animation.h>
#include <graphics/texture.h>
#include <graphics/mesh.h>
#include <graphics/material.h>
#include <graphics/skinned_mesh_instance.h>

#include "anim_system.h"
#include "blend_tree.h"

namespace gef {
	class Platform;
	class Renderer3D;
	class SkinnedMeshInstance;
	class Texture;
}

struct Animation3D {
	Animation3D() = default;
	Animation3D(gef::Animation &&anim) 
		: anim_data(std::move(anim)), duration(anim_data.duration()) {}
	
	bool updateTimer(float delta_time);
	void updatePose(gef::SkeletonPose &pose, const gef::SkeletonPose &bind_pose);
	bool update(float delta_time, gef::SkeletonPose &pose, const gef::SkeletonPose &bind_pose);
	
	gef::Animation anim_data;
	char name[24] = { 0 };
	float duration = 0.f;
	float timer = 0.f;
	float playback_speed = 1.f;
	bool looping = true;
};

class AnimSystem3D : public AnimSystem {
public:
	virtual bool update(float delta_time) override;
	virtual void draw() override;
	virtual void debugDraw() override;
	virtual void setAnimation(const char *name) override;
	virtual void setAnimation(int id) override;
	virtual gef::Transform getTransform() const override;
	virtual void setTransform(const gef::Transform &tran) override;
	virtual void read(FILE *fp) override;
	virtual void save(FILE *fp) const override;
	virtual bool checkFormatVersion(FILE *fp) override;

	void init(gef::Platform &plat, gef::Renderer3D *renderer3D, const char *mesh_fname);
	void cleanup();

	void loadSkeleton(const char *filename);
	int loadAnimation(const char *anim_scene, const char *anim_name = nullptr);
	bool isAnimationIdValid(int id);

	gef::SkinnedMeshInstance *getSkinnedMesh() { return skinned_mesh.get(); }

	void setPose(const gef::SkeletonPose &new_pose);
	gef::SkeletonPose &getPose() { return anim_pose; }

	Animation3D *getAnimation(int id);
	Animation3D *getAnimation(const char *name);
	Animation3D *getCurrentAnimation();

	int getAnimationId(const Animation3D *anim) const;

	BlendTree &getBlendTree() { return blend_tree; }

private:
	gef::Platform *platform = nullptr;
	gef::Renderer3D *renderer = nullptr;
	gef::Skeleton skeleton;
	gef::ptr<gef::Mesh> mesh = nullptr;
	gef::ptr<gef::SkinnedMeshInstance> skinned_mesh = nullptr;
	gef::Vec<gef::ptr<gef::Texture>> textures;
	gef::Vec<gef::Material> materials;
	gef::SkeletonPose anim_pose;
	gef::Vec<Animation3D> animations;
	BlendTree blend_tree;
	float speed_multiplier = 1.f;
	bool spinning = false;
	bool is_using_blend_tree = true;

	std::string scene_filename;
	gef::Vec<std::string> animation_scenes;
};
