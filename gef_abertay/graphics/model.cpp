#include <graphics/model.h>
#include <system/vec.h>
#include <maths/vector4.h>
#include <maths/vector2.h>
#include <graphics/mesh.h>
#include <graphics/primitive.h>
#include <system/platform.h>
#include <system/allocator.h>
#include <graphics/texture.h>


namespace gef
{
	Model::Model() :
		mesh_(NULL)
	{
	}

	Model::~Model()
	{
		Release();
	}

	void Model::Release()
	{
		// todo: use IAllocator interface
		for (auto &tex : textures_) {
			g_alloc->destroy(tex);
		}
		textures_.clear();
		g_alloc->destroy(mesh_);
	}
}