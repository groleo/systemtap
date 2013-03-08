/* Simple test program for loc2c code.

   This takes the standard libdwfl switches to select what address space
   you are talking about, same as for e.g. eu-addr2line (see --help).  */

#include <config.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <error.h>
#include <locale.h>
#include <argp.h>
#include <elfutils/libdwfl.h>
#include <elfutils/version.h>
#include <dwarf.h>
#include <obstack.h>
#include <unistd.h>
#include <stdarg.h>
#include "loc2c.h"

#define _(msg) msg

/* Using this structure in the callback (to handle_function) allows us
   to print out the function's address */
struct dwfunc
{
  Dwfl *dwfl;
  Dwarf_Die *cu;
  Dwarf_Addr dwbias;
  char * funcname;
  uintmax_t pc;
  bool matchdebug;
};

static void __attribute__ ((noreturn))
fail (void *arg __attribute__ ((unused)), const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "%s: ", program_invocation_short_name);

  va_start (ap, fmt);
  vfprintf (stderr, _(fmt), ap);
  va_end (ap);

  fprintf (stderr, "\n");

  exit (2);
}

static const Dwarf_Op *
get_location (Dwarf_Addr dwbias, Dwarf_Addr pc, Dwarf_Attribute *loc_attr,
	      size_t *len)
{
  Dwarf_Op *expr;

  switch (dwarf_getlocation_addr (loc_attr, pc - dwbias, &expr, len, 1))
    {
    case 1:			/* Should always happen.  */
      if (*len == 0)
	goto inaccessible;
      break;

    default:			/* Shouldn't happen.  */
    case -1:
      fail (NULL, _("dwarf_getlocation_addr (form %#x): %s"),
	    dwarf_whatform (loc_attr), dwarf_errmsg (-1));
      return NULL;

    case 0:			/* Shouldn't happen.  */
    inaccessible:
      fail (NULL, _("not accessible at this address"));
      return NULL;
    }

  return expr;
}

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

static void
handle_fields (struct obstack *pool,
	       struct location *head, struct location *tail,
	       Dwarf_Addr cubias, Dwarf_Die *vardie, Dwarf_Addr pc,
	       char **fields)
{
  Dwarf_Attribute attr_mem;

  if (dwarf_attr_integrate (vardie, DW_AT_type, &attr_mem) == NULL)
    error (2, 0, _("cannot get type of variable: %s"),
	   dwarf_errmsg (-1));

  bool store = false;
  Dwarf_Die die_mem, *die = vardie;
  while (*fields != NULL)
    {
      if (!strcmp (*fields, "="))
	{
	  store = true;
	  if (fields[1] != NULL)
	    error (2, 0, _("extra fields after ="));
	  break;
	}

      die = dwarf_formref_die (&attr_mem, &die_mem);

      const int typetag = dwarf_tag (die);
      switch (typetag)
	{
	case DW_TAG_typedef:
	case DW_TAG_const_type:
	case DW_TAG_volatile_type:
	  /* Just iterate on the referent type.  */
	  break;

	case DW_TAG_pointer_type:
	  if (**fields == '+')
	    goto subscript;
	  /* A "" field means explicit pointer dereference and we consume it.
	     Otherwise the next field implicitly gets the dereference.  */
	  if (**fields == '\0')
	    ++fields;
	  c_translate_pointer (pool, 1, cubias, die, &tail);
	  break;

	case DW_TAG_array_type:
	  if (**fields == '+')
	    {
	    subscript:;
	      char *endp = *fields + 1;
	      uintmax_t idx = strtoumax (*fields + 1, &endp, 0);
	      if (endp == NULL || endp == *fields || *endp != '\0')
		c_translate_array (pool, 1, cubias, die, &tail,
				   *fields + 1, 0);
	      else
		c_translate_array (pool, 1, cubias, die, &tail,
				   NULL, idx);
	      ++fields;
	    }
	  else
	    error (2, 0, _("bad field for array type: %s"), *fields);
	  break;

	case DW_TAG_structure_type:
	case DW_TAG_union_type:
	  switch (dwarf_child (die, &die_mem))
	    {
	    case 1:		/* No children.  */
	      error (2, 0, _("empty struct %s"),
		     dwarf_diename (die) ?: "<anonymous>");
	      break;
	    case -1:		/* Error.  */
	    default:		/* Shouldn't happen */
	      error (2, 0, _("%s %s: %s"),
		     typetag == DW_TAG_union_type ? "union" : "struct",
		     dwarf_diename (die) ?: "<anonymous>",
		     dwarf_errmsg (-1));
	      break;

	    case 0:
	      break;
	    }
	  while (dwarf_tag (die) != DW_TAG_member
		 || ({ const char *member = dwarf_diename (die);
		       member == NULL || strcmp (member, *fields); }))
	    if (dwarf_siblingof (die, &die_mem) != 0)
	      error (2, 0, _("field name %s not found"), *fields);

	  if (dwarf_attr_integrate (die, DW_AT_data_member_location,
				    &attr_mem) == NULL)
	    {
	      /* Union members don't usually have a location,
		 but just use the containing union's location.  */
	      if (typetag != DW_TAG_union_type)
		error (2, 0, _("no location for field %s: %s"),
		       *fields, dwarf_errmsg (-1));
	    }
	  else
	    {
	      /* We expect a block or a constant.  */
	      size_t locexpr_len = 0;
	      const Dwarf_Op *locexpr;
	      locexpr = get_location (cubias, pc, &attr_mem, &locexpr_len);
	      c_translate_location (pool, NULL, NULL, NULL,
				    1, cubias, pc, &attr_mem,
				    locexpr, locexpr_len,
				    &tail, NULL, NULL);
	    }
	  ++fields;
	  break;

	case DW_TAG_base_type:
	  error (2, 0, _("field %s vs base type %s"),
		 *fields, dwarf_diename (die) ?: "<anonymous type>");
	  break;

	case -1:
	  error (2, 0, _("cannot find type: %s"), dwarf_errmsg (-1));
	  break;

	default:
	  error (2, 0, _("%s: unexpected type tag %#x"),
		 dwarf_diename (die) ?: "<anonymous type>",
		 dwarf_tag (die));
	  break;
	}

      /* Now iterate on the type in DIE's attribute.  */
      if (dwarf_attr_integrate (die, DW_AT_type, &attr_mem) == NULL)
	error (2, 0, _("cannot get type of field: %s"), dwarf_errmsg (-1));
    }

  /* Fetch the type DIE corresponding to the final location to be accessed.
     It must be a base type or a typedef for one.  */

  Dwarf_Die typedie_mem;
  Dwarf_Die *typedie;
  int typetag;
  while (1)
    {
      typedie = dwarf_formref_die (&attr_mem, &typedie_mem);
      if (typedie == NULL)
	error (2, 0, _("cannot get type of field: %s"), dwarf_errmsg (-1));
      typetag = dwarf_tag (typedie);
      if (typetag != DW_TAG_typedef &&
	  typetag != DW_TAG_const_type &&
	  typetag != DW_TAG_volatile_type)
	break;
      if (dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem) == NULL)
	error (2, 0, _("cannot get type of field: %s"), dwarf_errmsg (-1));
    }

  switch (typetag)
    {
    case DW_TAG_base_type:
      if (store)
	c_translate_store (pool, 1, cubias, die, typedie, &tail, "value");
      else
	c_translate_fetch (pool, 1, cubias, die, typedie, &tail, "value");
      break;

    case DW_TAG_pointer_type:
    case DW_TAG_reference_type:
      if (store)
	error (2, 0, _("store not supported for pointer type"));
      c_translate_pointer (pool, 1, cubias, typedie, &tail);
      c_translate_addressof (pool, 1, cubias, die, typedie, &tail, "value");
      break;

    default:
      if (store)
	error (2, 0, _("store supported only for base type"));
      else
	error (2, 0, _("fetch supported only for base type or pointer"));
      break;
    }

  printf ("#define PROBEADDR %#" PRIx64 "ULL\n", pc);

  puts (store
	? "static void set_value(struct pt_regs *regs, intptr_t value)\n{"
	: "static void print_value(struct pt_regs *regs)\n"
	"{\n"
	"  intptr_t value;");

  unsigned int stack_depth;
  bool deref = c_emit_location (stdout, head, 1, &stack_depth);

  obstack_free (pool, NULL);

  printf ("  /* max expression stack depth %u */\n", stack_depth);

  puts (store ? "  return;" :
	"  printk (\" ---> %ld\\n\", (unsigned long) value);\n"
	"  return;");

  if (deref)
    puts ("\n"
	  " deref_fault:\n"
	  "  printk (\" => BAD ACCESS\\n\");");

  puts ("}");
}

static void
handle_variable (Dwarf_Die *lscopes, int lnscopes, int out,
		 Dwarf_Addr cubias, Dwarf_Die *vardie, Dwarf_Addr pc,
		 Dwarf_Op *cfa_ops, char **fields)
{
  struct obstack pool;
  obstack_init (&pool);

  /* Figure out the appropriate frame base for accessing this variable.
   * XXX not handling nested functions
   */
  Dwarf_Attribute fb_attr_mem, *fb_attr = NULL;
  int inner;
  /* We start out walking the "lexical scopes" as returned by
   * as returned by dwarf_getscopes for the address, starting with the
   * 'out' scope that the variable was found in.
   */
  Dwarf_Die *scopes = lscopes;
  int nscopes = lnscopes;
  for (inner = out; inner < nscopes && fb_attr == NULL; ++inner)
    {
      switch (dwarf_tag (&scopes[inner]))
	{
	default:
	  continue;
	case DW_TAG_subprogram:
	case DW_TAG_entry_point:
	  fb_attr = dwarf_attr_integrate (&scopes[inner],
					  DW_AT_frame_base,
					  &fb_attr_mem);
	  break;
	case DW_TAG_inlined_subroutine:
	  /* Unless we already are going through the "pyshical die tree",
	   * we now need to start walking the die tree where this
	   * subroutine is inlined to find the appropriate frame base. */
          if (out != -1)
	    {
	      nscopes = dwarf_getscopes_die (&scopes[inner], &scopes);
	      if (nscopes == -1)
		error (2, 0, _("cannot get die scopes inlined_subroutine: %s"),
		       dwarf_errmsg (-1));
	      inner = 0; // zero is current scope, for look will increase.
	      out = -1;
	    }
	  break;
	}
    }

  struct location *head, *tail = NULL;

  Dwarf_Attribute attr_mem;
  if (dwarf_attr_integrate (vardie, DW_AT_const_value, &attr_mem) != NULL)
    /* There is no location expression, but a constant value instead.  */
    head = tail = c_translate_constant (&pool, &fail, NULL, NULL,
					1, cubias, &attr_mem);
  else
    {
      if (dwarf_attr_integrate (vardie, DW_AT_location, &attr_mem) == NULL)
	error (2, 0, _("cannot get location of variable: %s"),
	       dwarf_errmsg (-1));

      size_t locexpr_len = 0;
      const Dwarf_Op *locexpr = get_location (cubias, pc,
					      &attr_mem, &locexpr_len);

      head = c_translate_location (&pool, &fail, NULL, NULL,
				   1, cubias, pc, &attr_mem,
				   locexpr, locexpr_len,
				   &tail, fb_attr, cfa_ops);
    }

  handle_fields (&pool, head, tail, cubias, vardie, pc, fields);
}

static void
paddr (const char *prefix, Dwarf_Addr addr, Dwfl_Line *line)
{
  const char *src;
  int lineno, linecol;
  if (line != NULL
      && (src = dwfl_lineinfo (line, &addr, &lineno, &linecol,
			       NULL, NULL)) != NULL)
    {
      if (linecol != 0)
	printf ("%s%#" PRIx64 " (%s:%d:%d)",
		prefix, addr, src, lineno, linecol);
      else
	printf ("%s%#" PRIx64 " (%s:%d)",
		prefix, addr, src, lineno);
    }
  else
    printf ("%s%#" PRIx64, prefix, addr);
}

static void
print_type (Dwarf_Die *typedie, char space)
{
  if (typedie == NULL)
    printf ("%c<no type>", space);
  else
    {
      const char *name = dwarf_diename (typedie);
      if (name != NULL)
	printf ("%c%s", space, name);
      else
	{
	  Dwarf_Attribute attr_mem;
	  Dwarf_Die die_mem;
	  Dwarf_Die *die = dwarf_formref_die
	    (dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem), &die_mem);
	  int tag = dwarf_tag (typedie);
	  switch (tag)
	  {
	  case DW_TAG_pointer_type:
	    print_type (die, space);
	    putchar ('*');
	    break;
	  case DW_TAG_array_type:
	    print_type (die, space);
	    printf ("[]");
	    break;
	  case DW_TAG_const_type:
	    print_type (die, space);
	    printf (" const");
	    break;
	  case DW_TAG_volatile_type:
	    print_type (die, space);
	    printf (" volatile");
	    break;
	  default:
	    printf ("%c<unknown %#x>", space, tag);
	    break;
	  }
	}
    }
}

static void
print_vars (unsigned int indent, Dwarf_Die *die)
{
  Dwarf_Die child;
  Dwarf_Attribute attr_mem;
  Dwarf_Die typedie_mem;
  Dwarf_Die *typedie;
  if (dwarf_child (die, &child) == 0)
    do
      switch (dwarf_tag (&child))
	{
	case DW_TAG_variable:
	case DW_TAG_formal_parameter:
	  printf ("%*s%-30s[%6" PRIx64 "]", indent, "",
		  dwarf_diename (&child),
		  (uint64_t) dwarf_dieoffset (&child));
	  typedie = dwarf_formref_die
	    (dwarf_attr_integrate (&child, DW_AT_type, &attr_mem),
	     &typedie_mem);
	  print_type (typedie, '\t');
	  puts ("");
	  break;
	default:
	  break;
	}
    while (dwarf_siblingof (&child, &child) == 0);
}

static int
handle_function (Dwarf_Die *funcdie, void *arg)
{
  struct dwfunc *a = arg;
  Dwarf_Addr entrypc;
  int result;
  int i = 0;
  Dwarf_Addr *bkpts = NULL;
  if (arg == NULL)
    error (2, 0, "Error, dwfl structure empty");
  const char *name = dwarf_diename (funcdie);
  if (name == NULL)
    error (2, 0, "Error, dwarf_diename returned NULL from Dwarf_die");
  if (dwarf_entrypc (funcdie, &entrypc) == 0)
    {
      entrypc += a->dwbias;
      result = dwarf_entry_breakpoints (funcdie, &bkpts);
      if (a->funcname == NULL)
	{
	  printf ("%-35s  %#.16" PRIx64, name, entrypc);
	  /* error check the result is greater than 0 */
	  if (result > 0)
	    {
	      /* the formatting changes as a looka-head if we
		 need different ending whitespace chars the
		 location is a combination of the address +
		 the offset (bias) */
	      for (i = 0; i < result; i++)
		printf (" %#.16" PRIx64 "%s", bkpts[i] + a->dwbias,
			i == result - 1 ? "\n" : "");
	      free (bkpts);
	    }
	}
      else if (a->funcname != NULL && (!strcmp (a->funcname, name)))
	{
	  if (!a->matchdebug)
	    {
	      entrypc += a->dwbias;
	      a->pc = entrypc;
	      free (bkpts);
	      return DWARF_CB_ABORT;
	    }
	  else if (result == 1 && a->matchdebug)
	    {
	      a->pc = (bkpts[0] + a->dwbias);
	      free (bkpts);
	      return DWARF_CB_ABORT;
	    }
	  else if (result > 1 && a->matchdebug)
	    {
	      printf ("Function: %-35s has multiple breakpoints: ", name);
	      for (i = 0; i < result; i++)
		printf (" %#.16" PRIx64 "%s", bkpts[i] + a->dwbias,
			i == result - 1 ? "\n" : "");
	      free (bkpts);
	      return DWARF_CB_ABORT;
	    }
	  else
	    {
	      error (2, 0, "Error, dwarf_entry_breakpoints returned an error( %s )\n",
		     dwarf_errmsg (result));
	    }
	}
    }
  return 0;
}

#define INDENT 4

int
main (int argc, char **argv)
{
  /* We use no threads here which can interfere with handling a stream.  */
  (void) __fsetlocking (stdout, FSETLOCKING_BYCALLER);

  /* Set locale.  */
  (void) setlocale (LC_ALL, "");

  const struct argp_child argp_children[] =
    {
      { .argp = dwfl_standard_argp () },
      { .argp = NULL }
    };
  const struct argp argp =
    {
      .children = argp_children,
      .args_doc = "ADDRESS\n\
FUNCTIONNAME\n\
ADDRESS VARIABLE [FIELD...]\n\
FUNCTIONNAME VARIABLE [FIELD...]\n\
ADDRESS VARIABLE [FIELD...] =",
      .doc = "Process DWARF locations visible with PC at ADDRESS.\v\
ADDRESS must be in %i format (i.e. \"0x...\").\n\
FUNCTIONNAME must be exact or prepended with '@' character to specify \
 breakpoint entry.\n\
In the first and second form, display the scope entries containing \
 ADDRESS or FUNCTIONNAME, and the variables/parameters visible there, \
 with their type names. Hex numbers within \"[...]\" are DIE offsets.\n\
In the third and fourth form, emit a C code fragment to access a \
 variable. Each FIELD argument is the name of a member of an aggregate \
 type (or pointer to one);\
 \"+INDEX\" to index into an array type (or offset a pointer type);\
 or \"\" (an empty argument) to dereference a pointer type.\n\
In the fifth form, the access is a store rather than a fetch."

    };
  Dwfl *dwfl = NULL;
  int argi;
  (void) argp_parse (&argp, argc, argv, 0, &argi, &dwfl);
  assert (dwfl != NULL);
  struct dwfunc a = { .dwfl = dwfl, .funcname = NULL,
		      .pc = 0, .matchdebug = 0 };
  if (argi == argc)
    {
      printf ("%-35s  %-18s %s\n",
	      "Function", "Address", "Debug Entry Address(s)");
      while ((a.cu = dwfl_nextcu(a.dwfl, a.cu, &a.dwbias)) != NULL)
	dwarf_getfuncs (a.cu, &handle_function, &a.dwfl, 0);
      return 0;
    }

  char *endp;
  uintmax_t pc = strtoumax (argv[argi], &endp, 0);
  if (endp == argv[argi])
    {
      a.funcname = argv[argi];
      /* We need to check and adjust for a possible '@' character */
      if (a.funcname[0] == '@')
	{
	  a.funcname++;
	  a.matchdebug = 1;
	}
      
      while ((a.cu = dwfl_nextcu(a.dwfl, a.cu, &a.dwbias)) != NULL)
	dwarf_getfuncs (a.cu, &handle_function, &a.dwfl, 0);
      /* If a.pc isn't set, that means the function specified wasn't found */
      if (!a.pc)
	error (EXIT_FAILURE, 0, "function: '%s' not found\n", argv[argi]);
      pc = a.pc;
    }

  Dwarf_Addr cubias;
  Dwarf_Die *cudie = dwfl_addrdie (dwfl, pc, &cubias);
  if (cudie == NULL)
    error (EXIT_FAILURE, 0, "dwfl_addrdie: %s", dwfl_errmsg (-1));

  Dwarf_Die *scopes;
  int n = dwarf_getscopes (cudie, pc - cubias, &scopes);
  if (n < 0)
    error (EXIT_FAILURE, 0, "dwarf_getscopes: %s", dwarf_errmsg (-1));
  else if (n == 0)
    error (EXIT_FAILURE, 0, "%#" PRIx64 ": not in any scope\n", pc);

  if (++argi == argc)
    {
      unsigned int indent = 0;
      while (n-- > 0)
	{
	  Dwarf_Die *const die = &scopes[n];

	  indent += INDENT;
	  printf ("%*s[%6" PRIx64 "] %s (%#x)", indent, "",
		  dwarf_dieoffset (die),
		  dwarf_diename (die) ?: "<unnamed>",
		  dwarf_tag (die));

	  Dwarf_Addr lowpc, highpc;
	  if (dwarf_lowpc (die, &lowpc) == 0
	      && dwarf_highpc (die, &highpc) == 0)
	    {
	      lowpc += cubias;
	      highpc += cubias;
	      Dwfl_Line *loline = dwfl_getsrc (dwfl, lowpc);
	      Dwfl_Line *hiline = dwfl_getsrc (dwfl, highpc);
	      paddr (": ", lowpc, loline);
	      if (highpc != lowpc)
		paddr (" .. ", lowpc, hiline == loline ? NULL : hiline);
	    }
	  puts ("");

	  print_vars (indent + INDENT, die);
	}
    }
  else
    {
      char *spec = argv[argi++];

      if (!strcmp (spec, "return"))
	{
	  int i;
	  for (i = 0; i < n; ++i)
	    if (dwarf_tag (&scopes[i]) == DW_TAG_subprogram)
	      {
		const Dwarf_Op *locexpr;
		int locexpr_len = dwfl_module_return_value_location
		  (dwfl_addrmodule (dwfl, pc), &scopes[i], &locexpr);
		if (locexpr_len < 0)
		  error (EXIT_FAILURE, 0,
			 "dwfl_module_return_value_location: %s",
			 dwfl_errmsg (-1));
		struct obstack pool;
		obstack_init (&pool);
		struct location *head, *tail = NULL;
		head = c_translate_location (&pool, &fail, NULL, NULL,
					     1, cubias, pc, NULL,
					     locexpr, locexpr_len,
					     &tail, NULL, NULL);
		handle_fields (&pool, head, tail, cubias, &scopes[i], pc,
			       &argv[argi]);
		free (scopes);
		dwfl_end (dwfl);
		return 0;
	      }
	  error (EXIT_FAILURE, 0, "no subprogram to have a return value!");
	}

      int lineno = 0, colno = 0, shadow = 0;
      char *at = strchr (spec, '@');
      if (at != NULL)
	{
	  *at++ = '\0';
	  if (sscanf (at, "%*[^:]:%i:%i", &lineno, &colno) < 1)
	    lineno = 0;
	}
      else
	{
	  int len;
	  if (sscanf (spec, "%*[^+]%n+%i", &len, &shadow) == 2)
	    spec[len] = '\0';
	}

      Dwarf_Die vardie;
      int out = dwarf_getscopevar (scopes, n, spec, shadow, at, lineno, colno,
				   &vardie);
      if (out == -2)
	error (0, 0, "no match for %s (+%d, %s:%d:%d)",
	       spec, shadow, at, lineno, colno);
      else if (out < 0)
	error (0, 0, "dwarf_getscopevar: %s (+%d, %s:%d:%d): %s",
	       spec, shadow, at, lineno, colno, dwarf_errmsg (-1));
      else
	{
	  Dwarf_Op *cfa_ops = NULL;

	  size_t cfa_nops;
	  Dwarf_Addr bias;
	  Dwfl_Module *module = dwfl_addrmodule (dwfl, pc);
	  if (module != NULL)
	    {
	      // Try debug_frame first, then fall back on eh_frame.
	      Dwarf_CFI *cfi = dwfl_module_dwarf_cfi (module, &bias);
	      if (cfi != NULL)
		{
		  Dwarf_Frame *frame = NULL;
		  if (dwarf_cfi_addrframe (cfi, pc - bias, &frame) == 0)
		    dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops);
		}
	      if (cfa_ops == NULL)
		{
		  cfi = dwfl_module_eh_cfi (module, &bias);
		  if (cfi != NULL)
		    {
		      Dwarf_Frame *frame = NULL;
		      if (dwarf_cfi_addrframe (cfi, pc - bias, &frame) == 0)
			dwarf_frame_cfa (frame, &cfa_ops, &cfa_nops);
		    }
		}
	    }

	  handle_variable (scopes, n, out, cubias, &vardie, pc, cfa_ops,
			   &argv[argi]);
	}
    }

  free (scopes);

  dwfl_end (dwfl);

  return 0;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
