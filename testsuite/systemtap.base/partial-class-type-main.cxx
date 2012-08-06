#include "partial-class-type.hxx"

int size (int resize)
{
  return (int)Heap::header_size() - resize;
}

int
main (int argc, char **argv)
{
  return size (argc);
}
