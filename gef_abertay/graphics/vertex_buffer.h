#ifndef _GEF_VERTEX_BUFFER_H
#define _GEF_VERTEX_BUFFER_H

#include <gef.h>

namespace gef
{
	class Platform;

	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer();
		virtual bool Init(const Platform& platform, void* vertices, const UInt32 num_vertices, const UInt32 vertex_byte_size, const bool read_only = true) = 0;
		virtual bool Init(const Platform& platform, const void* vertices, const UInt32 num_vertices, const UInt32 vertex_byte_size, const bool read_only = true) = 0;
		virtual bool Update(const Platform& platform) = 0;

		virtual void Bind(const Platform& platform) const = 0;
		virtual void Unbind(const Platform& platform) const = 0;

		inline UInt32 num_vertices() const { return num_vertices_; }
		inline UInt32 vertex_byte_size() const { return vertex_byte_size_; }
		inline void* vertex_data() {return vertex_data_; }

		inline void set_num_vertices(UInt32 num) { num_vertices_ = num; }
		inline void set_vertex_byte_size(UInt32 size) { vertex_byte_size_ = size; }
		inline void set_vertex_data(void *data) { vertex_data_ = data; }

		static VertexBuffer* Create(Platform& platform, IAllocator *alloc = g_alloc);

	protected:
		VertexBuffer();
		UInt32 num_vertices_;
		UInt32 vertex_byte_size_;
		void* vertex_data_;
	};
}

#endif // _GEF_VERTEX_BUFFER_H
