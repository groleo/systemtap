#include "partial-class-type.hxx"

size_t
Heap::header_size ()
{
  return 42;
}

Heap::Heap()
{
  _size = header_size () + 32;
  _memory = (char *) malloc (_size);
}

void*
Heap::allocate(size_t size)
{
  _size += size;
  return _memory + header_size();
}
