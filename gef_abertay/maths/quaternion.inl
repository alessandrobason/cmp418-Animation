#include "quaternion.h"
namespace gef {

	inline Quaternion::Quaternion() {
	}

	inline Quaternion::Quaternion(float new_x, float new_y, float new_z, float new_w) :
		x(new_x),
		y(new_y),
		z(new_z),
		w(new_w) {
	}

	inline Quaternion::Quaternion(const gef::Vector4 &axis, float angle) {
		float sin_angle = sinf(angle / 2.f);
		x = sin_angle * axis.x();
		y = sin_angle * axis.y();
		z = sin_angle * axis.z();
		w = cosf(angle / 2.f);
	}

	inline float Quaternion::LengthSquared() const {
		return x * x + y * y + z * z + w * w;
	}

	inline float Quaternion::Length() const {
		return sqrtf(LengthSquared());
	}

	inline Quaternion Quaternion::Norm() const {
		gef::Quaternion out = *this;
		float length = out.Length();
		out.x /= length;
		out.y /= length;
		out.z /= length;
		out.w /= length;
		return out;
	}

	inline Quaternion Quaternion::SafeNorm() const {
		gef::Quaternion out = *this;
		if (float length = out.Length()) {
			out.x /= length;
			out.y /= length;
			out.z /= length;
			out.w /= length;
		}
		return out;
	}

	inline const Quaternion Quaternion::operator -() const {
		return Quaternion(-x, -y, -z, -w);
	}

	inline const Quaternion Quaternion::operator *(float scale) const {
		return Quaternion(scale * x, scale * y, scale * z, scale * w);
	}

	inline const Quaternion Quaternion::operator /(float scale) const {
		return Quaternion(x / scale, y / scale, z / scale, w / scale);
	}

	inline const Quaternion Quaternion::operator +(const Quaternion &q) const {
		return Quaternion(q.x + x, q.y + y, q.z + z, q.w + w);
	}

}
