#pragma once

#include <maths/transform.h>
#include "utils.h"

enum class AnimSystemType {
	None, Sprite2D, Skeleton2D, Skeleton3D, InverseKinematics, Count
};

constexpr int INVALID_ID = -1;

class AnimSystem {
public:
	virtual ~AnimSystem() = default;
	virtual bool update(float delta_time) = 0;
	virtual void draw() = 0;
	virtual void debugDraw() = 0;

	void saveFile(const char *filename) const { save(CFile(filename, "wb")); }
	virtual void save(FILE *fp) const = 0;

	void readFile(const char *filename) { read(CFile(filename)); }
	virtual void read(FILE *fp) = 0;

	virtual bool checkFormatVersion(FILE *fp) = 0;

	virtual void setAnimation(const char *name) = 0;
	virtual void setAnimation(int id) = 0;

	virtual gef::Transform getTransform() const = 0;
	virtual void setTransform(const gef::Transform &tran) = 0;

	int getCurrentId() const { return cur_animation; }

	AnimSystemType getType() const { assert((int)type); return type; }
	bool is2D() const { assert((int)type); return type < AnimSystemType::Skeleton3D; }

protected:
	int cur_animation = INVALID_ID;
	AnimSystemType type = AnimSystemType::None;
};