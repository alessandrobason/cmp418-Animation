#include <maths/vector2.h>

#include <math.h>

#include <maths/matrix33.h>

namespace gef
{
	const Vector2 Vector2::kZero(0.0f, 0.0f);
	const Vector2 Vector2::kOne(1.0f, 1.0f);


	void Vector2::Normalise()
	{
		float length = Length();

		x /= length;
		y /= length;
	}

	float Vector2::LengthSqr() const
	{
		return (x*x + y*y);
	}

	float Vector2::Length() const
	{
		return sqrtf(x*x + y*y);
	}

	Vector2 Vector2::Rotate(float angle)
	{
		Vector2 result;

		result.x = x*cosf(angle) - y*sinf(angle);
		result.y = x*sinf(angle) + y*cosf(angle);

		return result;
	}

	float Vector2::DotProduct(const Vector2& _vec) const
	{
		return x*_vec.x + y*_vec.y;
	}

	Vector2 Vector2::Transform(const gef::Matrix33 &mat) const {
		gef::Vector2 out;

		float z = 1;

		out.x = mat.m[0][0] * x + mat.m[1][0] * y + mat.m[2][0] * z;
		out.y = mat.m[0][1] * x + mat.m[1][1] * y + mat.m[2][1] * z;

		return out;
	}

	Vector2 Vector2::Transform(const Matrix33 &mat, const Vector2 &vec) {
		return vec.Transform(mat);
	}

}