#ifndef _GEF_INDEX_BUFFER_H
#define _GEF_INDEX_BUFFER_H

#include <gef.h>

namespace gef
{
	// FORWARD DECLARATIONS
	class Platform;

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer();

		virtual bool Init(const Platform& platform, const void* indices, const UInt32 num_indices, const UInt32 index_byte_size,const bool read_only = true) = 0;
		virtual void Bind(const Platform& platform) const = 0;
		virtual void Unbind(const Platform& platform) const = 0;
		virtual bool Update(const Platform& platform) = 0;

		inline UInt32 num_indices() const { return num_indices_; }
		inline UInt32 index_byte_size() const { return index_byte_size_; }
		inline void* index_data() { return index_data_; }

		inline void set_num_indices(UInt32 num) { num_indices_ = num; }
		inline void set_index_byte_size(UInt32 size) { index_byte_size_ = size; }
		inline void set_index_data(void *data) { index_data_ = data; }

		static IndexBuffer* Create(Platform& platform, IAllocator *alloc = g_alloc);

	protected:
		IndexBuffer();
		Int32 num_indices_;
		Int32 index_byte_size_;
		void* index_data_;
	};
}

#endif // _GEF_INDEX_BUFFER_H
