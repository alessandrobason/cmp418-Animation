#include "anim3d_editor.h"

#include <fstream>

#include <external/ImGui/imgui.h>

#include <external/imgui_node/imgui_node_editor_internal.h>

#include "anim_system_3d.h"
#include "blend_tree.h"

namespace ed = ax::NodeEditor;

static const char *node_type_to_name[] = {
	"Input", "Clip Node", "Synced Clip Node",
	"Linear Blend Node", "1D Blend Node",
	"Output Node"
};
constexpr int node_types_len = (sizeof(node_type_to_name) / sizeof(*node_type_to_name));

static const gef::Colour node_colours[] = {
	gef::Colour::blue,       // input
	gef::Colour::green,      // clip
	gef::Colour::dark_green, // sync clip
	gef::Colour::red,        // blend
	gef::Colour::purple,     // blend1d
	gef::Colour::orange,     // output
};

static_assert(node_types_len == (int)Node::Type::Count);
static_assert((sizeof(node_colours)/sizeof(*node_colours)) == (int)Node::Type::Count);

// == NODE EDITOR DATA ====================================

bool Pin::canConnect(Pin *other) {
	if (!other) return false;
	bool is_diff = id != other->id &&
				   kind != other->kind &&
				   node != other->node;
	return is_diff && type == other->type;
}

Node::Node(Arena &arena) {
	inputs.setAllocator(&arena);
}

// == ANIMATION 3D EDITOR =================================

void Anim3DEditor::init(AnimSystem3D *anim_sys, gef::Platform &plat) {
	system = anim_sys;
	tree = &system->getBlendTree();
	platform = &plat;

	ctx = ed::CreateEditor();
	addOutputNode();
}

void Anim3DEditor::cleanup() {
	ed::DestroyEditor(ctx);
	links.destroy();
	head_node = nullptr;
	first_free = nullptr;
	arena.cleanup();
}

void Anim3DEditor::draw() {
	if (!is_open) {
		return;
	}

	if (!ImGui::Begin("Animation 3D Editor", &is_open)) {
		ImGui::End();
		return;
	}

	int result = imSaveAndRead(system, "3D Blend Tree File (.blend3d)", "*.blend3d");
	if (result == ResultRead) {
		generate();
	}

	ImGui::SameLine();

	if (ImGui::Button("Build tree", { 100, 30 })) {
		if (!buildTree()) {
			ImGui::OpenPopup("Error");
		}
	}

	if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Couldn't build animation blend tree, reason");
		ImGui::TextUnformatted(fail_reason.get());
		if (imBtnFillWidth("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ed::SetCurrentEditor(ctx);
	ed::Begin("Blend Tree");

	for (Node *node = head_node; node; node = node->next) {
		ed::GetStyle().Colors[ed::StyleColor_NodeBorder] = node_colours[(int)node->type];

		auto context = (ed::Detail::EditorContext *)ctx;
		bool is_first_time = !context->FindNode(node->id);

		ed::BeginNode(node->id);
		ImGui::PushID(node);

		if (is_first_time) {
			ed::SetNodePosition(node->id, node->pos);
		}

		ImGui::Text(node_type_to_name[(int)node->type]);

		switch (node->type) {
		case Node::Type::Anim:     drawClip(node); break;
		case Node::Type::Clip:     drawClipNode(node); break;
		case Node::Type::SyncClip: drawSyncedClipNode(node); break;
		case Node::Type::Blend:    drawBlendNode(node); break;
		case Node::Type::Blend1D:  drawBlendNode1D(node); break;
		case Node::Type::Output:   drawOutputNode(node); break;
		default: fatal("unrecognized node type"); break;
		}

		ImGui::PopID();
		ed::EndNode();
	}

	for (Link &link : links) {
		ed::Link(link.id, link.start->id, link.end->id);
	}

	if (ed::GetBackgroundClickButtonIndex() == ImGuiMouseButton_Right) {
		new_node_popup = -1;
		new_node_pos = ed::CanvasToScreen(ImGui::GetMousePos());
	}

	if (ed::BeginCreate()) {
		ed::PinId start, end;
		if (ed::QueryNewLink(&start, &end)) {
			if (start && end) {
				// user went from input to output
				Pin *beg = findPin(start);
				Pin *fin = findPin(end);
				if (beg && fin && beg->canConnect(fin)) {
					if (fin->kind == ed::PinKind::Output) {
						std::swap(beg, fin);
					}
					
					bool link_exists = false;
					for (Link &link : links) {
						if (link.start == beg && link.end == fin) {
							link_exists = true;
							break;
						}
					}

					if (link_exists) {
						ed::RejectNewItem(gef::Colour::red, 2.f);
					}
					else if (ed::AcceptNewItem(gef::Colour::green, 2.f)) {
						for (Link &link : links) {
							if (link.end == fin) {
								links.eraseIt(&link);
								break;
							}
						}

						links.push_back({ unique_id++, beg, fin });
						Link &newlink = links.back();
						ed::Link(newlink.id, newlink.start->id, newlink.end->id);
					}
				}
				else {
					ed::RejectNewItem(gef::Colour::red, 2.f);
				}
			}
		}
		ed::PinId node_pin;
		if (ed::QueryNewNode(&node_pin)) {
			if (ed::AcceptNewItem()) {
				new_node_popup = (Int64)node_pin.Get();
				new_node_pos = ImGui::GetMousePos();
			}
		}
	}
	ed::EndCreate();

	if (ed::BeginDelete()) {
		ed::LinkId link_id;
		while (ed::QueryDeletedLink(&link_id)) {
			if (ed::AcceptDeletedItem()) {
				for (Link &link : links) {
					if (link.id == link_id) {
						links.eraseSwapIt(&link);
						break;
					}
				}
			}
		}

		ed::NodeId node_id;
		while (ed::QueryDeletedNode(&node_id)) {
			Node *deleted = nullptr;
			size_t node_ind = 0;
			for (Node *node = head_node; node; node = node->next) {
				if (node->id == node_id) {
					if (node->type != Node::Type::Output) {
						deleted = node;
					}
					break;
				}
			}

			if (deleted) {
				// remove all the links that are connected to the node
				for (size_t i = 0; i < links.size(); ++i) {
					if (links[i].start->node == deleted ||
						links[i].end->node == deleted
					) {
						links.eraseSwap(i--);
					}
				}

				if (ed::AcceptDeletedItem()) {
					removeNode(deleted);
					first_free = deleted;
					deleted->next = first_free;
					// don't need a doubly linked list for the free list
					// we only ever pop from the top
					deleted->prev = nullptr;
					// reset values
					*deleted = { arena };
				}
			}
			else {
				ed::RejectDeletedItem();
			}
		}
	}
	ed::EndDelete();

	ed::Suspend();

	if (clip_popup) {
		if (!clip_popup.is_open) {
			ImGui::OpenPopup("Clip Chooser");
			clip_popup.is_open = true;
		}

		if (ImGui::BeginPopup("Clip Chooser")) {
			int anim_id = 0;
			Animation3D *anim = system->getAnimation(anim_id++);
			while (anim) {
				bool is_selected = anim == clip_popup.data->clip;

				if (ImGui::Selectable(anim->name, is_selected)) {
					clip_popup.data->clip = anim;
					clip_popup.reset();
					ImGui::CloseCurrentPopup();
					break;
				}

				if (is_selected) {
					ImGui::SetItemDefaultFocus();
				}

				anim = system->getAnimation(anim_id++);
			}

			ImGui::EndPopup();
		}
		else {
			clip_popup.reset();
		}
	}

	if (new_node_popup) {
		if (!new_node_popup.is_open) {
			ImGui::OpenPopup("New Node Chooser");
			new_node_popup.is_open = true;
		}

		if (ImGui::BeginPopup("New Node Chooser")) {
			// skip last one as it is the output node, which we spawn 
			// ourself at the start
			for (int i = 0; i < (node_types_len - 1); ++i) {
				if (ImGui::Selectable(node_type_to_name[i])) {
					switch ((Node::Type)i) {
					case Node::Type::Anim:     addClip(); break;
					case Node::Type::Clip:     addClipNode(); break;
					case Node::Type::SyncClip: addSyncedClipNode(); break;
					case Node::Type::Blend:    addBlendNode(); break;
					case Node::Type::Blend1D:  addBlendNode1D(); break;
					default: fatal("unknown node type"); break;
					}

					Node *node = head_node;
					ed::SetNodePosition(node->id, ed::ScreenToCanvas(new_node_pos));

					if (new_node_popup.data > 0) {
						if (Pin *start = findPin((uintptr_t)new_node_popup.data)) {
							Pin *end = node->inputs.begin();
							if (start->kind == ed::PinKind::Input) {
								end = start;
								start = &node->output;
							}
							if (start->canConnect(end)) {
								links.push_back({ unique_id++, start, end });
							}
						}
					}

					clip_popup.reset();
					ImGui::CloseCurrentPopup();
					break;
				}
			}

			ImGui::EndPopup();
		}
		else {
			new_node_popup.reset();
		}
	}

	if (bind_popup) {
		if (!bind_popup.is_open) {
			ImGui::OpenPopup("Bind Name Chooser");
			bind_popup.is_open = true;
		}

		if (ImGui::BeginPopup("Bind Name Chooser")) {
			imInputText("Name", bind_popup.data->bind_name);
			ImGui::EndPopup();
		}
		else {
			if (bind_popup.data->bind_name.empty()) {
				bind_popup.data->bind_value = false;
			}
			bind_popup.reset();
		}
	}

	ed::Resume();

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::End();
}

static const std::string *isBinded(BlendTree *tree, float *value) {
	if (!value) return nullptr;
	for (const auto &[name, ptr] : tree->value_map) {
		if (ptr == value) return &name;
	}
	return nullptr;
}

void Anim3DEditor::generateFromNode(
	ITreeNode *base_node, 
	Node *child, 
	Pin &pin, 
	gef::Vector2 &pos
) {
	if (!base_node) return;

	constexpr float offset_x = 200.f;

	ITreeNode *new_node = nullptr;
	Node *new_clip = nullptr;

	switch (base_node->node_type) {
	case NodeType::Clip: 
	{
		ClipNode *node = (ClipNode *)base_node;
		gef::Vector2 p = pos;
		
		addClipNode();
		p.x -= offset_x;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &pin });
		
		new_node = node;
		new_clip = head_node;
		
		addClip(node->clip);
		p.x -= offset_x;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &new_clip->inputs[0] });
		head_node->float_value = node->clip->playback_speed;
		
		pos.y += 100.f;

		break;
	}
	case NodeType::SyncClip:
	{
		SyncedClipNode *node = (SyncedClipNode *)base_node;
		gef::Vector2 p = pos;
		
		addSyncedClipNode();
		p.x -= offset_x;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &pin });
		
		new_node = node;
		new_clip = head_node;
		
		addClip(node->clip);
		p.x -= offset_x;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &new_clip->inputs[0] });
		head_node->float_value = node->clip->playback_speed;
		
		addClip(node->leader_clip);
		p.y += 100.f;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &new_clip->inputs[1] });
		head_node->float_value = node->leader_clip->playback_speed;
		
		pos.y = p.y + 125.f;

		break;
	}
	case NodeType::Blend:
	{
		BlendNode *node = (BlendNode *)base_node;
		gef::Vector2 p = pos;

		addBlendNode();
		p.x -= offset_x;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &pin });
		head_node->float_value = node->blending_value;

		new_node = node;
		new_clip = head_node;

		generateFromNode(node->input_nodes[0], new_clip, new_clip->inputs[0], p);
		generateFromNode(node->input_nodes[1], new_clip, new_clip->inputs[1], p);

		pos.y = p.y + 25.f;

		break;
	}
	case NodeType::Blend1D:
	{
		BlendNode1D *node = (BlendNode1D *)base_node;
		gef::Vector2 p = pos;

		addBlendNode1D();
		p.x -= offset_x;
		head_node->pos = p;
		links.push_back({ unique_id++, &head_node->output, &pin });
		head_node->float_value = node->blending_value;

		new_node = node;
		new_clip = head_node;

		generateFromNode(node->input_nodes[0], new_clip, new_clip->inputs[0], p);
		generateFromNode(node->input_nodes[1], new_clip, new_clip->inputs[1], p);
		generateFromNode(node->input_nodes[2], new_clip, new_clip->inputs[2], p);

		pos.y = p.y + 25.f;

		break;
	}
	}

	assert(new_clip);
	
	float *bind_value = new_node->getInputValue();
	if (auto bound_name = isBinded(tree, bind_value)) {
		new_clip->bind_name = *bound_name;
		new_clip->bind_value = *bind_value;
	}
}

void Anim3DEditor::generate() {
	assert(tree);
	// cleanup
	cleanup();
	// recreate the basic data that we need
	ctx = ed::CreateEditor();
	addOutputNode();

	// start from last node and build from there
	gef::Vector2 pos = gef::Vector2::kZero;
	generateFromNode(tree->exit_node, head_node, head_node->inputs[0], pos);
}

void Anim3DEditor::drawPinIn(const Pin &pin, const char *text) {
	ed::BeginPin(pin.id, ed::PinKind::Input);
		ed::PinPivotAlignment({ 0.f, 0.5f });
		ImGui::Text(text);
	ed::EndPin();
}

void Anim3DEditor::drawPinOut(const Pin &pin, const char *text) {
	ed::BeginPin(pin.id, ed::PinKind::Output);
		ed::PinPivotAlignment({ 1.f, 0.5f });
		ImGui::Text(text);
	ed::EndPin();
}

void Anim3DEditor::drawClip(Node *node) {
	assert(node && node->inputs.empty());

	if (ImGui::Button(node->clip ? node->clip->name : "clip")) {
		assert(!clip_popup);
		clip_popup = node;
	}

	ImGui::SameLine();
	
	drawPinOut(node->output);

	ImGui::SetNextItemWidth(50.f);
	ImGui::DragFloat("Speed", &node->float_value, 0.01f, 0.f, 1.f);
}

void Anim3DEditor::drawClipNode(Node *node) {
	assert(node && node->inputs.size() == 1);

	auto &input = node->inputs.front();

	drawPinIn(input, "-> clip");

	ImGui::SameLine();

	drawPinOut(node->output);

	if (ImGui::Checkbox("Bind", &node->bind_value) && node->bind_value) {
		bind_popup = node;
	}
}

void Anim3DEditor::drawSyncedClipNode(Node *node) {
	assert(node && node->inputs.size() == 2);

	drawPinIn(node->inputs[0], "-> clip");

	ImGui::SameLine();

	drawPinOut(node->output);

	drawPinIn(node->inputs[1], "-> leader");

	if (ImGui::Checkbox("Bind", &node->bind_value) && node->bind_value) {
		bind_popup = node;
	}
}

void Anim3DEditor::drawBlendNode(Node *node) {
	assert(node && node->inputs.size() == 2);

	drawPinIn(node->inputs[0], "-> from");
	
	ImGui::SameLine();

	drawPinOut(node->output);

	drawPinIn(node->inputs[1], "-> to");

	ImGui::SetNextItemWidth(50.f);
	ImGui::DragFloat("Blend", &node->float_value, 0.01f, 0.f, 1.f);

	if (ImGui::Checkbox("Bind", &node->bind_value) && node->bind_value) {
		bind_popup = node;
	}
}

void Anim3DEditor::drawBlendNode1D(Node *node) {
	assert(node && node->inputs.size() == 3);

	drawPinIn(node->inputs[0], "-> left");

	ImGui::SameLine();

	drawPinOut(node->output);

	drawPinIn(node->inputs[1], "-> centre");
	drawPinIn(node->inputs[2], "-> right");

	ImGui::SetNextItemWidth(50.f);
	ImGui::DragFloat("Blend", &node->float_value, 0.01f, -1.f, 1.f);

	if (ImGui::Checkbox("Bind", &node->bind_value) && node->bind_value) {
		bind_popup = node;
	}
}

void Anim3DEditor::drawOutputNode(Node *node) {
	assert(node && node->inputs.size() == 1);

	drawPinIn(node->inputs[0], "-> output");
}

Node *Anim3DEditor::makeNode() {
	Node *node = nullptr;
	if (first_free) {
		node = first_free;
		first_free = first_free->next;
	}
	else {
		node = arena.make<Node>(arena);
	}

	node->inputs.setAllocator(&arena);

	if (head_node) head_node->prev = node;
	node->next = head_node;
	node->prev = nullptr;
	head_node = node;
	return node;
}

void Anim3DEditor::removeNode(Node *node) {
	Node *next = node->next;
	Node *prev = node->prev;
	if (next) next->prev = prev;
	if (prev) prev->next = next;
	else	  head_node = node->next;
}

void Anim3DEditor::addClip(Animation3D *anim) {
	Node *node = makeNode();
	node->type = Node::Type::Anim;
	//node->id = anim ? (uintptr_t)anim : unique_id++;
	node->id = unique_id++;
	node->clip = anim;
	node->float_value = 1.f;
	node->output = { unique_id++, ed::PinKind::Output, node, Pin::Type::Clip };
}

void Anim3DEditor::addClipNode() {
	Node *node = makeNode();
	node->type = Node::Type::Clip;
	node->id = unique_id++;
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node, Pin::Type::Clip });
	node->output = { unique_id++, ed::PinKind::Output, node };
}

void Anim3DEditor::addSyncedClipNode() {
	Node *node = makeNode();
	node->type = Node::Type::SyncClip;
	node->id = unique_id++;
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node, Pin::Type::Clip });
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node, Pin::Type::Clip });
	node->output = { ed::PinId(unique_id++), ed::PinKind::Output, node };
}

void Anim3DEditor::addBlendNode() {
	Node *node = makeNode();
	node->type = Node::Type::Blend;
	node->id = unique_id++;
	node->float_value = 0.5f;
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node });
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node });
	node->output = { ed::PinId(unique_id++), ed::PinKind::Output, node };
}

void Anim3DEditor::addBlendNode1D() {
	Node *node = makeNode();
	node->type = Node::Type::Blend1D;
	node->id = unique_id++;
	node->float_value = 0.f;
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node });
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node });
	node->inputs.push_back({ unique_id++, ed::PinKind::Input, node });
	node->output = { ed::PinId(unique_id++), ed::PinKind::Output, node };
}

void Anim3DEditor::addOutputNode() {
	Node *node = makeNode();
	node->type = Node::Type::Output;
	node->id = unique_id++;
	node->inputs.push_back({ output_pin_id, ed::PinKind::Input, node });
}

bool Anim3DEditor::buildTreeFromNode(Node *node, Pin *end_pin, ITreeNode *child) {
	ITreeNode *new_node = nullptr;

	switch (node->type) {
	case Node::Type::Anim:
	{
		assert(child);
		if (!node->clip) {
			fail_reason = strcopy("Input's clip was null");
			return false;
		}

		Link *anim_to_clip = findLink(&node->output, end_pin);
		if (!anim_to_clip) {
			fail_reason = strcopy("Nothing connected to input clip");
			return false;
		}
		Pin *clip_pin = anim_to_clip->end;
		Node *clip_node = clip_pin->node;
		
		if (clip_node->type == Node::Type::SyncClip) {
			SyncedClipNode *clip = (SyncedClipNode *)child;
			if (clip_pin == &clip_node->inputs[0]) {
				clip->clip = node->clip;
				clip->clip->playback_speed = node->float_value;
			}
			else {
				clip->leader_clip = node->clip;
			}
		}
		else {
			assert(clip_node->type == Node::Type::Clip);
			ClipNode *clip = (ClipNode *)child;
			clip->clip = node->clip;
			clip->clip->playback_speed = node->float_value;
		}

		return true;
		break;
	}
	case Node::Type::Clip:
		new_node = tree->arena.make<ClipNode>(*tree);
		break;
	case Node::Type::SyncClip:
		new_node = tree->arena.make<SyncedClipNode>(*tree);
		break;
	case Node::Type::Blend:
	{
		BlendNode *clip = tree->arena.make<BlendNode>(*tree);
		clip->blending_value = node->float_value;
		new_node = clip;
		break;
	}
	case Node::Type::Blend1D:
	{
		BlendNode1D *clip = tree->arena.make<BlendNode1D>(*tree);
		clip->blending_value = node->float_value;
		new_node = clip;
		break;
	}
	}

	if (child) child->input_nodes.emplace_back(new_node);
	else       tree->exit_node = new_node;

	if (new_node) {
		tree->all_nodes.emplace_back(new_node);
	}

	bool is_valid = true;

	if (node->bind_value) {
		if (!tree->bindValue(node->bind_name, new_node)) {
			fail_reason = strfmt(
				"couldn't bind value %s to node of type %s", 
				node->bind_name,
				node_type_to_name[(int)node->type]
			);
			return false;
		}
	}

	for (Pin &pin : node->inputs) {
		Link *link = findLink(nullptr, &pin);
		if (!link) {
			is_valid = false;
			fail_reason = strfmt(
				"Input pin %d is unconnected in node of type %s",
				node->inputs.findIt(&pin) + 1,
				node_type_to_name[(int)node->type]
			);
			break;
		}

		Node *parent = link->start->node;
		if (!buildTreeFromNode(parent, &pin, new_node)) {
			is_valid = false;
			break;
		}
	}

	return is_valid;
}

bool Anim3DEditor::buildTree() {
	assert(tree);
	tree->cleanup();
	tree->mesh = system->getSkinnedMesh();
	fail_reason.destroy();
	
	PushAllocInfo("Anim3DEditor-BuildTree");

	// get output node
	Node *output = head_node;
	while (output) {
		if (output->type == Node::Type::Output) {
			break;
		}
		output = output->next;
	}
	assert(output);

	bool success = buildTreeFromNode(output, &output->inputs[0], nullptr);

	if (!success) {
		tree->cleanup();
	}

	return success;
}

Pin *Anim3DEditor::findPin(ed::PinId id) {
	if (!id) return nullptr;

	for (Node *node = head_node; node; node = node->next) {
		if (node->output.id == id) {
			return &node->output;
		}
		for (Pin &pin : node->inputs) {
			if (pin.id == id) {
				return &pin;
			}
		}
	}

	return nullptr;
}

Link *Anim3DEditor::findLink(Pin *start, Pin *end) {
	if (!start && !end) return nullptr;

	struct Both {
		Pin *start;
		Pin *end;
	};

	using PinPtr = Pin *;
	
	if (start && end) {
		Both both = { start, end };
		return links.findWhen<Both>(
			[](Link &l, size_t, const Both &b) {
				return l.start == b.start && l.end == b.end;
			}, 
			both
		);
	}
	else if (start) {
		return links.findWhen<PinPtr>(
			[](Link &l, size_t, const PinPtr &s) {
				return l.start == s;
			},
			start
		);
	}
	else {
		return links.findWhen<PinPtr>(
			[](Link &l, size_t, const PinPtr &e) {
				return l.end == e;
			},
			end
		);
	}
	// unreachable
	assert(false);
	return nullptr;
}
