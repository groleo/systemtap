#   include <stdio.h>
#   include <stdlib.h>

#define  towersbase 	 2.39

/* Towers */
#define maxcells 	18
#define stackrange	3
#define true		1
#define false		0

struct element
{
  int discsize;
  int next;
};

/* Towers */
int stack[stackrange + 1];
struct element cellspace[maxcells + 1];
int freelist, movesdone;

/*  Program to Solve the Towers of Hanoi */

void
error (emsg)
     char *emsg;
{
  printf ("Error in Towers: %s\n", emsg);
}

void
makenull (s)
{
  stack[s] = 0;
}

int
get_element ()
{
  int temp;
  if (freelist > 0)
    {
      temp = freelist;
      freelist = cellspace[freelist].next;
    }
  else
    error ("out of space   ");
  return (temp);
}

void
push (i, s)
     int i, s;
{
  int errorfound, localel;
  errorfound = false;
  if (stack[s] > 0)
    if (cellspace[stack[s]].discsize <= i)
      {
	errorfound = true;
	error ("disc size error");
      };
  if (!errorfound)
    {
      localel = get_element ();
      cellspace[localel].next = stack[s];
      stack[s] = localel;
      cellspace[localel].discsize = i;
    }
}

void
init (s, n)
     int s, n;
{
  int discctr;
  makenull (s);
  for (discctr = n; discctr >= 1; discctr--)
    push (discctr, s);
}

int
pop (s)
     int s;
{
  int temp, temp1;
  if (stack[s] > 0)
    {
      temp1 = cellspace[stack[s]].discsize;
      temp = cellspace[stack[s]].next;
      cellspace[stack[s]].next = freelist;
      freelist = stack[s];
      stack[s] = temp;
      return (temp1);
    }
  else
    error ("nothing to pop ");
  return 0;
}

void
move (s1, s2)
     int s1, s2;
{
  push (pop (s1), s2);
  movesdone = movesdone + 1;
}

void
tower (i, j, k)
     int i, j, k;
{
  int other;
  if (k == 1)
    move (i, j);
  else
    {
      other = 6 - i - j;
      tower (i, other, k - 1);
      move (i, j);
      tower (other, j, k - 1);
    }
}


void
towers ()
{
  int i;
  for (i = 1; i <= maxcells; i++)
    cellspace[i].next = i - 1;
  freelist = maxcells;
  init (1, 14);
  makenull (2);
  makenull (3);
  movesdone = 0;
  tower (1, 2, 14);
  if (movesdone != 16383)
    printf (" error in Towers.\n");
}

#ifndef LOOP
#define LOOP 500
#endif

int
main ()
{
  int i;
  for (i= 0; i < LOOP; i++)
    towers();
  return 0;
}

