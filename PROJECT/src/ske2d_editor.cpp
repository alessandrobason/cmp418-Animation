#include "ske2d_editor.h"

#include <system/platform.h>
#include <graphics/texture.h>
#include <maths/math_utils.h>

#include <external/ImGui/imgui.h>
//#include <external/ImGui/imgui_internal.h>
#include <external/ImGuizmo/ImSequencer.h>

#include "anim_system_ske2d.h"
#include "batch2d.h"
#include "utils.h"

void Ske2DEditor::init(AnimSystemSke2D *ske2d_sys, Batch2D *ren) {
	system = ske2d_sys;
	renderer = ren;

	bone_colour.a = 0.85f;
	selected_colour.a = 0.85f;
}

void Ske2DEditor::cleanup() {
}

void Ske2DEditor::draw() {
	if (!is_open) {
		return;
	}

	if (!ImGui::Begin("Skeleton 2D Editor", &is_open)) {
		ImGui::End();
		return;
	}

	renderer->flush();
	imSaveAndRead(system, "2D Skeleton Animation Files (.ske2d)", "*.ske2d");

	ImGui::ColorEdit4("Bone colour", (float *)&bone_colour);
	ImGui::ColorEdit4("Selected bone colour", (float *)&selected_colour);

	static Slot *selected = nullptr;
	Slot *head = system->getSlotHead();
	drawSlot(head);

	drawBone(head, system->getBaseTransform());

	static bool bones_popup = false;

	if (imBtnFillWidth("Edit bones")) {
		ImGui::OpenPopup("Bones editor");
		bones_popup = true;
	}

	if (ImGui::BeginPopupModal("Bones editor", &bones_popup)) {
		static ImGuiTableFlags flags = ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders;
		if (ImGui::BeginTable("Animation", 2, flags)) {
			ImGui::TableSetupColumn("skin", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("edit", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize);
			drawBoneEditor(head);
			ImGui::EndTable();
		}
		ImGui::EndPopup();
	}

	ImGui::Separator();
	ImGui::Text("Animations:");

	gef::Vec<AnimationSke2D> &animations = system->getAnimations();

	for (AnimationSke2D &anim : animations) {
		ImGui::PushID(&anim);
		static bool popup_open = false;

		if (imBtnFillWidth(anim.name.c_str())) {
			ImGui::OpenPopup("Animation option");
			popup_open = true;
			system->setAnimation(anim.name.c_str());
		}

		if (ImGui::BeginPopupModal("Animation option", &popup_open)) {
			drawAnimation(anim);
			ImGui::EndPopup();
		}
		
		ImGui::PopID();
	}

	if (imBtnFillWidth("+")) {
		AnimationSke2D new_anim;
		new_anim.name = "(none)";
		animations.emplace_back(std::move(new_anim));
	}


	ImGui::End();
}

void Ske2DEditor::drawSlot(Slot *slot) {
	if (!slot || !slot->bone) return;
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;

	if (!slot->first_child) {
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (selected == slot) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// open all the nodes the first time the menu is opened
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	bool is_open = ImGui::TreeNodeEx(slot->bone->name, flags);
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		if (selected == slot) {
			selected = nullptr;
		}
		else {
			selected = slot;
		}
	}

	if (is_open) {
		Slot *child = slot->first_child;
		while (child) {
			drawSlot(child);
			child = child->next;
		}
		ImGui::TreePop();
	}
}

void Ske2DEditor::drawBone(Slot *slot, gef::Matrix33 &transform) {
	if (!slot || !slot->bone || !slot->skin) {
		return;
	}

	Bone *bone = slot->bone;
	Skin *skin = slot->skin;
	Atlas &atlas = system->getAtlas();

	gef::Matrix33 bone_tran = system->getMatFromAnimatedBone(slot->bone) * transform;

	gef::Colour c = bone_colour;

	if (selected == slot) {
		c = selected_colour;
	}

	gef::Vector2 skin_pos = skin->transform.GetTranslation();
	gef::Matrix33 tran = mat3FromPos(skin_pos) * bone_tran;

	constexpr float width = 10.f;
	if (bone->length) {
		float len = bone->length / 2.f;

		renderer->drawTri(
			gef::Vector2::Transform(tran, { -len, 0 }),
			gef::Vector2::Transform(tran, { len, 0 }),
			gef::Vector2::Transform(tran, { -len + width, width }),
			c, c, c
		);

		renderer->drawTri(
			gef::Vector2::Transform(tran, { -len, 0 }),
			gef::Vector2::Transform(tran, { -len + width, -width }),
			gef::Vector2::Transform(tran, { len, 0 }),
			c, c, c
		);
	}
	else {
		float len = 2.5f;

		gef::Rect dst = { -len, -len, len * 2.f, len * 2.f };
		dst.pos = dst.pos.Transform(tran);

		renderer->drawRect(dst, c);
	}

	Slot *child = slot->first_child;
	while (child) {
		drawBone(child, bone_tran);
		child = child->next;
	}
}

void Ske2DEditor::drawBoneEditor(Slot *slot) {
	if (!slot || !slot->bone || !slot->skin) return;

	Bone *bone = slot->bone;
	Skin *skin = slot->skin;
	Atlas &atlas = system->getAtlas();
	SubTexture &sub = atlas.sub_textures[skin->sub_texture];
	gef::Texture *tex = atlas.texture;

	ImGui::PushID(slot);

	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);

	float max_width = ImGui::GetContentRegionAvail().x;
	gef::Vector2 size = tex->GetSize();
	if (size.x > max_width) {
		size.y = size.y * max_width / size.x;
		size.x = max_width;
	}

	ImGui::Image(imGetTexData(tex), size, sub.uv.pos, sub.uv.pos + sub.uv.size);

	ImGui::TableSetColumnIndex(1);
	ImGui::InputText("Name", bone->name, sizeof(bone->name));
	ImGui::DragFloat("Length", &bone->length, 0.1f, 0.f);
	ImGui::DragFloat("Rotation", &bone->rotation, 0.1f, 0.f, 0.f, "%.3f rad");
	ImGui::DragFloat2("Scale", (float *)&bone->scale, 0.1f);
	ImGui::DragFloat2("Translation", (float *)&bone->translation, 0.1f);
	
	ImGui::DragFloat4("UV", (float *)&sub.uv, 0.01f, 0.f, 1.f);

	ImGui::PopID();

	bone->length = gef::max(bone->length, 0.f);

	Slot *child = slot->first_child;
	while (child) {
		drawBoneEditor(child);
		child = child->next;
	}
}


void Ske2DEditor::drawAnimation(AnimationSke2D &anim) {
	imInputText("Name", anim.name);
	if (imBtnFillWidth("Delete")) {
		system->getAnimations().eraseIt(&anim);
		ImGui::CloseCurrentPopup();
		return;
	}

	for (auto &[bone, bone_anim] : anim.bone_animations) {
		ImGui::PushID(bone);

		if (ImGui::TreeNodeEx(bone->name, ImGuiTreeNodeFlags_SpanFullWidth)) {
			if (imBtnFillWidth("Remove")) {
				bone_anim->rotations.clear();
				bone_anim->translations.clear();
				if (!listRemove(anim.head, bone_anim)) {
					fatal("couldn't find bone animation in list");
				}
				bone_anim = nullptr;
				anim.bone_animations.erase(bone);
				ImGui::TreePop();
				ImGui::PopID();
				return;
			}

			if (ImGui::BeginTable("Animation", 2)) {
				auto &rotations = bone_anim->rotations;
				auto &translations = bone_anim->translations;
				size_t len = gef::max(rotations.size(), translations.size());

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Rotation frames");
				ImGui::Separator();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Translation frames");
				ImGui::Separator();
				ImGui::TableNextRow();

				for (size_t i = 0; i < len; ++i) {
					ImGui::PushID((int)i);

					ImGui::TableNextColumn();
					ImGui::PushID(0);
					if (i < rotations.size()) {
						AnimationRotation &rot = rotations[i];
						ImGui::DragFloat("Angle", &rot.angle, 0.1f, 0.f, 0.f, "%.3f rad", ImGuiSliderFlags_AlwaysClamp);
						ImGui::DragFloat("Duration", &rot.duration, 0.05f, 0.f, 0.f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
						ImGui::Checkbox("Is interpolated", &rot.tween);
						rot.duration = gef::max(rot.duration, 0.f);
						if (imBtnFillWidth("Remove")) {
							rotations.erase(i);
						}
						ImGui::Separator();
					}
					ImGui::PopID();

					ImGui::PushID(1);
					ImGui::TableNextColumn();
					if (i < translations.size()) {
						AnimationTranslation &tran = translations[i];
						ImGui::DragFloat2("Offset", (float *)&tran.offset);
						ImGui::DragFloat("Duration", &tran.duration, 0.05f, 0.f, 0.f, "%.3f sec", ImGuiSliderFlags_AlwaysClamp);
						ImGui::Checkbox("Is interpolated", &tran.tween);
						tran.duration = gef::max(tran.duration, 0.f);
						if (imBtnFillWidth("Remove")) {
							translations.erase(i);
						}
						ImGui::Separator();
					}
					ImGui::PopID();

					ImGui::PopID();
				}

				ImGui::TableNextColumn();
				if (imBtnFillWidth("Add##rotation")) {
					bone_anim->rotations.emplace_back();
				}
				ImGui::TableNextColumn();
				if (imBtnFillWidth("Add##translation")) {
					bone_anim->translations.emplace_back();
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	static bool popup_open = false;

	if (imBtnFillWidth("Add")) {
		ImGui::OpenPopup("Add Bone");
		popup_open = true;
	}

	if (ImGui::BeginPopupModal("Add Bone", &popup_open)) {
		Bone *bone = system->getBoneHead();
		while (bone) {
			auto &it = anim.bone_animations.find(bone);
			if (it == anim.bone_animations.end()) {
				if (imBtnFillWidth(bone->name)) {
					break;
				}
			}
			bone = bone->next;
		}
		if (bone) {
			Arena &arena = system->getArena();
			BoneAnimation *bone_anim = arena.make<BoneAnimation>(arena);
			bone_anim->next = anim.head;
			anim.head = bone_anim;
			anim.bone_animations[bone] = bone_anim;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
