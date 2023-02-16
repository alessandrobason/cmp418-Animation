#pragma once

#include <unordered_map>
#include <string>

#include <system/vec.h>
#include <system/ptr.h>
#include <animation/skeleton.h>
#include <graphics/material.h>
#include <graphics/texture.h>

#include "anim_system.h"

namespace gef {
	class Platform;
	class InputManager;
	class Renderer3D;
	class SkinnedMeshInstance;
}

struct Batch2D;

class AnimSystemIK : public AnimSystem {
public:
	virtual bool update(float delta_time) override;
	virtual void draw() override;
	virtual void debugDraw() override;
	virtual void save(FILE *fp) const override;
	virtual void read(FILE *fp) override;
	virtual bool checkFormatVersion(FILE *fp) override;
	virtual void setAnimation(const char *name) override;
	virtual void setAnimation(int id) override;
	virtual gef::Transform getTransform() const override;
	virtual void setTransform(const gef::Transform &tran) override;

	void init(gef::Platform &plat, gef::Renderer3D *renderer, gef::InputManager *input_manager, const char *filename);
	void cleanup();

	void drawPointer(Batch2D &batch);

private:
	void loadSkeleton(const char *filename);
	bool calculateCCD();

	gef::Platform *platform = nullptr;
	gef::Renderer3D *renderer = nullptr;
	gef::InputManager *input = nullptr;
	gef::Skeleton skeleton;
	gef::ptr<gef::Mesh> mesh = nullptr;
	gef::ptr<gef::SkinnedMeshInstance> skinned_mesh = nullptr;
	gef::Vec<gef::ptr<gef::Texture>> textures;
	gef::Vec<gef::Material> materials;

	std::unordered_map<std::string, int> bone_map;

	gef::Vector2 mouse_pos;
	gef::Vector4 dest_pos;
	gef::SkeletonPose ik_pose;
	gef::Vec<int> ccd_bones;
};