#include <malloc.h>

class Heap
{
  private:
    char *_memory;
    size_t _size;
  public:
    Heap();
    void* allocate  (size_t size);
    static size_t header_size();
};
