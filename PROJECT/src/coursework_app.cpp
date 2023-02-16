#include "coursework_app.h"
#include <system/platform.h>
#include <graphics/sprite_renderer.h>
#include <graphics/texture.h>
#include <graphics/mesh.h>
#include <graphics/primitive.h>
#include <assets/png_loader.h>
#include <graphics/image_data.h>
#include <graphics/font.h>
#include <maths/vector2.h>
#include <input/input_manager.h>
#include <input/sony_controller_input_manager.h>
#include <input/keyboard.h>
#include <input/touch_input_manager.h>
#include <maths/math_utils.h>
#include <graphics/renderer_3d.h>
#include <graphics/scene.h>
#include <animation/skeleton.h>
#include <animation/animation.h>

#include <system/allocator.h>

#include <external/imgui/imgui.h>

#include "scene_loader.h"
#include "utils.h"

CourseworkApp::CourseworkApp(gef::Platform &platform) :
	Application(platform)
{
}

void CourseworkApp::Init() {
	renderer_3d_ = gef::Renderer3D::Create(platform_);
	input_manager_ = gef::InputManager::Create(platform_);

	PushAllocInfo("App");

	InitFont();
	SetupCamera();
	SetupLights();

	animik.init(platform_, renderer_3d_, input_manager_, "xbot/xbot.scn");

	anim3d.init(platform_, renderer_3d_, "xbot/xbot.scn");

	bool loaded_default = false;
	{
		CFile default_file = "animations/default.blend3d";
		if (anim3d.checkFormatVersion(default_file)) {
			anim3d.read(default_file);
			loaded_default = true;
		}
	}
	if (!loaded_default) {
		anim3d.loadAnimation("xbot/xbot@running.scn", "running");
		anim3d.loadAnimation("xbot/xbot@left_strafe.scn", "left strafe");
		anim3d.loadAnimation("xbot/xbot@right_strafe.scn", "right strafe");
		anim3d.loadAnimation("xbot/xbot@idle.scn", "idle");
		anim3d.loadAnimation("xbot/xbot@jump.scn", "jump");
		anim3d.loadAnimation("xbot/xbot@dancing.scn", "dancing");
		BlendTree &tree = anim3d.getBlendTree();
		ClipNode *clip = tree.arena.make<ClipNode>(tree);
		clip->clip = anim3d.getAnimation(0);
		tree.all_nodes.emplace_back(clip);
		tree.exit_node = clip;
	}

	animske2d.init(&batch, platform_);
	animske2d.loadFromDragonBones("dragon/Dragon_tex.json", "dragon/Dragon_ske.json");
	animske2d.setAnimation(0);

	animsprite.init(&batch, platform_);

	frame2d_editor.init(&animsprite, platform_);
	ske2d_editor.init(&animske2d, &batch);
	anim3d_editor.init(&anim3d, platform_);

	anim3d_editor.generate();

	gef::Transform tran;

	tran = animsprite.getTransform();
	tran.set_scale({ 5.f, 5.f, 5.f });
	tran.set_translation(platform_.size() / 2.f);
	animsprite.setTransform(tran);

	tran = animske2d.getTransform();
	tran.set_scale({ 0.5f, 0.5f, 0.5f });
	tran.set_translation(platform_.size() / 2.f);
	animske2d.setTransform(tran);

	tran = anim3d.getTransform();
	tran.set_scale({ 0.01f, 0.01f, 0.01f });
	anim3d.setTransform(tran);

	tran = animik.getTransform();
	tran.set_scale({ 0.01f, 0.01f, 0.01f });
	animik.setTransform(tran);

	cur_system = &animsprite;

	batch.init(platform_);
}

void CourseworkApp::CleanUp() {
	PopAllocInfo();

	CleanUpFont();

	animik.cleanup();
	anim3d.cleanup();
	animske2d.cleanup();
	animsprite.cleanup();

	frame2d_editor.cleanup();
	ske2d_editor.cleanup();
	anim3d_editor.cleanup();

	batch.cleanup();

	g_alloc->destroy(input_manager_);
	input_manager_ = NULL;

	g_alloc->destroy(renderer_3d_);
	renderer_3d_ = NULL;
}

bool CourseworkApp::Update(float frame_time) {
	fps_ = 1.0f / frame_time;

	static float time = 0.f;
	time += frame_time;

	// read input devices
	if (input_manager_) {
		input_manager_->Update();

		// controller input
		if (auto controller = input_manager_->controller_input()) {
		}

		// keyboard input
		if (auto keyboard = input_manager_->keyboard()) {
		}

		if (auto mouse = input_manager_->touch_manager()) {
			gef::Vector2 pos = mouse->mouse_position();
			gef::Vector2 sz = platform_.size();
			pos = gef::clamp(pos, gef::Vector2::kZero, sz);
			// get in range (0, 1)
			pos = pos / sz;
			// get in range (-1, 1)
			pos = pos * 2.f - 1.f;
			anim3d.getBlendTree().setValue("mouse x", pos.x);
			anim3d.getBlendTree().setValue("mouse y", pos.y);
		}
	}

	if (!cur_system) {
		return true;
	}

	cur_system->update(frame_time);

	if (is_centered) {
		if (cur_system->is2D()) {
			gef::Transform tran = cur_system->getTransform();
			tran.set_translation(platform_.size() / 2.f);
			cur_system->setTransform(tran);
		}
	}

	return true;
}

void CourseworkApp::Render() {
	if (!cur_system) {
		return;
	}

	// setup view and projection matrices
	gef::Matrix44 projection_matrix;
	gef::Matrix44 view_matrix;
	projection_matrix = platform_.PerspectiveProjectionFov(camera_fov_, (float)platform_.width() / (float)platform_.height(), near_plane_, far_plane_);
	view_matrix.LookAt(camera_eye_, camera_lookat_, camera_up_);
	renderer_3d_->set_projection_matrix(projection_matrix);
	renderer_3d_->set_view_matrix(view_matrix);
	
	// draw meshes here
	renderer_3d_->Begin();

	if (!cur_system->is2D()) {
		cur_system->draw();
	}

	renderer_3d_->End();

	ImGui::Begin("Animations");

	static gef::Vector4 cur_pos = cur_system->getTransform().translation();

	const char *anim_types[] = { "Sprite 2D", "Skeleton 2D", "Skeleton 3D", "Inverse Kinematics"};
	int cur_type = (int)cur_system->getType() - 1;
	if (ImGui::ListBox("Type", &cur_type, anim_types, (int)AnimSystemType::Count - 1)) {
		frame2d_editor.close();
		ske2d_editor.close();
		anim3d_editor.close();
		switch ((AnimSystemType)(cur_type + 1)) {
		case AnimSystemType::Sprite2D:   cur_system = &animsprite; break;
		case AnimSystemType::Skeleton2D: cur_system = &animske2d;  break;
		case AnimSystemType::Skeleton3D: cur_system = &anim3d;     break;
		case AnimSystemType::InverseKinematics: cur_system = &animik; break;
		}
		cur_pos = cur_system->getTransform().translation();
	}

	if (cur_system && cur_system->getType() != AnimSystemType::InverseKinematics) {
		if (ImGui::Button("Open editor", { 100, 25 })) {
			frame2d_editor.close();
			ske2d_editor.close();
			anim3d_editor.close();
			switch (cur_system->getType()) {
			case AnimSystemType::Sprite2D:   frame2d_editor.open(); break;
			case AnimSystemType::Skeleton2D: ske2d_editor.open();   break;
			case AnimSystemType::Skeleton3D: anim3d_editor.open(); break;
			}
		}

		int cur_id = cur_system->getCurrentId();
		if (ImGui::InputInt("Current", &cur_id)) {
			cur_system->setAnimation(cur_id);
		}

		if (ImGui::Checkbox("Center model", &is_centered)) {
			// when the checkbox is unchecked, update the cur_pos value
			if (!is_centered) {
				cur_pos = cur_system->getTransform().translation();
			}
			// when the checkbox is checked, update the translation only once for
			// the 3D renderer, compared to the 2D one it doesn't need the screen size
			// to be centred
			else if (!cur_system->is2D()) {
				gef::Transform tran = cur_system->getTransform();
				tran.set_translation(gef::Vector4::kZero);
				cur_system->setTransform(tran);
			}
		}
		
		if (!is_centered) {
			float min_v = 0.f, max_v = 0.f, speed = 1.f;
			if (!cur_system->is2D()) {
				min_v = -2.f; max_v = 2.f; speed = 0.01f;
			}
			if (ImGui::DragFloat3("Position", (float *)&cur_pos, speed, min_v, max_v)) {
				gef::Transform tran = cur_system->getTransform();
				tran.set_translation(cur_pos);
				cur_system->setTransform(tran);
			}
		}

		ImGui::Separator();

		cur_system->debugDraw();
	}

	ImGui::End();

	static bool show_alloc = false;

	if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
		show_alloc = !show_alloc;
	}

	if (show_alloc && g_debug_alloc) {
		const auto &alloc_info = g_debug_alloc->alloc_info;
		if (ImGui::Begin("Allocations", &show_alloc)) {
			size_t total = 0;
			for (const auto &info : alloc_info) {
				total += info.size;
			}
			ImGui::Text("Total: %zub - %zuKB - %zuMB", total, total / 1024, total / (1024 * 1024));
			if (ImGui::BeginTable("AllocInfo", 4, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("size");
				ImGui::TableSetupColumn("Extra");
				ImGui::TableSetupColumn("Pointer");
				ImGui::TableHeadersRow();

				for (const auto &info : alloc_info) {
					ImGui::TableNextColumn();
					ImGui::Text("%s", info.name);
					ImGui::TableNextColumn();
					ImGui::Text("%zu", info.size);
					ImGui::TableNextColumn();
					ImGui::Text("%s", info.extra);
					ImGui::TableNextColumn();
					ImGui::Text("%p", info.ptr);
				}
				ImGui::EndTable();
			}
		}
		ImGui::End();
	}

	frame2d_editor.draw();
	anim3d_editor.draw();

	batch.begin(false);
	
	if (cur_system->is2D()) {
		cur_system->draw();
	}

	if (cur_system->getType() == AnimSystemType::InverseKinematics) {
		animik.drawPointer(batch);
	}

	ske2d_editor.draw();

	DrawHUD();

	batch.end();
}

void CourseworkApp::InitFont() {
	PushAllocInfo("font");
	font_ = g_alloc->make<gef::Font>(platform_);
	font_->Load("comic_sans");
	PopAllocInfo();
}

void CourseworkApp::CleanUpFont() {
	g_alloc->destroy(font_);
	font_ = NULL;
}

void CourseworkApp::DrawHUD() {
	if (font_) {
		// display frame rate
		constexpr float x_off = 130.f;
		constexpr float y_off = 40.f;
		const float width = (float)platform_.width();
		const float height = (float)platform_.height();
		batch.drawTextF(font_, gef::Vector2(width - x_off, height - y_off), gef::Colour::white, "FPS: %.1f", fps_);
	}
}

void CourseworkApp::SetupLights() {
	gef::PointLight default_point_light;
	default_point_light.set_colour(gef::Colour(0.7f, 0.7f, 1.0f, 1.0f));
	default_point_light.set_position(gef::Vector4(-300.0f, -500.0f, 100.0f));

	gef::Default3DShaderData &default_shader_data = renderer_3d_->default_shader_data();
	default_shader_data.set_ambient_light_colour(gef::Colour(0.5f, 0.5f, 0.5f, 1.0f));
	default_shader_data.AddPointLight(default_point_light);
}

void CourseworkApp::SetupCamera() {
	// initialise the camera settings
	camera_eye_ = gef::Vector4(-1.0f, 1.0f, 4.0f);
	camera_lookat_ = gef::Vector4(0.0f, 1.0f, 0.0f);
	camera_up_ = gef::Vector4(0.0f, 1.0f, 0.0f);
	camera_fov_ = gef::DegToRad(45.0f);
	near_plane_ = 0.01f;
	far_plane_ = 1000.f;
}
