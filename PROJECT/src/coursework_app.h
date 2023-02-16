#ifndef _ANIMATED_MESH_APP_H
#define _ANIMATED_MESH_APP_H

#include <vector>

#include <system/application.h>
#include <maths/vector2.h>
#include <maths/vector4.h>
#include <maths/matrix44.h>

#include "anim_system.h"
#include "anim_system_sprite.h"
#include "anim_system_ske2d.h"
#include "anim_system_3d.h"
#include "anim_system_ik.h"

#include "frame2d_editor.h"
#include "ske2d_editor.h"
#include "anim3d_editor.h"

#include "batch2d.h"
#include "blend_tree.h"

// FRAMEWORK FORWARD DECLARATIONS
namespace gef {
class Platform;
class SpriteRenderer;
class Font;
class Renderer3D;
class Mesh;
class Scene;
class Skeleton;
class InputManager;
}

class CourseworkApp : public gef::Application {
public:
	CourseworkApp(gef::Platform &platform);
	virtual void Init() override;
	virtual void CleanUp() override;
	virtual bool Update(float frame_time) override;
	virtual void Render() override;

private:
	void InitFont();
	void CleanUpFont();
	void DrawHUD();
	void SetupLights();
	void SetupCamera();

	gef::Renderer3D *renderer_3d_ = nullptr;
	gef::InputManager *input_manager_ = nullptr;
	gef::Font *font_ = nullptr;

	float fps_ = 0.f;
	bool is_centered = true;

	gef::Vector4 camera_eye_;
	gef::Vector4 camera_lookat_;
	gef::Vector4 camera_up_;
	float camera_fov_;
	float near_plane_;
	float far_plane_;

	AnimSystem *cur_system = nullptr;
	AnimSystemSprite animsprite;
	AnimSystemSke2D animske2d;
	AnimSystem3D anim3d;
	AnimSystemIK animik;

	Frame2DEditor frame2d_editor;
	Ske2DEditor ske2d_editor;
	Anim3DEditor anim3d_editor;

	Batch2D batch;
};

#endif // _ANIMATED_MESH_APP_H
