#include "transform.h"

#include <maths/math_utils.h>

namespace gef
{
	const Transform Transform::kIdentity = { Quaternion::kIdentity, Vector4::kZero, Vector4::kOne  };
	const Transform Transform::kZero = { { 0.f, 0.f, 0.f, 0.f }, Vector4::kZero, Vector4::kZero };

	Transform::Transform()
	{
	}

	Transform::Transform(const Quaternion &quat, const Vector4 &trans, const Vector4 &scale) 
		: rotation_(quat),
		  translation_(trans),
		  scale_(scale)
	{
	}

	Transform::Transform(const Matrix44& matrix)
	{
		Set(matrix);
	}

	const Matrix44 Transform::GetMatrix() const
	{
		Matrix44 result, scale_matrix, rotation_matrix;
	
		scale_matrix.Scale(scale_);
		rotation_matrix.Rotation(rotation_);
		result = scale_matrix * rotation_matrix;
		result.SetTranslation(translation_);

		return result;
	}

	void Transform::Set(const Matrix44& matrix)
	{
		translation_ = matrix.GetTranslation();
		Quaternion rotation;
		rotation_.SetFromMatrix(matrix);
		rotation_.Normalise();
		scale_ = matrix.GetScale();
	}

	Transform Transform::lerp(const gef::Transform &start, const gef::Transform &end, float time) {
		Transform out;
		Vector4 scale(1.0f, 1.0f, 1.0f), translation;
		Quaternion rotation;
		out.set_scale(gef::lerp(start.scale(), end.scale(), time));
		out.set_translation(gef::lerp(start.translation(), end.translation(), time));
		out.set_rotation(Quaternion::slerp(start.rotation(), end.rotation(), time));
		return out;
	}

	void Transform::Linear2TransformBlend(const gef::Transform& start, const gef::Transform& end, const float time)
	{
		Vector4 scale(1.0f, 1.0f, 1.0f), translation;
		Quaternion rotation;
		scale = gef::lerp(start.scale(), end.scale(), time);
		translation = gef::lerp(start.translation(), end.translation(), time);
		rotation.Slerp(start.rotation(), end.rotation(), time);
		set_scale(scale);
		set_rotation(rotation);
		set_translation(translation);
	}

	void Transform::Inverse(const Transform& transform)
	{
		rotation_.Conjugate(transform.rotation());

		Vector4 inv_translation = -transform.translation();

		inv_translation = Quaternion::Rotate(rotation_, inv_translation);
		translation_ = inv_translation;
	}

}