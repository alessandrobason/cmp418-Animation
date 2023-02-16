#include "frame2d_editor.h"

#include <maths/math_utils.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <external/ImGui/imgui.h>
#include <external/ImGui/imgui_internal.h>
#include <external/portable-file-dialogs/portable-file-dialogs.h>
#ifdef _WIN32
#include <platform/d3d11/graphics/texture_d3d11.h>
#endif

#include "anim_system_sprite.h"
#include "utils.h"

constexpr float zoom = 3.f;

void Frame2DEditor::init(AnimSystemSprite *sprite_sys, gef::Platform &plat) {
	PushAllocInfo("SpriteEditor");
	system = sprite_sys;
	plus_texture = loadPng("gui/plus.png", plat);
	PopAllocInfo();
}

void Frame2DEditor::cleanup() {
}

void Frame2DEditor::draw() {
	if (!is_open) {
		return;
	}

	if (!ImGui::Begin("Frame 2D Editor", &is_open)) {
		ImGui::End();
		return;
	}

	auto &animations = system->getAnimations();

	imSaveAndRead(system, "2D Animation Files (.2d)", "*.2d");

	ImGui::SameLine();

	ImGuiStyle &style = ImGui::GetStyle();
	static bool new_anim_popup = false;
	//static bool new_texture_popup = false;
	static char new_anim_name[sizeof(IBaseSpriteAnim::debug_name)];

	if (ImGui::Button("New animation", { 100, 30 })) {
		ImGui::OpenPopup("Add Animation");
		new_anim_popup = true;
		memset(new_anim_name, 0, sizeof(new_anim_name));
	}

	ImGui::SameLine();

	if (ImGui::Button("New texture", { 100, 30 })) {
		std::vector<std::string> dest = pfd::open_file::open_file(
			"Load Texture",
			".",
			{ "PNG File (.png)", "*.png" },
			pfd::opt::none
		).result();
		if (!dest.empty()) {
			system->addTexture(dest[0].c_str());
		}
	}

	if (ImGui::BeginPopupModal("Add Animation", &new_anim_popup)) {
		drawNewAnimationPicker(new_anim_name, sizeof(new_anim_name));
		ImGui::EndPopup();
	}

	int id = 0;
	for (auto &anim : animations) {
		ImGui::Separator();
		ImGui::PushID(id++);

		if (ImGui::Button("Delete", { 100, 30 })) {
			animations.eraseIt(&anim);
		}

		if (auto atlas = dynamic_cast<AtlasAnim*>(anim.get())) {
			ImGui::TextUnformatted("Type: Atlas");
			ImGui::InputText("Name", anim->debug_name, sizeof(anim->debug_name));
			drawAtlasAnimation(atlas);
		}

		if (auto sprite = dynamic_cast<SpriteAnim*>(anim.get())) {
			ImGui::TextUnformatted("Type: Sprite");
			ImGui::InputText("Name", anim->debug_name, sizeof(anim->debug_name));
			drawSpriteAnimation(sprite);
		}

		ImGui::NewLine();
		ImGui::PopID();
	}

	ImGui::End();
}

void Frame2DEditor::drawNewAnimationPicker(char *name_buf, size_t name_len) {
	ImGui::InputText("Name", name_buf, name_len);
	ImGui::Separator();
	ImGui::Text("Animation type");
	if (imBtnFillWidth("Atlas")) {
		ImGui::OpenPopup("atlas_popup");
	}
	imHelper("Atlas based animation where every frame is the same size", false);

	if (ImGui::BeginPopup("atlas_popup")) {
		const auto &names = system->getTexturesName();
		for (size_t i = 1; i < names.size(); ++i) {
			if (ImGui::Selectable(names[i].c_str())) {
				AtlasAnim *anim = g_alloc->make<AtlasAnim>();
				strCopyInto(anim->debug_name, name_buf);
				anim->atlas = system->getTextures()[i].get();
				anim->rows = anim->columns = 1;
				system->getAnimations().emplace_back(anim);
				
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				ImGui::CloseCurrentPopup();
				return;
			}
			if ((i + 1) < names.size()) 
				ImGui::Separator();
		}
		ImGui::EndPopup();
	}

	if (imBtnFillWidth("Sprite")) {
		SpriteAnim *anim = g_alloc->make<SpriteAnim>();
		strCopyInto(anim->debug_name, name_buf);
		system->getAnimations().emplace_back(anim);
		ImGui::CloseCurrentPopup();
		return;
	}
	imHelper(
		"Sprite based animation where every frame can be of "
		"different size and come from a different texture",
		false
	);

}

void Frame2DEditor::drawAtlasAnimation(AtlasAnim *anim) {
	ImGuiStyle &style = ImGui::GetStyle();
	float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	gef::Vector2 atlas_size = anim->atlas ? anim->atlas->GetSize() : gef::Vector2(16, 16);
	gef::Vector2 tile_size = atlas_size / gef::Vector2((float)anim->columns, (float)anim->rows);
	gef::Vector2 zoomed_size = tile_size * zoom;

	ImGui::InputInt("Rows", &anim->rows);
	ImGui::InputInt("Columns", &anim->columns);
	anim->rows = gef::max(anim->rows, 1);
	anim->columns = gef::max(anim->columns, 1);

	for (size_t i = 0; i < anim->frames.size(); ++i) {
		ImGui::PushID((int)i);
		static bool popup_open = false;
		auto &f = anim->frames[i];
		gef::Rect src = {
			(float)(f.id % anim->columns),
			(float)(f.id / anim->columns),
			1.f / anim->columns,
			1.f / anim->rows
		};
		src.pos *= src.size;
		if (imImageBtn("frame", anim->atlas, button_size, zoomed_size, src)) {
			if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
				anim->frames.erase(i);
				if (anim->current_frame > anim->frames.size()) {
					anim->current_frame = 0;
				}
			}
			else {
				ImGui::OpenPopup("Atlas frame options");
				popup_open = true;
			}
		}

		if (ImGui::BeginPopupModal("Atlas frame options", &popup_open, 0)) {
			drawAtlasPicker(f.length, f.id, anim->atlas, anim->columns, anim->rows);
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("ATLAS_FRAME_CELL", &i, sizeof(i));
			ImGui::Text("Swap frames");
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (auto payload = ImGui::AcceptDragDropPayload("ATLAS_FRAME_CELL")) {
				assert(payload->DataSize == sizeof(i));
				size_t payload_i = *(const size_t *)payload->Data;
				auto &frames = anim->frames;
				std::swap(frames[i], frames[payload_i]);
			}
			ImGui::EndDragDropTarget();
		}

		float last_image_x2 = ImGui::GetItemRectMax().x;
		float next_image_x2 = last_image_x2 + style.ItemSpacing.x + button_size.x; // Expected position if next button was on same line
		if (next_image_x2 < window_visible_x2)
			ImGui::SameLine();
		ImGui::PopID();
	}

	static bool new_frame_popup = false;

	if (imImageBtn("plus atlas", plus_texture.get(), button_size)) {
		ImGui::OpenPopup("Atlas new frame");
		new_frame_popup = true;
	}
	
	if (ImGui::BeginPopupModal("Atlas new frame", &new_frame_popup, ImGuiWindowFlags_AlwaysAutoResize * 0)) {
		static int new_id = -1;
		static float new_length = 0.1f;
		if (drawAtlasPicker(new_length, new_id, anim->atlas, anim->columns, anim->rows)) {
			anim->frames.emplace_back(new_id, new_length);
			new_id = -1;
			new_length = 0.1f;
		}
		ImGui::EndPopup();
	}
}

void Frame2DEditor::drawSpriteAnimation(SpriteAnim *anim) {	
	ImGuiStyle &style = ImGui::GetStyle();
	float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	for (size_t i = 0; i < anim->frames.size(); ++i) {
		ImGui::PushID((int)i);
		static bool popup_open = false;
		auto &f = anim->frames[i];
		gef::Vector2 tex_size = f.texture ? f.texture->GetSize() : gef::Vector2(50, 50);
		gef::Rect src = f.source / tex_size;

		if (imImageBtn(
			"Sprite Frame",
			f.texture,
			button_size,
			f.source.size * zoom,
			src
		)) {
			if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
				anim->frames.erase(i);
				if (anim->current_frame > anim->frames.size()) {
					anim->current_frame = 0;
				}
			}
			else {
				ImGui::OpenPopup("Sprite frame options");
				popup_open = true;
			}
		}

		if (ImGui::BeginPopupModal("Sprite frame options", &popup_open, ImGuiWindowFlags_AlwaysAutoResize * 0)) {
			drawSpritePicker(f.texture, f.source, f.offset, f.length);
			ImGui::EndPopup();
		}

		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("SPRITE_FRAME_CELL", &i, sizeof(i));
			ImGui::Text("Swap frames");
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (auto payload = ImGui::AcceptDragDropPayload("SPRITE_FRAME_CELL")) {
				assert(payload->DataSize == sizeof(i));
				size_t payload_i = *(const size_t *)payload->Data;
				auto &frames = anim->frames;
				std::swap(frames[i], frames[payload_i]);
			}
			ImGui::EndDragDropTarget();
		}

		float last_image_x2 = ImGui::GetItemRectMax().x;
		float next_image_x2 = last_image_x2 + style.ItemSpacing.x + button_size.x; // Expected position if next button was on same line
		if (next_image_x2 < window_visible_x2)
			ImGui::SameLine();

		ImGui::PopID();
	}

	static bool new_frame_popup = false;

	if (imImageBtn("plus sprite", plus_texture.get(), button_size)) {
		ImGui::OpenPopup("Sprite new frame");
		new_frame_popup = true;
	}

	if (ImGui::BeginPopupModal("Sprite new frame", &new_frame_popup, ImGuiWindowFlags_AlwaysAutoResize * 0)) {
		static gef::Texture *texture = nullptr;
		static gef::Rect source = { 0, 0, 0, 0 };
		static gef::Vector2 offset = gef::Vector2::kZero;
		static float length = 0.1f;
		if (drawSpritePicker(texture, source, offset, length, true)) {
			anim->frames.emplace_back(source, length, texture, offset);
			texture = nullptr;
			source = { 0, 0, 0, 0 };
			offset = gef::Vector2::kZero;
			length = 0.1f;
		}
		ImGui::EndPopup();
	}
}

bool Frame2DEditor::drawAtlasPicker(float &length, int &id, gef::Texture *atlas, int columns, int rows) {
	constexpr float local_zoom = 2.f;

	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
		return false;
	}

	gef::Vector2 atlas_size = atlas->GetSize();
	gef::Vector2 tile_size = atlas_size / gef::Vector2((float)columns, (float)rows);
	gef::Vector2 src_pos = gef::Vector2::kZero;
	if (id != INVALID_ID) {
		src_pos.x = (float)(id % columns);
		src_pos.y = (float)(id / columns);
		src_pos *= tile_size;
	}
	
	ImGui::DragFloat("Length", &length, 0.005f, 0.f, 2.f);

	if (!ImGui::BeginChild("atlas", { 0, 0 }, true, ImGuiWindowFlags_HorizontalScrollbar)) {
		ImGui::EndChild();
		return false;
	}

	ImGuiStyle &style = ImGui::GetStyle();
	gef::Vector2 child_start = ImGui::GetWindowPos();
	gef::Vector2 child_size = ImGui::GetWindowSize();
	child_size.x -= style.ScrollbarSize;
	child_size.y -= style.ScrollbarSize;

	gef::Vector2 start = ImGui::GetCursorScreenPos();
	ImGui::Image(imGetTexData(atlas), atlas_size * local_zoom);
	ImDrawList *draw_list = ImGui::GetWindowDrawList();

	// draw current frame's rectangle
	gef::Vector2 src_size = tile_size * 2.f;
	if (id != INVALID_ID) {
		src_pos = start + src_pos * local_zoom;
		draw_list->AddRectFilled(
			src_pos,
			src_pos + src_size,
			IM_COL32(255, 255, 0, 100)
		);
		draw_list->AddRect(
			src_pos,
			src_pos + src_size,
			IM_COL32(255, 255, 0, 200)
		);
	}

	// draw mouse frame's rectangle
	if (ImGui::IsMouseHoveringRect(child_start, child_start + child_size)) {
		gef::Vector2 mouse_pos = ImGui::GetMousePos();
		gef::Vector2 mouse_id = (mouse_pos - start) / src_size;
		mouse_id.x = floorf(mouse_id.x);
		mouse_id.y = floorf(mouse_id.y);
		bool is_mouse_valid = false;
		if (mouse_id.x >= 0 && mouse_id.x < columns &&
			mouse_id.y >= 0 && mouse_id.y < rows
			) {
			is_mouse_valid = true;
			src_pos = start + mouse_id * tile_size * local_zoom;
			draw_list->AddRectFilled(
				src_pos,
				src_pos + src_size,
				IM_COL32(255, 255, 0, 20)
			);
			draw_list->AddRect(
				src_pos,
				src_pos + src_size,
				IM_COL32(255, 255, 0, 100)
			);
		}

		if (is_mouse_valid && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			id = (int)mouse_id.y * columns + (int)mouse_id.x;
			ImGui::CloseCurrentPopup();
			ImGui::EndChild();
			return true;
		}
	}

	ImGui::EndChild();
	return false;
}

bool Frame2DEditor::drawSpritePicker(
	gef::Texture *&texture, 
	gef::Rect &source, 
	gef::Vector2 &offset, 
	float &length, 
	bool is_new_frame
) {
	constexpr float local_zoom = 2.f;

	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		ImGui::CloseCurrentPopup();
		return false;
	}

	if (is_new_frame && ImGui::Button("Add", { 100, 40 })) {
		ImGui::CloseCurrentPopup();
		return true;
	}

	auto &textures = system->getTextures();
	auto &names = system->getTexturesName();
	size_t cur_id = 0;

	for (size_t tex_id = 0; tex_id < textures.size(); ++tex_id) {
		if (textures[tex_id].get() == texture) {
			cur_id = tex_id;
			break;
		}
	}

	if (ImGui::BeginCombo("Texture", names[cur_id].c_str())) {
		for (size_t tex_id = 0; tex_id < names.size(); ++tex_id) {
			bool is_selected = tex_id == cur_id;
			if (ImGui::Selectable(names[tex_id].c_str(), is_selected)) {
				cur_id = tex_id;
				texture = textures[cur_id].get();
			}
		}

		ImGui::EndCombo();
	}

	gef::Texture *tex = textures[cur_id].get();
	gef::Vector2 tex_sz = tex ? tex->GetSize() : gef::Vector2::kZero;

	ImGui::Separator();
	ImGui::SliderFloat("x", &source.x, 0.f, tex_sz.x, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SliderFloat("y", &source.y, 0.f, tex_sz.y, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SliderFloat("w", &source.w, 0.f, tex_sz.x, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::SliderFloat("h", &source.h, 0.f, tex_sz.y, "%.0f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::Separator();
	ImGui::DragFloat("offset x", &offset.x);
	ImGui::DragFloat("offset y", &offset.y);
	imHelper("Local offset relative to the top left\n"
		     "mighe be useful when the frames are of different sizes");
	ImGui::Separator();
	ImGui::DragFloat("Length", &length, 0.005f, 0.f, 2.f);

	if (ImGui::BeginChild("Sprite image", { 0, 0 }, true, ImGuiWindowFlags_HorizontalScrollbar)) {
		gef::Vector2 start = ImGui::GetCursorScreenPos();
		ImDrawList *draw_list = ImGui::GetWindowDrawList();
		ImGui::Image(imGetTexData(tex), tex_sz * local_zoom);

		gef::Rect src_zoomed = source * local_zoom;

		if (tex) {
			draw_list->AddRectFilled(
				start + src_zoomed.pos,
				start + src_zoomed.pos + src_zoomed.size,
				IM_COL32(255, 255, 0, 50)
			);
			draw_list->AddRect(
				start + src_zoomed.pos,
				start + src_zoomed.pos + src_zoomed.size,
				IM_COL32(255, 255, 0, 100)
			);

			if (offset != gef::Vector2::kZero) {
				draw_list->AddLine(
					start + src_zoomed.pos,
					start + src_zoomed.pos + offset * local_zoom,
					IM_COL32(255, 50, 20, 100),
					local_zoom
				);
			}
		}
	}
	ImGui::EndChild();

	return false;
}
