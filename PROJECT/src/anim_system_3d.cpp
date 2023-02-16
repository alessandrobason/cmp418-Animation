#include "anim_system_3d.h"

#include <system/platform.h>
#include <system/allocator.h>
#include <graphics/scene.h>
#include <graphics/renderer_3d.h>
#include <graphics/skinned_mesh_instance.h>
#include <external/ImGui/imgui.h>

#include "scene_loader.h"
#include "utils.h"

/*
save format:

name             | type
-----------------------------------
		       header
-----------------------------------
format_ver       | uint8_t
transform        | float * 16
scene_file_len   | uint8_t
scene_filename   | char * scene_file_len
animations_count | uint8_t
-----------------------------------
			 animation			   
-----------------------------------
filename_len     | uint8_t
filename         | char * filename_len
name_len         | uint8_t
name             | char * name_len
playback_spd     | float
looping          | bool
-----------------------------------
			 blend tree
-----------------------------------
nodes_count      | uint8_t
map_count        | uint8_t
exit_node        | uint8_t
---------- for each node ----------
type             | uint8_t
  ~~~~~~~~~~ clip node ~~~~~~~~~~
clip_id          | uint8_t
  ~~~~~~~~~~ sync node ~~~~~~~~~~
clip_id          | uint8_t
lead_id          | uint8_t
  ~~~~~~~~~~ blend node ~~~~~~~~~
blend            | float
inp_0            | uint8_t
inp_1            | uint8_t
  ~~~~~~~~ 1D blend node ~~~~~~~~
blend            | float
right            | uint8_t
centre           | uint8_t
left             | uint8_t
---------- for each value ---------
node_id          | uint8_t
namelen          | uint8_t
name             | char * namelen
*/

// increase this every time a change to the format is made
// it'll make sure that it won't try to load the wrong 
// version of the file
static constexpr uint8_t format_ver = 2;

bool Animation3D::updateTimer(float delta_time) {
	timer += delta_time * playback_speed;
	if (timer >= duration) {
		timer = 0;
		if (!looping) {
			return true;
		}
	}

	return false;
}

void Animation3D::updatePose(gef::SkeletonPose &pose, const gef::SkeletonPose &bind_pose) {
	// add the clip start time to the playback time to calculate the final time
	// that will be used to sample the animation data
	float anim_time = timer + anim_data.start_time();

	// sample the animation data at the calculated time
	// any bones that don't have animation data are set to the bind pose
	pose.SetPoseFromAnim(anim_data, bind_pose, anim_time);
}

bool Animation3D::update(float delta_time, gef::SkeletonPose &pose, const gef::SkeletonPose &bind_pose) {
	bool finished = updateTimer(delta_time);
	updatePose(pose, bind_pose);
	return finished;
}

bool AnimSystem3D::update(float delta_time) {
	if (!skinned_mesh) {
		return true;
	}

	if (spinning) {
		gef::Matrix44 rot = gef::Matrix44::kIdentity;
		rot.RotationY(delta_time * 2.f);
		skinned_mesh->set_transform(skinned_mesh->transform() * rot);
	}

	delta_time *= speed_multiplier;

	if (is_using_blend_tree) {
		blend_tree.update(delta_time);
	}
	else {
		if (isAnimationIdValid(cur_animation)) {
			animations[cur_animation].update(delta_time, anim_pose, skinned_mesh->bind_pose());
			skinned_mesh->UpdateBoneMatrices(anim_pose);
		}
		else {
			skinned_mesh->UpdateBoneMatrices(skinned_mesh->bind_pose());
		}
	}

	return false;
}

void AnimSystem3D::draw() {
	if (!renderer || !skinned_mesh) {
		return;
	}

	renderer->DrawSkinnedMesh(*skinned_mesh, skinned_mesh->bone_matrices());
}

void AnimSystem3D::debugDraw() {
	ImGui::SliderFloat("speed", &speed_multiplier, 0.f, 1.f);
	ImGui::Checkbox("use blend tree", &is_using_blend_tree);
	ImGui::Checkbox("spinning", &spinning);
	if (ImGui::Button("Reset Spin")) {
		const gef::Matrix44 &tran = skinned_mesh->transform();
		skinned_mesh->set_transform(mat4FromPosScale(tran.GetTranslation(), tran.GetScale()));
	}
	ImGui::Separator();

	for (Animation3D &anim : animations) {
		ImGui::PushID(&anim);

		ImGui::Text("Name: %s", anim.name);
		ImGui::Checkbox("Looping", &anim.looping);
		imHelper(
			"This setting does nothing for viewing purposes, "
			"but all the logic is still implemented"
		);
		ImGui::DragFloat("Playback Speed", &anim.playback_speed, 0.1f, 0.f, 5.f);
		ImGui::Text("Timer: %.3f/%.3f", anim.timer, anim.duration);
		ImGui::Separator();

		ImGui::PopID();
	}
}

void AnimSystem3D::cleanup() {
	blend_tree.cleanup();
	mesh.destroy();
	skinned_mesh.destroy();
	textures.clear();
	materials.clear();
	animations.clear();
}

void AnimSystem3D::setAnimation(const char *name) {
	for (size_t i = 0; i < animations.size(); ++i) {
		if (strcmp(animations[i].name, name) == 0) {
			setAnimation((int)i);
			return;
		}
	}
	setAnimation(INVALID_ID);
}

void AnimSystem3D::setAnimation(int id) {
	if (is_using_blend_tree) return;

	if (!isAnimationIdValid(id)) {
		id = INVALID_ID;
	}
	cur_animation = id;
}

gef::Transform AnimSystem3D::getTransform() const {
	if (!skinned_mesh) {
		return gef::Transform::kIdentity;
	}
	return skinned_mesh->transform();
}

void AnimSystem3D::setTransform(const gef::Transform &tran) {
	if (!skinned_mesh) {
		return;
	}
	skinned_mesh->set_transform(tran.GetMatrix());
}

void AnimSystem3D::read(FILE *fp) {
	if (!fp) return;
	cleanup();

	uint8_t scenefile_len = 0;
	uint8_t animations_count = 0;
	gef::Matrix44 transform;

	fileRead(transform, fp);
	fileRead(scenefile_len, fp);
	scene_filename.resize(scenefile_len);
	fileRead(scene_filename, fp);
	fileRead(animations_count, fp);

	loadSkeleton(scene_filename.c_str());
	skinned_mesh->set_transform(transform);

	for (uint8_t i = 0; i < animations_count; ++i) {
		uint8_t filename_len = 0;
		uint8_t name_len = 0;
		std::string filename;

		fileRead(filename_len, fp);
		filename.resize(filename_len);
		fileRead(filename, fp);
		fileRead(name_len, fp);
		char name[sizeof(Animation3D::name)];
		memset(name, 0, sizeof(name));
		fread(name, 1, name_len, fp);

		loadAnimation(filename.c_str(), name);
		Animation3D &anim = animations.back();
		fileRead(anim.playback_speed, fp);
		fileRead(anim.looping, fp);
	}

	blend_tree.init(this);
	blend_tree.read(fp);
}

void AnimSystem3D::save(FILE *fp) const {
	if (!fp) return;

	uint8_t animations_count = (uint8_t)animations.size();

	fileWrite(format_ver,       fp);
	fileWrite(skinned_mesh->transform(), fp);
	fileWrite((uint8_t)scene_filename.size(), fp);
	fileWrite(scene_filename, fp);
	fileWrite(animations_count, fp);

	for (uint8_t i = 0; i < animations_count; ++i) {
		const Animation3D &anim = animations[i];
		const std::string &fname = animation_scenes[i];
		uint8_t name_len = (uint8_t)strlen(anim.name);

		fileWrite((uint8_t)fname.size(), fp);
		fileWrite(fname,                 fp);
		fileWrite(name_len,              fp);
		fwrite(anim.name, 1, name_len,   fp);
		fileWrite(anim.playback_speed,   fp);
		fileWrite(anim.looping,          fp);
	}

	blend_tree.save(fp);
}

bool AnimSystem3D::checkFormatVersion(FILE *fp) {
	if (!fp) return false;
	uint8_t version = 0;
	fread(&version, 1, sizeof(version), fp);
	return version == format_ver;
}

void AnimSystem3D::init(gef::Platform &plat, gef::Renderer3D *renderer3D, const char *mesh_fname) {
	type = AnimSystemType::Skeleton3D;
	platform = &plat;
	renderer = renderer3D;

	PushAllocInfo("Anim3DInit");
	loadSkeleton(mesh_fname);
	blend_tree.init(this);
	PopAllocInfo();
}

void AnimSystem3D::loadSkeleton(const char *filename) {
	assert(platform);
	gef::Platform &plat = *platform;
	scene_filename = filename;

	SceneLoader model_scene;
	if (model_scene.loadScene(plat, filename)) {
		model_scene.createMaterials(plat);
		model_scene.createMeshes(plat);

		skeleton = model_scene.popFirstSkeleton();
		mesh = model_scene.popFirstMesh();

		skinned_mesh = gef::ptr<gef::SkinnedMeshInstance>::make(skeleton);
		anim_pose = skinned_mesh->bind_pose();
		skinned_mesh->set_mesh(mesh.get());

		textures = model_scene.moveTextures();
		materials = model_scene.moveMaterials();
	}
}

int AnimSystem3D::loadAnimation(const char *anim_scene, const char *anim_name) {
	PushAllocInfo("Anim3DLoad");
	gef::Scene scene;
	if (scene.ReadSceneFromFile(*platform, anim_scene)) {
		animation_scenes.emplace_back(anim_scene);

		auto it = scene.animations.begin();

		if (it != scene.animations.end()) {
			int id = (int)animations.size();
			Animation3D new_anim(std::move(*it->second));
			strCopyInto(new_anim.name, anim_name ? anim_name : anim_scene);
			animations.emplace_back(new_anim);
			if (cur_animation == INVALID_ID) {
				cur_animation = id;
			}
			
			PopAllocInfo();
			return id;
		}
		else {
			fatal("couldn't load animation, no animation in scene");
		}
	}
	else {
		fatal("couldn't load animation, couldn't load scene file");
	}

	PopAllocInfo();
	return INVALID_ID;
}

bool AnimSystem3D::isAnimationIdValid(int id) {
	return id >= 0 && id < animations.size();
}

void AnimSystem3D::setPose(const gef::SkeletonPose &new_pose) {
	anim_pose = new_pose;
}

Animation3D *AnimSystem3D::getAnimation(int id) {
	return isAnimationIdValid(id) ? &animations[id] : nullptr;
}

Animation3D *AnimSystem3D::getAnimation(const char *name) {
	for (Animation3D &anim : animations) {
		if (strcmp(anim.name, name) == 0) {
			return &anim;
		}
	}
	return nullptr;
}

Animation3D *AnimSystem3D::getCurrentAnimation() {
	return getAnimation(cur_animation);
}

int AnimSystem3D::getAnimationId(const Animation3D *anim) const {
	size_t index = animations.findIt(anim);
	return index == SIZE_MAX ? INVALID_ID : (int)index;
}

