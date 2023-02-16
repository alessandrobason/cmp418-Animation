#ifndef _GEF_VECTOR2_H
#define _GEF_VECTOR2_H

namespace gef
{
	class Matrix33;
class Vector2
{
public:

	constexpr Vector2() = default;
	constexpr Vector2(const float x, const float y);

	const Vector2 operator - (const Vector2& vec) const;
	const Vector2 operator + (const Vector2& vec) const;
	const Vector2 operator * (const Vector2 &vec) const;
	const Vector2 operator / (const Vector2& vec) const;
	Vector2& operator -= (const Vector2& vec);
	Vector2& operator += (const Vector2& _vec);
	Vector2& operator *= (const Vector2& _vec);
	Vector2& operator /= (const Vector2& _vec);
	const Vector2 operator + (const float scalar) const;
	const Vector2 operator - (const float scalar) const;
	const Vector2 operator * (const float scalar) const;
	const Vector2 operator / (const float scalar) const;
	Vector2& operator += (const float scalar);
	Vector2& operator -= (const float scalar);
	Vector2& operator *= (const float scalar);
	Vector2& operator /= (const float scalar);

	bool operator==(const Vector2& other) const;
	bool operator!=(const Vector2& other) const;

	void Normalise();
	float LengthSqr() const;
	float Length() const;
	Vector2 Rotate(float angle);
	float DotProduct(const Vector2& _vec) const;
	Vector2 Transform(const Matrix33 &mat) const;
	static Vector2 Transform(const Matrix33 &mat, const Vector2 &vec);

	float x = 0.f;
	float y = 0.f;

	static const Vector2 kZero;
	static const Vector2 kOne;

};

}

#include "vector2.inl"

#endif // _GEF_VECTOR2_H
