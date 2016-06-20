#ifndef MEMORY_
#define MEMORY_
#include <boost/pool/pool.hpp>
#include <boost/pool/object_pool.hpp>
#include <cstdlib>
void* MALLOC(size_t sz);
void* CALLOC(size_t sz, size_t itemSz);
void FREE(void* p);

struct AllocatorMallocFree: public boost::default_user_allocator_malloc_free {
  static char * malloc(const size_type bytes)
  { return reinterpret_cast<char *>(MALLOC(bytes)); }
  static void free(char * const block)
  { FREE(block); }
};
#endif