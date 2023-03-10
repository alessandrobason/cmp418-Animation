#include <graphics/mesh.h>
#include <graphics/primitive.h>
#include <graphics/vertex_buffer.h>
#include <system/platform.h>
#include <system/allocator.h>

namespace gef
{
	Mesh* Mesh::Create(Platform& platform, IAllocator *alloc)
	{
		return alloc->make<Mesh>(platform);
	}


	Mesh::Mesh(Platform& platform) :
	num_primitives_(0),
	primitives_(NULL),
	vertex_buffer_(NULL),
	platform_(platform)
	{
	}

	Mesh::~Mesh()
	{
		ReleasePrimitives();

		if(vertex_buffer_)
		{
			platform_.RemoveVertexBuffer(vertex_buffer_);
			g_alloc->destroy(vertex_buffer_);
		}
	}
	bool Mesh::InitVertexBuffer(Platform& platform, const void* vertices, const UInt32 num_vertices, const UInt32 vertex_byte_size, bool read_only)
	{
		vertex_buffer_ = gef::VertexBuffer::Create(platform);
		bool success = vertex_buffer_->Init(platform, vertices, num_vertices, vertex_byte_size, read_only);
		platform.AddVertexBuffer(vertex_buffer_);

		return success;
	}

	void Mesh::AllocatePrimitives(const UInt32 num_primitives)
	{
		if(primitives_)
			ReleasePrimitives();
		num_primitives_ = num_primitives;
		if(num_primitives_)
		{
			primitives_ = g_alloc->makeArr<Primitive*>(num_primitives_);
			for(UInt32 primitive_index=0;primitive_index<num_primitives_;++primitive_index)
				primitives_[primitive_index] = g_alloc->make<Primitive>(platform_);
		}
	}

	Primitive* Mesh::AllocatePrimitive()
	{
		return g_alloc->make<Primitive>(platform_);
	}

	void Mesh::ReleasePrimitives()
	{
		if(num_primitives_ && primitives_)
		{
			for(UInt32 primitive_index=0;primitive_index<num_primitives_;++primitive_index)
			{
				g_alloc->destroy(primitives_[primitive_index]);
				primitives_[primitive_index] = NULL;
			}
			g_alloc->destroyArr(primitives_);
			primitives_ = NULL;
		}
		num_primitives_ = 0;
	}
}