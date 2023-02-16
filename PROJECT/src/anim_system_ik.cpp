#include "anim_system_ik.h"

#include <system/platform.h>
#include <input/input_manager.h>
#include <input/touch_input_manager.h>
#include <graphics/renderer_3d.h>
#include <graphics/skinned_mesh_instance.h>

#include "scene_loader.h"
#include "batch2d.h"

static void getScreenPosRay(const gef::Vector2 &screen_position, const gef::Matrix44 &projection, const gef::Matrix44 &view, gef::Vector4 &start_point, gef::Vector4 &direction, const gef::Vector2 &screen_sz);
static bool rayPlaneIntersect(gef::Vector4 &start_point, gef::Vector4 &direction, const gef::Vector4 &point_on_plane, const gef::Vector4 &plane_normal, gef::Vector4 &hitpoint);

void AnimSystemIK::init(gef::Platform &plat, gef::Renderer3D *renderer3d, gef::InputManager *input_manager, const char *filename) {
	type = AnimSystemType::InverseKinematics;
	platform = &plat;
	renderer = renderer3d;
	input = input_manager;

	PushAllocInfo("AnimIKInit");
	loadSkeleton(filename);
	PopAllocInfo();
}

void AnimSystemIK::cleanup() {
}

bool AnimSystemIK::update(float delta_time) {
	bool mb_down = false;

	if (auto mouse = input->touch_manager()) {
		mb_down = mouse->is_button_down(0);
		mouse_pos = mouse->mouse_position();
	}

	if (mb_down) {
		gef::Vector4 mouse_ray_start_point, mouse_ray_direction;
		static float ndc_zmin_;
		getScreenPosRay(
			mouse_pos, 
			renderer->projection_matrix(), 
			renderer->view_matrix(), 
			mouse_ray_start_point, 
			mouse_ray_direction,
			platform->size() 
		);

		if (rayPlaneIntersect(
				mouse_ray_start_point, 
				mouse_ray_direction, 
				gef::Vector4::kZero, 
				gef::Vector4(0.0f, 0.0f, 1.0f), 
				dest_pos
			)
		) {
			calculateCCD();
			skinned_mesh->UpdateBoneMatrices(ik_pose);
		}
	}

	return true;
}

void AnimSystemIK::draw() {
	if (!renderer || !skinned_mesh) {
		return;
	}

	renderer->DrawSkinnedMesh(*skinned_mesh, skinned_mesh->bone_matrices());
}

void AnimSystemIK::debugDraw() {
	ImGui::Text("Hello World!");
}

void AnimSystemIK::save(FILE *fp) const {
	// no-op
	(void)fp;
}

void AnimSystemIK::read(FILE *fp) {
	// no-op
	(void)fp;
}

bool AnimSystemIK::checkFormatVersion(FILE *fp) {
	// no-op
	return false;
}

void AnimSystemIK::setAnimation(const char *name) {
	// no-op
	(void)name;
}

void AnimSystemIK::setAnimation(int id) {
	// no-op
	(void)id;
}

gef::Transform AnimSystemIK::getTransform() const {
	return skinned_mesh ? skinned_mesh->transform() : gef::Transform::kIdentity;
}

void AnimSystemIK::setTransform(const gef::Transform &tran) {
	if (skinned_mesh) {
		skinned_mesh->set_transform(tran.GetMatrix());
	}
}

void AnimSystemIK::drawPointer(Batch2D &batch) {
	constexpr float len = 15.f;
	constexpr float wid = 3.f;

	gef::Vector2 a = mouse_pos - gef::Vector2(0, len);
	gef::Vector2 b = mouse_pos + gef::Vector2(0, len);
	gef::Vector2 c = mouse_pos - gef::Vector2(len, 0);
	gef::Vector2 d = mouse_pos + gef::Vector2(len, 0);

	batch.drawLine(a, b, wid, gef::Colour::green);
	batch.drawLine(c, d, wid, gef::Colour::green);
}

void AnimSystemIK::loadSkeleton(const char *filename) {
	assert(platform);
	
	SceneLoader loader;
	if (loader.loadScene(*platform, filename)) {
		loader.createMaterials(*platform);
		loader.createMeshes(*platform);

		skeleton = loader.popFirstSkeleton();
		mesh = loader.popFirstMesh();

		skinned_mesh = gef::ptr<gef::SkinnedMeshInstance>::make(skeleton);
		ik_pose = skinned_mesh->bind_pose();
		skinned_mesh->set_mesh(mesh.get());
		skinned_mesh->UpdateBoneMatrices(skinned_mesh->bind_pose());

		textures = loader.moveTextures();
		materials = loader.moveMaterials();

		auto &string_table = loader.getStringTable();
		const auto &joints = skeleton.joints();

		for (size_t i = 0; i < joints.size(); ++i) {
			std::string bone_name;
			if (string_table.Find(joints[i].name_id, bone_name)) {
				size_t start = bone_name.find_first_of(':');
				if (start != std::string::npos) {
					bone_name = bone_name.substr(++start);
				}
				info("%zu: %s", i, bone_name.c_str());
				bone_map[bone_name] = (int)i;
			}
			else {
				warn("could not find bone %zu in string table", i);
			}
		}
	}
	// right arm
	// ccd_bones = { 43, 44, 45 };
	
	// right leg
	// ccd_bones = { 9, 10, 11 };
	
	// left arm
	ccd_bones = { 18, 19, 20 };

	// left leg
	// ccd_bones = { 5, 6, 7 };
	
	// back
	// ccd_bones = { 4, 13, 14 };
}

bool AnimSystemIK::calculateCCD() {
	if (ccd_bones.size() < 2) {
		return false;
	}

	gef::Vec<gef::Matrix44> &global_pose = ik_pose.global_pose();
	gef::Vec<gef::JointPose> &local_pose = ik_pose.local_pose();

	//obtain the inverse of the animated model's transform
	gef::Matrix44 world_to_model_tran;
	world_to_model_tran.Inverse(skinned_mesh->transform());

	//...and use it to move the destination point from world space to model space
	gef::Vector4 dest_point_modelspace = dest_pos.Transform(world_to_model_tran);

	// Get the end effectors position
	gef::Vector4 end_effector = global_pose[ccd_bones.back()].GetTranslation();

	//calculate the distance between the end effector's position and the destination position
	float distance = (dest_point_modelspace - end_effector).Length();
	float old_distance = distance;
	float distance_delta = distance;

	float epsilon = 0.01f;
	//max number of iterations is used to stop CCD if there is no solution
	int max_iterations = 200;

	bool once = true;

	//perform the CCD algorithm if all the following conditions are valid
	/*
	- if the distance between the end effector point is greater than epsilon
	- we have not reached the maximum number of iterations
	*/
	while ((distance > epsilon) && (max_iterations > 0) && (distance_delta > epsilon)) {
		for (int i = (int)ccd_bones.size() - 2; i >= 0; --i) {
			int bone = ccd_bones[i];

			gef::Matrix44 &bone_tran = global_pose[bone];
			const gef::Vector4 &bone_pos = bone_tran.GetTranslation();

			gef::Vector4 EB = (end_effector - bone_pos).Normalised();
			gef::Vector4 DB = (dest_point_modelspace - bone_pos).Normalised();

			gef::Vector4 axis = (EB.CrossProduct(DB)).Normalised();
			float angle = EB.Angle(DB);

			gef::Quaternion ik_bone_rot = { axis, angle };

			gef::Matrix44 next_matrix = gef::Matrix44(ik_bone_rot.Norm()) * bone_tran;

			gef::Matrix44 old_parent_tran = bone_tran;
			gef::Matrix44 new_parent_tran = next_matrix;

			for (int j = i + 1; true && j < ccd_bones.size(); ++j) {
				int child = ccd_bones[j];

				gef::Matrix44 &child_glob = global_pose[child];

				gef::Matrix44 parent_inv;
				parent_inv.Inverse(old_parent_tran);
				gef::Matrix44 child_local = child_glob * parent_inv;

				old_parent_tran = child_glob;
				child_glob = child_local * new_parent_tran;
				new_parent_tran = child_glob;
			}

			bone_tran = next_matrix;
		}

		// recalculate the end effector position
		end_effector = global_pose[ccd_bones.back()].GetTranslation();
		
		// recalculate distance
		distance = (dest_point_modelspace - end_effector).Length();
		distance_delta = fabsf(old_distance - distance);
		old_distance = distance;

		//if a solution has not been reached, decrement the iterations counter
		--max_iterations;
	}

	// This remain part of the function updates the gef::SkeletonPose with the newly calculate bone
	// transforms.
	// The gef::SkeletonPose interface wasn't originally designed to be updated in this way
	// You may wish to consider altering the interface to this class to remove redundant calculations

	// calculate new local pose of bones in IK chain
	for (size_t i = 0; i < ccd_bones.size(); ++i) {
		int bone_num = ccd_bones[i];

		const gef::Joint &joint = ik_pose.skeleton()->joint(bone_num);
		if (joint.parent == -1) {
			local_pose[bone_num] = global_pose[bone_num];
		}
		else {
			gef::Matrix44 parent_inv;
			parent_inv.Inverse(global_pose[joint.parent]);
			local_pose[bone_num] = global_pose[bone_num] * parent_inv;
		}
	}

	// recalculate global pose based on new local pose
	for (int i = 0; i < ik_pose.skeleton()->joint_count(); ++i) {
		const gef::Joint &joint = ik_pose.skeleton()->joint(i);
		const gef::Matrix44 local = local_pose[i].GetMatrix();
		if (joint.parent == -1)
			global_pose[i] = local;
		else
			global_pose[i] = local * global_pose[joint.parent];
	}

	return max_iterations > 0;
}

// http://antongerdelan.net/opengl/raycasting.html
// https://forum.libcinder.org/topic/picking-ray-from-mouse-coords
static void getScreenPosRay(
	const gef::Vector2 &screen_position, 
	const gef::Matrix44 &projection, 
	const gef::Matrix44 &view, 
	gef::Vector4 &start_point, 
	gef::Vector4 &direction, 
	const gef::Vector2 &screen_sz
) {
	gef::Vector2 half_sz = screen_sz / 2.f;

	gef::Vector2 ndc = {
		(screen_position.x - half_sz.x) / half_sz.x,
		(half_sz.y - screen_position.y) / half_sz.y
	};

	gef::Matrix44 projectionInverse;
	projectionInverse.Inverse(view * projection);

	gef::Vector4 nearPoint, farPoint;

	constexpr float ndc_z_min = 0.0001f;
	nearPoint = gef::Vector4(ndc.x, ndc.y, ndc_z_min, 1.0f).TransformW(projectionInverse);
	farPoint = gef::Vector4(ndc.x, ndc.y, 1.0f, 1.0f).TransformW(projectionInverse);

	nearPoint /= nearPoint.w();
	farPoint /= farPoint.w();

	start_point = gef::Vector4(nearPoint.x(), nearPoint.y(), nearPoint.z());
	direction = farPoint - nearPoint;
	direction.Normalise();
}

// modified and fixed from https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-plane-and-ray-disk-intersection
static bool rayPlaneIntersect(
	gef::Vector4 &start_point, 
	gef::Vector4 &direction, 
	const gef::Vector4 &point_on_plane, 
	const gef::Vector4 &plane_normal, 
	gef::Vector4 &hitpoint
) {
	gef::Vector4 p0, l0, n, l;
	l0 = start_point;
	l = direction;
	p0 = point_on_plane;
	n = plane_normal;
	float t = 0.0f;

	// assuming vectors are all normalized
	float denom = n.DotProduct(l);
	if (fabsf(denom) > 1e-6) {
		gef::Vector4 p0l0 = p0 - l0;
		t = p0l0.DotProduct(n) / denom;

		if (t >= 0)
			hitpoint = start_point + direction * t;

		return (t >= 0);
	}

	return false;
}

