int a(int i) 
{ 
  return i + 1; 
}
int b(int i) 
{
  return i + 2; 
}
int c(int i) 
{ 
  return i + 3; 
}

int main()
{
  int i = c(
            b(
              a(1)));
  return i;
}
