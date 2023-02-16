#ifndef _GEF_SKINNED_MESH_SHADER_DATA_H
#define _GEF_SKINNED_MESH_SHADER_DATA_H

#include <graphics/default_3d_shader_data.h>

namespace gef
{
	class SkinnedMeshShaderData : public Default3DShaderData
	{
	public:
		SkinnedMeshShaderData();

		const gef::Vec<Matrix44>* const bone_matrices() const { return bone_matrices_; }
		
		void set_bone_matrices(const gef::Vec<Matrix44>* const bone_matrices) { bone_matrices_ = bone_matrices; }

	private:
		const gef::Vec<Matrix44>* bone_matrices_;
	};
}


#endif // _GEF_SKINNED_MESH_SHADER_DATA_H
