#pragma once

#include <mutex>
#include <vector>
#include <string>
#include <algorithm>

namespace utils
{
	class memory final
	{
	public:
		class allocator final
		{
		public:
			~allocator();

			void clear();

			void free(void* data);
			void free(const void* data);

			void free_all();

			bool find(const void* data) const;

			template <typename T>
			inline T* allocate()
			{
				return this->allocate_array<T>(1);
			}

			template <typename T>
			inline T* allocate_array(const size_t count = 1)
			{
				return static_cast<T*>(this->allocate(count * sizeof(T)));
			}

			template <typename T>
			T* get()
			{
				return static_cast<T*>(this->allocate(sizeof(T)));
			}

			template <typename T>
			T* get_array(const size_t count)
			{
				return static_cast<T*>(this->allocate(sizeof(T) * count));
			}

			char* duplicate_string(const std::string& string);

		private:
			void* allocate(size_t size);

			mutable std::mutex mutex_;
			std::vector<void*> pool_;
		};

		static void* allocate(size_t length);

		template <typename T>
		static inline T* allocate()
		{
			return allocate_array<T>(1);
		}

		template <typename T>
		static inline T* allocate_array(const size_t count = 1)
		{
			return static_cast<T*>(allocate(count * sizeof(T)));
		}

		static char* duplicate_string(const std::string& string);

		static void free(void* data);
		static void free(const void* data);

		static bool is_set(const void* mem, char chr, size_t length);
		static void* copy(void* destination, const void* source, size_t length);
		static void* set(void* ptr, char chr, size_t length);

		static bool is_bad_read_ptr(const void* ptr);
		static bool is_bad_code_ptr(const void* ptr);
		static bool is_rdata_ptr(void* ptr);

		static allocator* get_allocator();

	private:
		static allocator mem_allocator_;
	};
}
