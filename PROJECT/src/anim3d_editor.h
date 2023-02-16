#pragma once

#include <string>

#include <system/ptr.h>
#include <system/vec.h>
#include <external/imgui_node/imgui_node_editor.h>

#include "arena.h"

namespace gef {
	class Platform;
} // namespace gef

class AnimSystem3D;
struct Animation3D;
struct BlendTree;
struct ITreeNode;

namespace ed = ax::NodeEditor;

struct Node;

struct Pin {
	enum class Type : uint8_t {
		None, Skeleton, Clip
	};

	bool canConnect(Pin *other);

	ed::PinId id;
	ed::PinKind kind;
	Node *node = nullptr;
	Type type = Type::Skeleton;
};

struct Node {
	enum class Type : uint8_t {
		Anim, Clip, SyncClip, Blend, Blend1D, Output, Count
	};

	Node(Arena &arena);

	ed::NodeId id;
	gef::Vec<Pin> inputs;
	Pin output;
	Type type = Type::Count;
	gef::Vector2 pos;

	Animation3D *clip = nullptr;
	float float_value = 0.f;

	bool bind_value = false;
	std::string bind_name;

	Node *next = nullptr;
	Node *prev = nullptr;
};

struct Link {
	ed::LinkId id;
	Pin *start;
	Pin *end;
};

template<typename T>
struct Popup {
	Popup(const T &data) : data(data) {}
	operator bool() const { return (bool)data; }
	void reset() { memset(this, 0, sizeof(*this)); }
	T data;
	bool is_open = false;
};

struct Anim3DEditor {
	void init(AnimSystem3D *anim_sys, gef::Platform &plat);
	void cleanup();
	void draw();

	void generate();

	void open() { is_open = true; }
	void close() { is_open = false; }

private:
	void drawPinIn(const Pin &pin, const char *text);
	void drawPinOut(const Pin &pin, const char *text = "out ->");

	void drawClip(Node *node);
	void drawClipNode(Node *node);
	void drawSyncedClipNode(Node *node);
	void drawBlendNode(Node *node);
	void drawBlendNode1D(Node *node);
	void drawOutputNode(Node *node);

	Node *makeNode();
	void removeNode(Node *node);

	void addClip(Animation3D *anim = nullptr);
	void addClipNode();
	void addSyncedClipNode();
	void addBlendNode();
	void addBlendNode1D();
	void addOutputNode();

	bool buildTreeFromNode(Node *node, Pin *end_pin, ITreeNode *child_clip);
	bool buildTree();

	Pin *findPin(ed::PinId id);
	Link *findLink(Pin *start, Pin *end);

	void generateFromNode(ITreeNode *base_node, Node *child, Pin &pin, gef::Vector2 &pos);

	static constexpr uintptr_t output_pin_id = 1;
	uintptr_t unique_id = 2;

	Arena arena;

	AnimSystem3D *system = nullptr;
	gef::Platform *platform = nullptr;
	BlendTree *tree = nullptr;
	bool is_open = false;
	gef::ptr<char> fail_reason;

	ed::EditorContext *ctx = nullptr;
	gef::Vec<Link> links;

	Node *head_node = nullptr;
	Node *first_free = nullptr;

	gef::Vector2 new_node_pos = gef::Vector2::kZero;
	Popup<Node *> clip_popup = nullptr;
	Popup<Int64> new_node_popup = 0;
	Popup<Node *> bind_popup = nullptr;
};