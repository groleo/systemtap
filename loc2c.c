// dwarf location-list-to-c translator
// Copyright (C) 2005-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <obstack.h>
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <dwarf.h>
#include <elfutils/libdw.h>
#include <elfutils/version.h>

#include <assert.h>
#include "loc2c.h"

#include "config.h"


#define N_(x) x

/* NB: PR10601 may make one suspect that intptr_t and uintptr_t aren't
   right, for example on a 64-bit kernel targeting a 32-bit userspace
   process.  At least these types are always at least as wide as
   userspace (since 64-bit userspace doesn't run on a 32-bit kernel).
   So as long as deref() and {fetch,store}_register() widen/narrow
   their underlying values to these, there should be no problem. */

#define STACK_TYPE	"intptr_t"  /* Must be the signed type.  */
#define UTYPE		"uintptr_t" /* Must be the unsigned type.  */
#define SFORMAT		"%" PRId64 "L"
#define UFORMAT		"%" PRIu64 "UL"
#define AFORMAT		"%#" PRIx64 "UL"
#define STACKFMT	"s%u"

struct location_context
{
  struct obstack *pool;

  void (*fail) (void *arg, const char *fmt, ...)
    __attribute__ ((noreturn, format (printf, 2, 3)));
  void *fail_arg;
  void (*emit_address) (void *fail_arg, struct obstack *, Dwarf_Addr);

  Dwarf_Attribute *attr;
  Dwarf_Addr dwbias;
  Dwarf_Addr pc;
  Dwarf_Attribute *fb_attr;
  const Dwarf_Op *cfa_ops;
};

struct location
{
  struct location *next;

  struct location_context *context;

  const Dwarf_Op *ops;
  size_t nops;

  Dwarf_Word byte_size;

  enum
    {
      loc_address, loc_register, loc_noncontiguous, loc_unavailable,
      loc_value, loc_constant, loc_implicit_pointer,
      loc_decl, loc_fragment, loc_final
    } type;
  struct location *frame_base;
  union
  {
    struct	      /* loc_address, loc_value, loc_fragment, loc_final */
    {
      const char *declare;	/* Temporary that needs declared.  */
      char *program;		/* C fragment, leaves address in s0.  */
      unsigned int stack_depth;	/* Temporaries "s0..<N>" used by it.  */
      bool used_deref;		/* Program uses "deref" macro.  */
    } address;
    struct			/* loc_register */
    {
      unsigned int regno;
      Dwarf_Word offset;
    } reg;
    struct location *pieces;	/* loc_noncontiguous */
    const void *constant_block;	/* loc_constant */
    struct			/* loc_implicit_pointer */
    {
      struct location *target;
      Dwarf_Sword offset;
    } pointer;
  };
};

/* Select the C type to use in the emitted code to represent slots in the
   DWARF expression stack.  For really proper semantics this should be the
   target address size.  */
static const char *
stack_slot_type (struct location *context __attribute__ ((unused)), bool sign)
{
  return sign ? STACK_TYPE : UTYPE;
}

static struct location *
alloc_location (struct location_context *ctx)
{
  struct location *loc = obstack_alloc (ctx->pool, sizeof *loc);
  loc->context = ctx;
  loc->byte_size = 0;
  loc->frame_base = NULL;
  loc->ops = NULL;
  loc->nops = 0;
  return loc;
}

#define FAIL(loc, fmt, ...) \
  (*(loc)->context->fail) ((loc)->context->fail_arg, fmt, ## __VA_ARGS__)

static void
default_emit_address (void *fail_arg __attribute__ ((unused)),
		      struct obstack *pool, Dwarf_Addr address)
{
  obstack_printf (pool, AFORMAT, address);
}

static struct location_context *
new_context (struct obstack *pool,
	     void (*fail) (void *arg, const char *fmt, ...)
	     __attribute__ ((noreturn, format (printf, 2, 3))),
	     void *fail_arg,
	     void (*emit_address) (void *fail_arg,
				   struct obstack *, Dwarf_Addr),
	     Dwarf_Addr dwbias, Dwarf_Addr pc_address,
	     Dwarf_Attribute *attr, Dwarf_Attribute *fb_attr,
	     const Dwarf_Op *cfa_ops)
{
  struct location_context *ctx = obstack_alloc (pool, sizeof *ctx);
  ctx->pool = pool;
  ctx->fail = fail;
  ctx->fail_arg = fail_arg;
  ctx->emit_address = emit_address ?: &default_emit_address;
  ctx->attr = attr;
  ctx->dwbias = dwbias;
  ctx->pc = pc_address;
  ctx->fb_attr = fb_attr;
  ctx->cfa_ops = cfa_ops;
  return ctx;
}

/* Translate a DW_AT_const_value attribute as if it were a location of
   constant-value flavor.  */

static struct location *
translate_constant (struct location_context *ctx, int indent,
		    Dwarf_Attribute *attr)
{
  indent += 2;

  struct location *loc = obstack_alloc (ctx->pool, sizeof *loc);
  loc->context = ctx;
  loc->next = NULL;
  loc->byte_size = 0;
  loc->frame_base = NULL;
  loc->address.stack_depth = 0;
  loc->address.declare = NULL;
  loc->address.used_deref = false;
  loc->ops = NULL;
  loc->nops = 0;

  switch (dwarf_whatform (attr))
    {
    case DW_FORM_addr:
      {
	Dwarf_Addr addr;
	if (dwarf_formaddr (attr, &addr) != 0)
	  {
            //TRANSLATORS: failure to find an address that was a constant literal number
	    FAIL (loc, N_("cannot get constant address: %s"),
		  dwarf_errmsg (-1));
	    return NULL;
	  }
	loc->type = loc_value;
	obstack_printf (ctx->pool, "%*saddr = ", indent * 2, "");
	(*ctx->emit_address) (ctx->fail_arg, ctx->pool, ctx->dwbias + addr);
	obstack_grow (ctx->pool, ";\n", 3);
	loc->address.program = obstack_finish (ctx->pool);
	break;
      }

    case DW_FORM_block:
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
      {
	Dwarf_Block block;
	if (dwarf_formblock (attr, &block) != 0)
	  {
	    FAIL (loc, N_("cannot get constant block: %s"), dwarf_errmsg (-1));
	    return NULL;
	  }
	loc->type = loc_constant;
	loc->byte_size = block.length;
	loc->constant_block = block.data;
	break;
      }

    case DW_FORM_string:
    case DW_FORM_strp:
      {
	const char *string = dwarf_formstring (attr);
	if (string == NULL)
	  {
	    FAIL (loc, N_("cannot get string constant: %s"), dwarf_errmsg (-1));
	    return NULL;
	  }
	loc->type = loc_constant;
	loc->byte_size = strlen (string) + 1;
	loc->constant_block = string;
	break;
      }

    default:
      {
	Dwarf_Sword value;
	if (dwarf_formsdata (attr, &value) != 0)
	  {
	    FAIL (loc, N_("cannot get constant value: %s"), dwarf_errmsg (-1));
	    return NULL;
	  }
	loc->type = loc_value;
	obstack_printf (ctx->pool, "%*saddr = (" UTYPE ")%#" PRIx64 "ULL;\n",
			indent * 2, "", value);
	obstack_1grow (ctx->pool, '\0');
	loc->address.program = obstack_finish (ctx->pool);
	break;
      }
    }

  return loc;
}

struct location *
c_translate_constant (struct obstack *pool,
		      void (*fail) (void *arg,
				    const char *fmt, ...)
		      __attribute__ ((noreturn,
				      format (printf, 2, 3))),
		      void *fail_arg,
		      void (*emit_address) (void *fail_arg,
					    struct obstack *,
					    Dwarf_Addr),
		      int indent, Dwarf_Addr dwbias, Dwarf_Attribute *attr)
{
  return translate_constant (new_context (pool, fail, fail_arg, emit_address,
					  dwbias, 0, attr, NULL, NULL),
			     indent, attr);
}

#if _ELFUTILS_PREREQ (0, 149)
static struct location *location_from_attr (struct location_context *ctx,
					    int indent, Dwarf_Attribute *attr);
#endif


/* Synthesize a new loc_address using the program on the obstack.  */
static struct location *
new_synthetic_loc (struct location *origin, bool deref)
{
  obstack_1grow (origin->context->pool, '\0');
  char *program = obstack_finish (origin->context->pool);

  struct location *loc = alloc_location (origin->context);
  loc->next = NULL;
  loc->byte_size = 0;
  loc->type = loc_address;
  loc->address.program = program;
  loc->address.stack_depth = 0;
  loc->address.declare = NULL;
  loc->address.used_deref = deref;

  if (origin->type == loc_register)
    {
      loc->ops = origin->ops;
      loc->nops = origin->nops;
    }
  else
    {
      loc->ops = NULL;
      loc->nops = 0;
    }

  return loc;
}


/* Die in the middle of an expression.  */
static struct location *
lose (struct location *loc, const Dwarf_Op *lexpr, size_t len,
      const char *failure, size_t i)
{
  if (lexpr == NULL || i >= len)
    FAIL (loc, "%s", failure);
  else if (i < len)
    FAIL (loc, N_("%s in DWARF expression [%Zu] at %" PRIu64
		  " (%#x: %" PRId64 ", %" PRId64 ")"),
	  failure, i, lexpr[i].offset,
	  lexpr[i].atom, lexpr[i].number, lexpr[i].number2);
  return NULL;
}


/* Translate a (constrained) DWARF expression into C code
   emitted to the obstack POOL.  INDENT is the number of indentation levels.
   ADDRBIAS is the difference between runtime and Dwarf info addresses.
   INPUT is null or an expression to be initially pushed on the stack.
   If NEED_FB is null, fail on DW_OP_fbreg, else set *NEED_FB to true
   and emit "frame_base" for it.  On success, set *MAX_STACK to the number
   of stack slots required.  On failure, set *LOSER to the index in EXPR
   of the operation we could not handle.

   Returns a failure message or null for success.  */

static const char *
translate (struct location_context *ctx, int indent,
	   const Dwarf_Op *expr, const size_t len,
	   struct location *input, bool *need_fb, size_t *loser,
	   struct location *loc)
{
  loc->ops = expr;
  loc->nops = len;

#define DIE(msg) return (*loser = i, N_(msg))

#define emit(fmt, ...) obstack_printf (ctx->pool, fmt, ## __VA_ARGS__)

  unsigned int stack_depth;
  unsigned int max_stack = 0;
  inline void deepen (void)
    {
      if (stack_depth == max_stack)
	++max_stack;
    }

#define POP(var)							      \
    if (stack_depth > 0)						      \
      --stack_depth;							      \
    else if (tos_register != -1)					      \
      fetch_tos_register ();						      \
    else								      \
      goto underflow;							      \
    int var = stack_depth
#define PUSH 		(deepen (), stack_depth++)
#define STACK(idx)  ({int z = (stack_depth - 1 - (idx)); assert (z >= 0); z;})

  /* Don't put stack operations in the arguments to this.  */
#define push(fmt, ...) \
  emit ("%*s" STACKFMT " = " fmt ";\n", indent * 2, "", PUSH, ## __VA_ARGS__)

  int tos_register;
  inline void fetch_tos_register (void)
    {
      deepen ();
      emit ("%*s" STACKFMT " = fetch_register (%d);\n",
	    indent * 2, "", stack_depth, tos_register);
      tos_register = -1;
    }

  bool tos_value;
  Dwarf_Block implicit_value;
  bool used_deref;
  const Dwarf_Op *implicit_pointer;

  /* Initialize our state for handling each new piece.  */
  inline void reset ()
    {
      stack_depth = 0;
      tos_register = -1;
      tos_value = false;
      implicit_value.data = NULL;
      implicit_pointer = NULL;
      used_deref = false;

      if (input != NULL)
	switch (input->type)
	  {
	  case loc_address:
	    push ("addr");
	    break;

	  case loc_value:
	    push ("addr");
	    tos_value = true;
	    break;

	  case loc_register:
	    tos_register = input->reg.regno;
	    break;

	  default:
	    abort ();
	    break;
	  }
    }

  size_t i;
  inline const char *finish (struct location *piece)
    {
      if (piece->nops == 0)
	{
	  assert (stack_depth == 0);
	  assert (tos_register == -1);
	  assert (obstack_object_size (ctx->pool) == 0);
	  piece->type = loc_unavailable;
	}
      else if (stack_depth >= 1)
	{
	  /* The top of stack has our value.
	     Other stack slots left don't matter.  */
	  obstack_1grow (ctx->pool, '\0');
	  char *program = obstack_finish (ctx->pool);
	  if (implicit_pointer != NULL)
	    {
	      piece->type = loc_implicit_pointer;
	      piece->pointer.offset = implicit_pointer->number2;
#if !_ELFUTILS_PREREQ (0, 149)
	      /* Then how did we get here?  */
	      abort ();
#else
	      Dwarf_Attribute target;
	      if (dwarf_getlocation_implicit_pointer (ctx->attr,
						      implicit_pointer,
						      &target) != 0)
		DIE ("invalid implicit pointer");
	      switch (dwarf_whatattr (&target))
		{
		case DW_AT_const_value:
		  piece->pointer.target = translate_constant (ctx, indent,
							      &target);
		  break;
		case DW_AT_location:
		  piece->pointer.target = location_from_attr (ctx, indent,
							      &target);
		  break;
		default:
		  DIE ("unexpected implicit pointer attribute!");
		  break;
		}
#endif
	    }
	  else if (implicit_value.data == NULL)
	    {
	      piece->type = tos_value ? loc_value : loc_address;
	      piece->address.declare = NULL;
	      piece->address.program = program;
	      piece->address.stack_depth = max_stack;
	      piece->address.used_deref = used_deref;
	    }
	  else
	    {
	      piece->type = loc_constant;
	      piece->byte_size = implicit_value.length;
	      piece->constant_block = implicit_value.data;
	    }
	}
      else if (tos_register == -1)
	DIE ("stack underflow");
      else if (obstack_object_size (ctx->pool) != 0)
	DIE ("register value must stand alone in location expression");
      else
	{
	  piece->type = loc_register;
	  piece->reg.regno = tos_register;
	  piece->reg.offset = 0;
	}
      return NULL;
    }

  reset ();
  struct location *pieces = NULL, **tailpiece = &pieces;
  size_t piece_expr_start = 0;
  Dwarf_Word piece_total_bytes = 0;
  for (i = 0; i < len; ++i)
    {
      unsigned int reg;
      uint_fast8_t sp;
      Dwarf_Word value;

      inline bool more_ops (void)
      {
	return (expr[i].atom != DW_OP_nop
		&& expr[i].atom != DW_OP_piece
		&& expr[i].atom != DW_OP_bit_piece);
      }

      if (tos_value && more_ops ())
	DIE ("operations follow DW_OP_stack_value");

      if (implicit_value.data != NULL && more_ops ())
	DIE ("operations follow DW_OP_implicit_value");

      if (implicit_pointer != NULL && more_ops ())
	DIE ("operations follow DW_OP_GNU_implicit_pointer");

      switch (expr[i].atom)
	{
	  /* Basic stack operations.  */
	case DW_OP_nop:
	  break;

	case DW_OP_dup:
	  if (stack_depth < 1)
	    goto underflow;
	  else
	    {
	      unsigned int tos = STACK (0);
	      push (STACKFMT, tos);
	    }
	  break;

	case DW_OP_drop:
	  {
	    POP (ignore);
	    emit ("%*s/* drop " STACKFMT "*/\n", indent * 2, "", ignore);
	    break;
	  }

	case DW_OP_pick:
	  sp = expr[i].number;
	op_pick:
	  if (sp >= stack_depth)
	    goto underflow;
	  sp = STACK (sp);
	  push (STACKFMT, sp);
	  break;

	case DW_OP_over:
	  sp = 1;
	  goto op_pick;

	case DW_OP_swap:
	  if (stack_depth < 2)
	    goto underflow;
	  deepen ();		/* Use a temporary slot.  */
	  emit ("%*s"
		STACKFMT " = " STACKFMT ", "
		STACKFMT " = " STACKFMT ", "
		STACKFMT " = " STACKFMT ";\n",
		indent * 2, "",
		STACK (-1), STACK (0),
		STACK (0), STACK (1),
		STACK (1), STACK (-1));
	  break;

	case DW_OP_rot:
	  if (stack_depth < 3)
	    goto underflow;
	  deepen ();		/* Use a temporary slot.  */
	  emit ("%*s"
		STACKFMT " = " STACKFMT ", "
		STACKFMT " = " STACKFMT ", "
		STACKFMT " = " STACKFMT ", "
		STACKFMT " = " STACKFMT ";\n",
		indent * 2, "",
		STACK (-1), STACK (0),
		STACK (0), STACK (1),
		STACK (1), STACK (2),
		STACK (2), STACK (-1));
	  break;


	  /* Control flow operations.  */
	case DW_OP_skip:
	  {
	    Dwarf_Off target = expr[i].offset + 3 + expr[i].number;
	    while (i + 1 < len && expr[i + 1].offset < target)
	      ++i;
	    if (expr[i + 1].offset != target)
	      DIE ("invalid skip target");
	    break;
	  }

	case DW_OP_bra:
	  DIE ("conditional branches not supported");
	  break;


	  /* Memory access.  */
	case DW_OP_deref:
	  {
	    POP (addr);
	    push ("deref (sizeof (void *), " STACKFMT ")", addr);
	    used_deref = true;
	  }
	  break;

	case DW_OP_deref_size:
	  {
	    POP (addr);
	    push ("deref (" UFORMAT ", " STACKFMT ")",
		  expr[i].number, addr);
	    used_deref = true;
	  }
	  break;

	case DW_OP_xderef:
	  {
	    POP (addr);
	    POP (as);
	    push ("xderef (sizeof (void *), " STACKFMT ", " STACKFMT ")",
		  addr, as);
	    used_deref = true;
	  }
	  break;

	case DW_OP_xderef_size:
	  {
	    POP (addr);
	    POP (as);
	    push ("xderef (" UFORMAT ", " STACKFMT ", " STACKFMT ")",
		  expr[i].number, addr, as);
	    used_deref = true;
	  }
	  break;

	  /* Constant-value operations.  */

	case DW_OP_addr:
	  emit ("%*s" STACKFMT " = ", indent * 2, "", PUSH);
	  (*ctx->emit_address) (ctx->fail_arg, ctx->pool,
				ctx->dwbias + expr[i].number);
	  emit (";\n");
	  break;

	case DW_OP_lit0 ... DW_OP_lit31:
	  value = expr[i].atom - DW_OP_lit0;
	  goto op_const;

	case DW_OP_const1u:
	case DW_OP_const1s:
	case DW_OP_const2u:
	case DW_OP_const2s:
	case DW_OP_const4u:
	case DW_OP_const4s:
	case DW_OP_const8u:
	case DW_OP_const8s:
	case DW_OP_constu:
	case DW_OP_consts:
	  value = expr[i].number;
	op_const:
	  push (SFORMAT, value);
	  break;

	  /* Arithmetic operations.  */
#define UNOP(dw_op, c_op)						      \
	case DW_OP_##dw_op:						      \
	  {								      \
	    POP (tos);							      \
	    push ("%s (" STACKFMT ")", #c_op, tos);			      \
	  }								      \
	  break
#define BINOP(dw_op, c_op)						      \
	case DW_OP_##dw_op:						      \
	  {								      \
	    POP (b);							      \
	    POP (a);							      \
	    push (STACKFMT " %s " STACKFMT, a, #c_op, b);		      \
	  }								      \
	  break

	  UNOP (abs, op_abs);
	  BINOP (and, &);
	  BINOP (minus, -);
	  BINOP (mul, *);
	  UNOP (neg, -);
	  UNOP (not, ~);
	  BINOP (or, |);
	  BINOP (plus, +);
	  BINOP (shl, <<);
	  BINOP (shr, >>);
	  BINOP (xor, ^);

	  /* Comparisons are binary operators too.  */
	  BINOP (le, <=);
	  BINOP (ge, >=);
	  BINOP (eq, ==);
	  BINOP (lt, <);
	  BINOP (gt, >);
	  BINOP (ne, !=);

#undef	UNOP
#undef	BINOP

	case DW_OP_shra:
	  {
	    POP (b);
	    POP (a);
	    push ("(%s) " STACKFMT " >> (%s)" STACKFMT,
		  stack_slot_type (loc, true), a,
		  stack_slot_type (loc, true), b);
	    break;
	  }

	case DW_OP_div:
	  {
	    POP (b);
	    POP (a);
	    push ("dwarf_div_op((%s) " STACKFMT ", (%s) " STACKFMT ")",
		  stack_slot_type (loc, true), a,
		  stack_slot_type (loc, true), b);
	    used_deref = true;
	    break;
	  }

	case DW_OP_mod:
	  {
	    POP (b);
	    POP (a);
	    push ("dwarf_mod_op((%s) " STACKFMT ", (%s) " STACKFMT ")",
		  stack_slot_type (loc, false), a,
		  stack_slot_type (loc, false), b);
	    used_deref = true;
	    break;
	  }

	case DW_OP_plus_uconst:
	  {
	    POP (x);
	    push (STACKFMT " + " UFORMAT, x, expr[i].number);
	  }
	  break;


	  /* Register-relative addressing.  */
	case DW_OP_breg0 ... DW_OP_breg31:
	  reg = expr[i].atom - DW_OP_breg0;
	  value = expr[i].number;
	  goto op_breg;

	case DW_OP_bregx:
	  reg = expr[i].number;
	  value = expr[i].number2;
	op_breg:
	  push ("fetch_register (%u) + " SFORMAT, reg, value);
	  break;

	case DW_OP_fbreg:
	  if (need_fb == NULL)
	    DIE ("DW_OP_fbreg from DW_AT_frame_base");
	  *need_fb = true;
	  push ("frame_base + " SFORMAT, expr[i].number);
	  break;

	  /* Direct register contents.  */
	case DW_OP_reg0 ... DW_OP_reg31:
	  reg = expr[i].atom - DW_OP_reg0;
	  goto op_reg;

	case DW_OP_regx:
	  reg = expr[i].number;
	op_reg:
	  tos_register = reg;
	  break;

	  /* Special magic.  */
	case DW_OP_piece:
	  if (stack_depth > 1)
	    /* If this ever happens we could copy the program.  */
	    DIE ("DW_OP_piece left multiple values on stack");
	  else
	    {
	      /* The obstack has a pending program for loc_address,
		 so we must finish that piece off before we can
		 allocate again.  */
	      struct location temp_piece =
		{
		  .context = loc->context,
		  .frame_base = NULL,
		  .ops = &expr[piece_expr_start],
		  .nops = i - piece_expr_start,
		};
	      const char *failure = finish (&temp_piece);
	      if (failure != NULL)
		return failure;

	      struct location *piece = obstack_alloc (ctx->pool, sizeof *piece);
	      *piece = temp_piece;

	      piece_expr_start = i + 1;

	      piece_total_bytes += piece->byte_size = expr[i].number;

	      *tailpiece = piece;
	      tailpiece = &piece->next;
	      piece->next = NULL;

	      /* Reset default conditions for handling the next piece.  */
	      reset ();
	    }
	  break;

	case DW_OP_stack_value:
	  if (stack_depth > 1)
	    DIE ("DW_OP_stack_value left multiple values on stack");
	  else
	    {
	      /* Fetch a register to top of stack, or check for underflow.
		 Then mark the TOS as being a value.  */
	      POP (tos);
	      assert (tos == 0);
	      PUSH;
	      tos_value = true;
	    }
	  break;

	case DW_OP_implicit_value:
	  if (ctx->attr == NULL)
	    DIE ("DW_OP_implicit_value used in invalid context"
		 " (no DWARF attribute, ABI return value location?)");

	  /* It's supposed to appear by itself, except for DW_OP_piece.  */
	  if (stack_depth != 0)
	    DIE ("DW_OP_implicit_value follows stack operations");

#if _ELFUTILS_PREREQ (0, 143)
	  if (dwarf_getlocation_implicit_value (ctx->attr,
						(Dwarf_Op *) &expr[i],
						&implicit_value) != 0)
	    DIE ("dwarf_getlocation_implicit_value failed");

	  /* Fake top of stack: implicit_value being set marks it.  */
	  PUSH;
	  break;
#endif

	  DIE ("DW_OP_implicit_value not supported");
	  break;

#if _ELFUTILS_PREREQ (0, 149)
	case DW_OP_GNU_implicit_pointer:
	  implicit_pointer = &expr[i];
	  /* Fake top of stack: implicit_pointer being set marks it.  */
	  PUSH;
	  break;
#endif

	case DW_OP_call_frame_cfa:
	  // We pick this out when processing DW_AT_frame_base in
	  // so it really shouldn't turn up here.
	  if (need_fb == NULL)
	    DIE ("DW_OP_call_frame_cfa while processing frame base");
	  else
	    DIE ("DW_OP_call_frame_cfa not expected outside DW_AT_frame_base");
	  break;

	case DW_OP_push_object_address:
	  DIE ("XXX DW_OP_push_object_address");
	  break;

	default:
	  DIE ("unrecognized operation");
	  break;
	}
    }

  if (pieces == NULL)
    return finish (loc);

  if (piece_expr_start != i)
    DIE ("extra operations after last DW_OP_piece");

  loc->type = loc_noncontiguous;
  loc->pieces = pieces;
  loc->byte_size = piece_total_bytes;

  return NULL;

 underflow:
  DIE ("stack underflow");

#undef emit
#undef push
#undef PUSH
#undef POP
#undef STACK
#undef DIE
}

/* Translate a location starting from an address or nothing.  */
static struct location *
location_from_address (struct location_context *ctx, int indent,
		       const Dwarf_Op *expr, size_t len,
		       struct location **input)
{
  struct location *loc = obstack_alloc (ctx->pool, sizeof *loc);
  loc->context = ctx;
  loc->byte_size = 0;
  loc->frame_base = NULL;
  loc->ops = NULL;
  loc->nops = 0;

  bool need_fb = false;
  size_t loser;
  const char *failure = translate (ctx, indent + 1, expr, len,
				   *input, &need_fb, &loser, loc);
  if (failure != NULL)
    return lose (loc, expr, len, failure, loser);

  loc->next = NULL;
  if (need_fb)
    {
      /* The main expression uses DW_OP_fbreg, so we need to compute
	 the DW_AT_frame_base attribute expression's value first.  */

      if (ctx->fb_attr == NULL)
	FAIL (loc, N_("required DW_AT_frame_base attribute not supplied"));

      Dwarf_Op *fb_expr;
      size_t fb_len;
      switch (dwarf_getlocation_addr (ctx->fb_attr, ctx->pc,
				      &fb_expr, &fb_len, 1))
	{
	case 1:			/* Should always happen.  */
	  if (fb_len == 0)
	    goto fb_inaccessible;
	  break;

	default:		/* Shouldn't happen.  */
	case -1:
	  FAIL (loc, N_("dwarf_getlocation_addr (form %#x): %s"),
		dwarf_whatform (ctx->fb_attr), dwarf_errmsg (-1));
	  return NULL;

	case 0:			/* Shouldn't happen.  */
	fb_inaccessible:
	  FAIL (loc, N_("DW_AT_frame_base not accessible at this address"));
	  return NULL;
	}

      // If it is DW_OP_call_frame_cfa then get cfi cfa ops.
      const Dwarf_Op * fb_ops;
      if (fb_len == 1 && fb_expr[0].atom == DW_OP_call_frame_cfa)
	{
	  if (ctx->cfa_ops == NULL)
	    FAIL (loc, N_("No cfa_ops supplied, but needed by DW_OP_call_frame_cfa"));
	  fb_ops = ctx->cfa_ops;
	}
      else
	fb_ops = fb_expr;

      loc->frame_base = alloc_location (ctx);
      failure = translate (ctx, indent + 1, fb_ops, fb_len, NULL,
			   NULL, &loser, loc->frame_base);
      if (failure != NULL)
	return lose (loc, fb_ops, fb_len, failure, loser);
    }

  if (*input != NULL)
    (*input)->next = loc;
  *input = loc;

  return loc;
}

#if _ELFUTILS_PREREQ (0, 149)
static struct location *
location_from_attr (struct location_context *ctx, int indent,
		    Dwarf_Attribute *attr)
{
  Dwarf_Op *expr;
  size_t len;
  switch (dwarf_getlocation_addr (attr, ctx->pc, &expr, &len, 1))
    {
    case 1:			/* Should always happen.  */
      if (len > 0)
	break;
      /* Fall through.  */

    case 0:			/* Shouldn't happen.  */
      (*ctx->fail) (ctx->fail_arg, N_("not accessible at this address (%#" PRIx64 ")"), ctx->pc);
      return NULL;

    default:			/* Shouldn't happen.  */
    case -1:
      (*ctx->fail) (ctx->fail_arg, "dwarf_getlocation_addr: %s",
		    dwarf_errmsg (-1));
      return NULL;
    }

  struct location *input = NULL;
  return location_from_address (ctx, indent, expr, len, &input);
}
#endif

static struct location *
translate_offset (int indent, const Dwarf_Op *expr, size_t len, size_t i,
		  struct location *head, struct location **input,
		  Dwarf_Word offset)
{
  struct location_context *const ctx = (*input)->context;

#define DIE(msg) return lose (*input, expr, len, N_(msg), i)

  while ((*input)->type == loc_noncontiguous)
    {
      /* We are starting from a noncontiguous object (DW_OP_piece).
	 Find the piece we want.  */

      struct location *piece = (*input)->pieces;
      while (piece != NULL && offset >= piece->byte_size)
	{
	  offset -= piece->byte_size;
	  piece = piece->next;
	}
      if (piece == NULL)
	DIE ("offset outside available pieces");

      assert ((*input)->next == NULL);
      (*input)->next = piece;
      *input = piece;
    }

  switch ((*input)->type)
    {
    case loc_address:
      {
	/* The piece we want is actually in memory.  Use the same
	   program to compute the address from the preceding input.  */

	struct location *loc = obstack_alloc (ctx->pool, sizeof *loc);
	*loc = **input;
	if (head == NULL)
	  head = loc;
	(*input)->next = loc;
	if (offset == 0)
	  {
	    /* The piece addresses exactly where we want to go.  */
	    loc->next = NULL;
	    *input = loc;
	  }
	else
	  {
	    /* Add a second fragment to offset the piece address.  */
	    obstack_printf (ctx->pool, "%*saddr += " SFORMAT "\n",
			    indent * 2, "", offset);
	    *input = loc->next = new_synthetic_loc (*input, false);
	  }

	/* That's all she wrote.  */
	return head;
      }

    case loc_register:
      /* This piece (or the whole struct) fits in a register.  */
      (*input)->reg.offset += offset;
      return head ?: *input;

    case loc_constant:
      /* This piece has a constant offset.  */
      if (offset >= (*input)->byte_size)
	DIE ("offset outside available constant block");
      (*input)->constant_block += offset;
      (*input)->byte_size -= offset;
      return head ?: *input;

    case loc_implicit_pointer:
      /* This piece is an implicit pointer.  */
      (*input)->pointer.offset += offset;
      return head ?: *input;

    case loc_unavailable:
      /* Let it be diagnosed later.  */
      return head ?: *input;

    case loc_value:
      /* The piece we want is part of a computed offset.
	 If it's the whole thing, we are done.  */
      if (offset == 0)
	return head ?: *input;
      DIE ("XXX extract partial rematerialized value");
      break;

    default:
      abort ();
    }

#undef DIE
}


/* Translate a location starting from a non-address "on the top of the
   stack".  The *INPUT location is a register name or noncontiguous
   object specification, and this expression wants to find the "address"
   of an object (or the actual value) relative to that "address".  */

static struct location *
location_relative (struct location_context *ctx, int indent,
		   const Dwarf_Op *expr, size_t len,
		   struct location **input)
{
  Dwarf_Sword *stack = NULL;
  unsigned int stack_depth = 0, max_stack = 0;
  inline void deepen (void)
    {
      if (stack_depth == max_stack)
	{
	  ++max_stack;
	  obstack_blank (ctx->pool, sizeof stack[0]);
	  stack = (void *) obstack_base (ctx->pool);
	}
    }

#define POP(var)							      \
    if (stack_depth > 0)						      \
      --stack_depth;							      \
    else								      \
      goto underflow;							      \
    int var = stack_depth
#define PUSH 		(deepen (), stack_depth++)
#define STACK(idx)	(stack_depth - 1 - (idx))
#define STACKWORD(idx)	stack[STACK (idx)]

  /* Don't put stack operations in the arguments to this.  */
#define push(value) (stack[PUSH] = (value))

  const char *failure = NULL;
#define DIE(msg) do { failure = N_(msg); goto fail; } while (0)

  struct location *head = NULL;
  size_t i;
  for (i = 0; i < len; ++i)
    {
      uint_fast8_t sp;
      Dwarf_Word value;

      switch (expr[i].atom)
	{
	  /* Basic stack operations.  */
	case DW_OP_nop:
	  break;

	case DW_OP_dup:
	  if (stack_depth < 1)
	    goto underflow;
	  else
	    {
	      unsigned int tos = STACK (0);
	      push (stack[tos]);
	    }
	  break;

	case DW_OP_drop:
	  if (stack_depth > 0)
	    --stack_depth;
	  else if (*input != NULL)
	    /* Mark that we have consumed the input.  */
	    *input = NULL;
	  else
	    /* Hits if cleared above, or if we had no input at all.  */
	    goto underflow;
	  break;

	case DW_OP_pick:
	  sp = expr[i].number;
	op_pick:
	  if (sp >= stack_depth)
	    goto underflow;
	  sp = STACK (sp);
	  push (stack[sp]);
	  break;

	case DW_OP_over:
	  sp = 1;
	  goto op_pick;

	case DW_OP_swap:
	  if (stack_depth < 2)
	    goto underflow;
	  deepen ();		/* Use a temporary slot.  */
	  STACKWORD (-1) = STACKWORD (0);
	  STACKWORD (0) = STACKWORD (1);
	  STACKWORD (1) = STACKWORD (-1);
	  break;

	case DW_OP_rot:
	  if (stack_depth < 3)
	    goto underflow;
	  deepen ();		/* Use a temporary slot.  */
	  STACKWORD (-1) = STACKWORD (0);
	  STACKWORD (0) = STACKWORD (1);
	  STACKWORD (2) = STACKWORD (2);
	  STACKWORD (2) = STACKWORD (-1);
	  break;


	  /* Control flow operations.  */
	case DW_OP_bra:
	  {
	    POP (taken);
	    if (stack[taken] == 0)
	      break;
	  }
	  /*FALLTHROUGH*/

	case DW_OP_skip:
	  {
	    Dwarf_Off target = expr[i].offset + 3 + expr[i].number;
	    while (i + 1 < len && expr[i + 1].offset < target)
	      ++i;
	    if (expr[i + 1].offset != target)
	      DIE ("invalid skip target");
	    break;
	  }

	  /* Memory access.  */
	case DW_OP_deref:
	case DW_OP_deref_size:
	case DW_OP_xderef:
	case DW_OP_xderef_size:

	  /* Register-relative addressing.  */
	case DW_OP_breg0 ... DW_OP_breg31:
	case DW_OP_bregx:
	case DW_OP_fbreg:

	  /* This started from a register, but now it's following a pointer.
	     So we can do the translation starting from address here.  */
	  return location_from_address (ctx, indent, expr, len, input);


	  /* Constant-value operations.  */
	case DW_OP_addr:
	  DIE ("static calculation depends on load-time address");
	  push (ctx->dwbias + expr[i].number);
	  break;

	case DW_OP_lit0 ... DW_OP_lit31:
	  value = expr[i].atom - DW_OP_lit0;
	  goto op_const;

	case DW_OP_const1u:
	case DW_OP_const1s:
	case DW_OP_const2u:
	case DW_OP_const2s:
	case DW_OP_const4u:
	case DW_OP_const4s:
	case DW_OP_const8u:
	case DW_OP_const8s:
	case DW_OP_constu:
	case DW_OP_consts:
	  value = expr[i].number;
	op_const:
	  push (value);
	  break;

	  /* Arithmetic operations.  */
#define UNOP(dw_op, c_op)						      \
	case DW_OP_##dw_op:						      \
	  {								      \
	    POP (tos);							      \
	    push (c_op (stack[tos])); 					      \
	  }								      \
	  break
#define BINOP(dw_op, c_op)						      \
	case DW_OP_##dw_op:						      \
	  {								      \
	    POP (b);							      \
	    POP (a);							      \
 	    push (stack[a] c_op stack[b]);				      \
	  }								      \
	  break

#define op_abs(x) (x < 0 ? -x : x)
	  UNOP (abs, op_abs);
	  BINOP (and, &);
	  BINOP (div, /);
	  BINOP (mod, %);
	  BINOP (mul, *);
	  UNOP (neg, -);
	  UNOP (not, ~);
	  BINOP (or, |);
	  BINOP (shl, <<);
	  BINOP (shra, >>);
	  BINOP (xor, ^);

	  /* Comparisons are binary operators too.  */
	  BINOP (le, <=);
	  BINOP (ge, >=);
	  BINOP (eq, ==);
	  BINOP (lt, <);
	  BINOP (gt, >);
	  BINOP (ne, !=);

#undef	UNOP
#undef	BINOP

	case DW_OP_shr:
	  {
	    POP (b);
	    POP (a);
	    push ((Dwarf_Word) stack[a] >> (Dwarf_Word) stack[b]);
	    break;
	  }

	  /* Simple addition we may be able to handle relative to
	     the starting register name.  */
	case DW_OP_minus:
	  {
	    POP (tos);
	    value = -stack[tos];
	    goto plus;
	  }
	case DW_OP_plus:
	  {
	    POP (tos);
	    value = stack[tos];
	    goto plus;
	  }
	case DW_OP_plus_uconst:
	  value = expr[i].number;
	plus:
	  if (stack_depth > 0)
	    {
	      /* It's just private diddling after all.  */
	      POP (a);
	      push (stack[a] + value);
	      break;
	    }
	  if (*input == NULL)
	    goto underflow;

	  /* This is the primary real-world case: the expression takes
	     the input address and adds a constant offset.  */

	  head = translate_offset (indent, expr, len, i, head, input, value);
	  if (head != NULL && i + 1 < len)
	    {
	      if ((*input)->type != loc_address)
		DIE ("too much computation for non-address location");

	      /* This expression keeps going, but further
		 computations now have an address to start with.
		 So we can punt to the address computation generator.  */
	      struct location *loc = location_from_address
		(ctx, indent, &expr[i + 1], len - i - 1, input);
	      if (loc == NULL)
		head = NULL;
	    }
	  return head;

	  /* Direct register contents.  */
	case DW_OP_reg0 ... DW_OP_reg31:
	case DW_OP_regx:
	  DIE ("register");
	  break;

	  /* Special magic.  */
	case DW_OP_piece:
	  DIE ("DW_OP_piece");
	  break;

	case DW_OP_push_object_address:
	  DIE ("XXX DW_OP_push_object_address");
	  break;

	default:
	  DIE ("unrecognized operation");
	  break;
	}
    }

  if (stack_depth > 1)
    DIE ("multiple values left on stack");

  if (stack_depth > 0)		/* stack_depth == 1 */
    {
      if (*input != NULL)
	DIE ("multiple values left on stack");

      /* Could handle this if it ever actually happened.  */
      DIE ("relative expression computed constant");
    }

  return head;

 underflow:
  if (*input == NULL)
    DIE ("stack underflow");
  else
    DIE ("cannot handle location expression");

 fail:
  return lose (*input, expr, len, failure, i);
}

/* Translate a C fragment for the location expression, using *INPUT
   as the starting location, begin from scratch if *INPUT is null.
   If DW_OP_fbreg is used, it may have a subfragment computing from
   the FB_ATTR location expression.

   On errors, call FAIL and never return.  On success, return the
   first fragment created, which is also chained onto (*INPUT)->next.
   *INPUT is then updated with the new tail of that chain.  */

struct location *
c_translate_location (struct obstack *pool,
		      void (*fail) (void *arg, const char *fmt, ...)
		        __attribute__ ((noreturn, format (printf, 2, 3))),
		      void *fail_arg,
		      void (*emit_address) (void *fail_arg,
					    struct obstack *, Dwarf_Addr),
		      int indent, Dwarf_Addr dwbias, Dwarf_Addr pc_address,
		      Dwarf_Attribute *attr,
		      const Dwarf_Op *expr, size_t len,
		      struct location **input, Dwarf_Attribute *fb_attr,
		      const Dwarf_Op *cfa_ops)
{
  indent += 2;

  struct location_context *ctx;
  if (*input == NULL)
    ctx = new_context (pool, fail, fail_arg, emit_address, dwbias, pc_address,
		       attr, fb_attr, cfa_ops);
  else
    {
      ctx = (*input)->context;
      assert (ctx->pool == pool);
      if (pc_address == 0)
	pc_address = ctx->pc;
      else if (ctx->pc == 0)
	ctx->pc = pc_address;
      // PR15148: disable this assertion, in case the PR15123 address-retry logic
      // sent us this way
      // assert (ctx->pc == pc_address);
    }

  switch (*input == NULL ? loc_address : (*input)->type)
    {
    case loc_address:
      /* We have a previous address computation.
	 This expression will compute starting with that on the stack.  */
      return location_from_address (ctx, indent, expr, len, input);

    case loc_noncontiguous:
    case loc_register:
    case loc_value:
    case loc_constant:
    case loc_unavailable:
    case loc_implicit_pointer:
      /* The starting point is not an address computation, but a
	 register or implicit value.  We can only handle limited
	 computations from here.  */
      return location_relative (ctx, indent, expr, len, input);

    default:
      abort ();
      break;
    }

  return NULL;
}


/* Translate a C fragment for a direct argument VALUE.  On errors, call FAIL,
   which should not return.  Any later errors will use FAIL and FAIL_ARG from
   this translate call.  On success, return the fragment created. */
struct location *
c_translate_argument (struct obstack *pool,
                      void (*fail) (void *arg, const char *fmt, ...)
                      __attribute__ ((noreturn, format (printf, 2, 3))),
                      void *fail_arg,
                      void (*emit_address) (void *fail_arg,
                                            struct obstack *, Dwarf_Addr),
                      int indent, const char *value)
{
  indent += 2;

  obstack_printf (pool, "%*saddr = %s;\n", indent * 2, "", value);
  obstack_1grow (pool, '\0');
  char *program = obstack_finish (pool);

  struct location *loc = obstack_alloc (pool, sizeof *loc);
  loc->context = new_context (pool, fail, fail_arg, emit_address, 0,
			      0, NULL, NULL, NULL);
  loc->next = NULL;
  loc->ops = NULL;
  loc->nops = 0;
  loc->byte_size = 0;
  loc->type = loc_address;
  loc->frame_base = NULL;
  loc->address.declare = NULL;
  loc->address.program = program;
  loc->address.stack_depth = 0;
  loc->address.used_deref = false;

  return loc;
}


/* Emit "uintNN_t TARGET = ...;".  */
static bool
emit_base_fetch (struct obstack *pool, Dwarf_Word byte_size,
                 bool signed_p, const char *target, struct location *loc)
{
  bool deref = false;

  /* Emit size/signed coercion. */
  obstack_printf (pool, "{ ");
  obstack_printf (pool, "%sint%u_t value = ",
                  (signed_p ? "" : "u"), (unsigned)(byte_size * 8));

  switch (loc->type)
    {
    case loc_value:
      obstack_printf (pool, "addr;");
      break;

    case loc_address:
      if (byte_size != 0 && byte_size != (Dwarf_Word) -1)
	obstack_printf (pool, "deref (%" PRIu64 ", addr);", byte_size);
      else
	obstack_printf (pool, "deref (sizeof %s, addr);", target);
      deref = true;
      break;

    case loc_register:
      if (loc->reg.offset != 0)
	FAIL (loc, N_("cannot handle offset into register in fetch"));
      obstack_printf (pool, "fetch_register (%u);", loc->reg.regno);
      break;

    case loc_noncontiguous:
      FAIL (loc, N_("noncontiguous location for base fetch"));
      break;

    case loc_implicit_pointer:
      FAIL (loc, N_("pointer has been optimized out"));
      break;

    case loc_unavailable:
      FAIL (loc, N_("location not available"));
      break;

    default:
      abort ();
      break;
    }

  obstack_printf (pool, " %s = value; }", target);
  return deref;
}

/* Emit "... = RVALUE;".  */
static bool
emit_base_store (struct obstack *pool, Dwarf_Word byte_size,
		 const char *rvalue, struct location *loc)
{
  switch (loc->type)
    {
    case loc_address:
      if (byte_size != 0 && byte_size != (Dwarf_Word) -1)
	obstack_printf (pool, "store_deref (%" PRIu64 ", addr, %s); ",
			byte_size, rvalue);
      else
	obstack_printf (pool, "store_deref (sizeof %s, addr, %s); ",
			rvalue, rvalue);
      return true;

    case loc_register:
      if (loc->reg.offset != 0)
	FAIL (loc, N_("cannot handle offset into register in store"));
      obstack_printf (pool, "store_register (%u, %s);", loc->reg.regno, rvalue);
      break;

    case loc_noncontiguous:
      FAIL (loc, N_("noncontiguous location for base store"));
      break;

    case loc_implicit_pointer:
      FAIL (loc, N_("pointer has been optimized out"));
      break;

    case loc_value:
      FAIL (loc, N_("location is computed value, cannot store"));
      break;

    case loc_constant:
      FAIL (loc, N_("location is constant value, cannot store"));
      break;

    case loc_unavailable:
      FAIL (loc, N_("location is not available, cannot store"));
      break;

    default:
      abort ();
      break;
    }

  return false;
}


/* Slice up an object into pieces no larger than MAX_PIECE_BYTES,
   yielding a loc_noncontiguous location unless LOC is small enough.  */
static struct location *
discontiguify (struct location_context *ctx, int indent, struct location *loc,
	       Dwarf_Word total_bytes, Dwarf_Word max_piece_bytes)
{
  inline bool pieces_small_enough (void)
    {
      if (loc->type != loc_noncontiguous)
	return total_bytes <= max_piece_bytes;
      struct location *p;
      for (p = loc->pieces; p != NULL; p = p->next)
	if (p->byte_size > max_piece_bytes)
	  return false;
      return true;
    }

  /* Constants are always copied byte-wise, but we may need to
   * truncate to the total_bytes requested here. */
  if (loc->type == loc_constant)
    {
      if (loc->byte_size > total_bytes)
	loc->byte_size = total_bytes;
      return loc;
    }

  if (pieces_small_enough ())
    return loc;

  struct location *noncontig = alloc_location (ctx);
  noncontig->next = NULL;
  noncontig->type = loc_noncontiguous;
  noncontig->byte_size = total_bytes;
  noncontig->pieces = NULL;
  struct location **tailpiece = &noncontig->pieces;
  inline void add (struct location *piece)
    {
      *tailpiece = piece;
      tailpiece = &piece->next;
    }

  switch (loc->type)
    {
    case loc_address:
      {
	/* Synthesize a piece that sets "container_addr" to the computed
	   address of the whole object.  Each piece will refer to this.  */
	obstack_printf (ctx->pool, "%*scontainer_addr = addr;\n",
			indent++ * 2, "");
	loc->next = new_synthetic_loc (loc, false);
	loc->next->byte_size = loc->byte_size;
	loc->next->type = loc_fragment;
	loc->next->address.declare = "container_addr";
	loc = loc->next;

	/* Synthesize pieces that just compute "container_addr + N".  */
	Dwarf_Word offset = 0;
	while (total_bytes - offset > 0)
	  {
	    Dwarf_Word size = total_bytes - offset;
	    if (size > max_piece_bytes)
	      size = max_piece_bytes;

	    obstack_printf (ctx->pool,
			    "%*saddr = container_addr + " UFORMAT ";\n",
			    indent * 2, "", offset);
	    struct location *piece = new_synthetic_loc (loc, false);
	    piece->byte_size = size;
	    add (piece);

	    offset += size;
	  }

	--indent;
	break;
      }

    case loc_value:
      FAIL (loc, N_("stack value too big for fetch ???"));
      break;

    case loc_register:
      FAIL (loc, N_("single register too big for fetch/store ???"));
      break;

    case loc_implicit_pointer:
      FAIL (loc, N_("implicit pointer too big for fetch/store ???"));
      break;

    case loc_noncontiguous:
      /* Could be handled if it ever happened.  */
      FAIL (loc, N_("cannot support noncontiguous location"));
      break;

    default:
      abort ();
      break;
    }

  loc->next = noncontig;
  return noncontig;
}

/* Make a fragment that declares a union such as:
    union {
      char bytes[8];
      struct {
        uint32_t p0;
        uint32_t p4;
      } pieces __attribute__ ((packed));
      uint64_t whole;
    } u_pieces<depth>;
*/
static void
declare_noncontig_union (struct obstack *pool, int indent,
			 struct location **input, struct location *loc,
			 int depth)
{
  if (depth > 9)
    FAIL (loc, N_("declaring noncontig union for depth > 9, too many pieces"));

  obstack_printf (pool, "%*sunion {\n", indent++ * 2, "");

  obstack_printf (pool, "%*schar bytes[%" PRIu64 "];\n",
		  indent * 2, "", loc->byte_size);

  if (loc->type == loc_noncontiguous)
    {
      Dwarf_Word offset = 0;
      struct location *p;
      obstack_printf (pool, "%*sstruct {\n", indent++ * 2, "");

      for (p = loc->pieces; p != NULL; p = p->next)
	{
	  obstack_printf (pool, "%*suint%" PRIu64 "_t p%" PRIu64 ";\n",
			  indent * 2, "", p->byte_size * 8, offset);
	  offset += p->byte_size;
	}

      obstack_printf (pool, "%*s} pieces __attribute__ ((packed));\n",
		      --indent * 2, "");
    }

  obstack_printf (pool, "%*suint%" PRIu64 "_t whole;\n",
		  indent * 2, "", loc->byte_size * 8);

  obstack_printf (pool, "%*s} u_pieces%d;\n", --indent * 2, "", depth);

  loc = new_synthetic_loc (*input, false);
  loc->type = loc_decl;
  (*input)->next = loc;
  *input = loc;
}

/* Determine the byte size of a base type.  */
static Dwarf_Word
base_byte_size (Dwarf_Die *typedie, struct location *origin)
{
  assert (dwarf_tag (typedie) == DW_TAG_base_type ||
	  dwarf_tag (typedie) == DW_TAG_enumeration_type);

  Dwarf_Attribute attr_mem;
  Dwarf_Word size;
  if (dwarf_attr_integrate (typedie, DW_AT_byte_size, &attr_mem) != NULL
      && dwarf_formudata (&attr_mem, &size) == 0)
    return size;

  FAIL (origin,
	 N_("cannot get byte_size attribute for type %s: %s"),
	 dwarf_diename (typedie) ?: "<anonymous>",
	 dwarf_errmsg (-1));
  return -1;
}

static Dwarf_Word
base_encoding (Dwarf_Die *typedie, struct location *origin)
{
  if (! (dwarf_tag (typedie) == DW_TAG_base_type ||
         dwarf_tag (typedie) == DW_TAG_enumeration_type))
    return -1;

  Dwarf_Attribute attr_mem;
  Dwarf_Word encoding;
  if (dwarf_attr_integrate (typedie, DW_AT_encoding, &attr_mem) != NULL
      && dwarf_formudata (&attr_mem, &encoding) == 0)
    return encoding;

  (void) origin;
  /*
  FAIL (origin,
	 N_("cannot get encoding attribute for type %s: %s"),
	 dwarf_diename (typedie) ?: "<anonymous>",
	 dwarf_errmsg (-1));
  */
  return -1;
}



/* Fetch the bitfield parameters.  */
static void
get_bitfield (struct location *loc,
	      Dwarf_Die *die, Dwarf_Word *bit_offset, Dwarf_Word *bit_size)
{
  Dwarf_Attribute attr_mem;
  if (dwarf_attr_integrate (die, DW_AT_bit_offset, &attr_mem) == NULL
      || dwarf_formudata (&attr_mem, bit_offset) != 0
      || dwarf_attr_integrate (die, DW_AT_bit_size, &attr_mem) == NULL
      || dwarf_formudata (&attr_mem, bit_size) != 0)
    FAIL (loc, N_("cannot get bit field parameters: %s"), dwarf_errmsg (-1));
}

/* Translate a fragment to fetch the base-type value of BYTE_SIZE bytes
   at the *INPUT location and store it in lvalue TARGET.  */
static void
translate_base_fetch (struct obstack *pool, int indent,
		      Dwarf_Word byte_size, bool signed_p,
		      struct location **input, const char *target,
		      int depth)
{
  bool deref = false;

  if ((*input)->type == loc_noncontiguous)
    {
      struct location *p = (*input)->pieces;

      declare_noncontig_union (pool, indent, input, *input, depth);

      Dwarf_Word offset = 0;
      char piece[sizeof "u_pieces?.pieces.p" + 20] = "u_pieces?.pieces.p";
      piece[8] = (char) ('0' + depth);
      int pdepth = depth + 1;
      while (p != NULL)
	{
	  struct location *newp = obstack_alloc (pool, sizeof *newp);
	  *newp = *p;
	  newp->next = NULL;
	  (*input)->next = newp;
	  *input = newp;

	  snprintf (&piece[sizeof "u_pieces?.pieces.p" - 1], 20,
		    "%" PRIu64, offset);
	  translate_base_fetch (pool, indent, p->byte_size, signed_p /* ? */,
                                input, piece, pdepth);
	  (*input)->type = loc_fragment;

	  offset += p->byte_size;
	  p = p->next;
	  pdepth++;
	}

      obstack_printf (pool, "%*s%s = u_pieces%d.whole;\n", indent * 2,
		      "", target, depth);
    }
  else if ((*input)->type == loc_constant)
    {
      const unsigned char *constant_block = (*input)->constant_block;
      const size_t byte_size = (*input)->byte_size;
      size_t i;

      declare_noncontig_union (pool, indent, input, *input, depth);

      for (i = 0; i < byte_size; ++i)
	obstack_printf (pool, "%*su_pieces%d.bytes[%zu] = %#x;\n", indent * 2,
			"", depth, i, constant_block[i]);

      obstack_printf (pool, "%*s%s = u_pieces%d.whole;\n", indent * 2,
		      "", target, depth);
    }
  else
    switch (byte_size)
      {
      case 0:			/* Special case, means address size.  */
      case 1:
      case 2:
      case 4:
      case 8:
	obstack_printf (pool, "%*s", indent * 2, "");
	deref = emit_base_fetch (pool, byte_size, signed_p, target, *input);
	obstack_printf (pool, "\n");
	break;

      default:
	/* Could handle this generating call to memcpy equivalent.  */
	FAIL (*input, N_("fetch is larger than base integer types"));
	break;
      }

  struct location *loc = new_synthetic_loc (*input, deref);
  loc->byte_size = byte_size;
  loc->type = loc_final;
  (*input)->next = loc;
  *input = loc;
}

/* Determine the maximum size of a base type, from some DIE in the CU.  */
static Dwarf_Word
max_fetch_size (struct location *loc, Dwarf_Die *die)
{
  Dwarf_Die cu_mem;
  uint8_t address_size;
  Dwarf_Die *cu = dwarf_diecu (die, &cu_mem, &address_size, NULL);
  if (cu == NULL)
    //TRANSLATORS: CU stands for 'compilation unit'
    FAIL (loc, N_("cannot determine CU address size from %s: %s"),
	  dwarf_diename (die), dwarf_errmsg (-1));

  return address_size;
}

/* Translate a fragment to fetch the value of variable or member DIE
   at the *INPUT location and store it in lvalue TARGET.  */
void
c_translate_fetch (struct obstack *pool, int indent,
		   Dwarf_Addr dwbias __attribute__ ((unused)),
		   Dwarf_Die *die, Dwarf_Die *typedie,
		   struct location **input, const char *target)
{
  struct location_context *const ctx = (*input)->context;
  assert (ctx->pool == pool);

  ++indent;

  Dwarf_Attribute size_attr;
  Dwarf_Word byte_size;
  if (dwarf_attr_integrate (die, DW_AT_byte_size, &size_attr) == NULL
      || dwarf_formudata (&size_attr, &byte_size) != 0)
    byte_size = base_byte_size (typedie, *input);

  Dwarf_Attribute encoding_attr;
  Dwarf_Word encoding;
  if (dwarf_attr_integrate (die, DW_AT_encoding, &encoding_attr) == NULL
      || dwarf_formudata (&encoding_attr, &encoding) != 0)
    encoding = base_encoding (typedie, *input);
  bool signed_p = encoding == DW_ATE_signed || encoding == DW_ATE_signed_char;

  *input = discontiguify (ctx, indent, *input, byte_size,
			  max_fetch_size (*input, die));

  if (dwarf_hasattr_integrate (die, DW_AT_bit_offset))
    {
      /* This is a bit field.  Fetch the containing base type into a
	 temporary variable.  */

      translate_base_fetch (pool, indent, byte_size, signed_p, input, "tmp", 0);
      (*input)->type = loc_fragment;
      (*input)->address.declare = "tmp";

      Dwarf_Word bit_offset = 0;
      Dwarf_Word bit_size = 0;
      get_bitfield (*input, die, &bit_offset, &bit_size);

      obstack_printf (pool, "%*s"
		      "fetch_bitfield (%s, tmp, %" PRIu64 ", %" PRIu64 ");\n",
		      indent *2, "", target, bit_offset, bit_size);

      struct location *loc = new_synthetic_loc (*input, false);
      loc->type = loc_final;
      (*input)->next = loc;
      *input = loc;
    }
  else
    translate_base_fetch (pool, indent, byte_size, signed_p, input, target, 0);
}

/* Translate a fragment to store RVALUE into the base-type value of
   BYTE_SIZE bytes at the *INPUT location.  */
static void
translate_base_store (struct obstack *pool, int indent, Dwarf_Word byte_size,
		      struct location **input, struct location *store_loc,
		      const char *rvalue, int depth)
{
  bool deref = false;

  if (store_loc->type == loc_noncontiguous)
    {
      declare_noncontig_union (pool, indent, input, store_loc, depth);

      obstack_printf (pool, "%*su_pieces%d.whole = %s;\n", indent * 2,
		      "", depth, rvalue);
      struct location *loc = new_synthetic_loc (*input, deref);
      loc->type = loc_fragment;
      (*input)->next = loc;
      *input = loc;

      Dwarf_Word offset = 0;
      char piece[sizeof "u_pieces?.pieces.p" + 20] = "u_pieces?.pieces.p";
      piece[8] = (char) ('0' + depth);
      struct location *p;
      int pdepth = depth + 1;
      for (p = store_loc->pieces; p != NULL; p = p->next)
        {
	  struct location *newp = obstack_alloc (pool, sizeof *newp);
	  *newp = *p;
	  newp->next = NULL;
	  (*input)->next = newp;
	  *input = newp;

	  snprintf (&piece[sizeof "u_pieces?.pieces.p" - 1], 20, "%" PRIu64,
		    offset);
	  translate_base_store (pool, indent,
				p->byte_size, input, *input, piece, pdepth++);
	  (*input)->type = loc_fragment;

	  offset += p->byte_size;
	}

      (*input)->type = loc_final;
    }
  else
    {
      switch (byte_size)
	{
	case 1:
	case 2:
	case 4:
	case 8:
	  obstack_printf (pool, "%*s", indent * 2, "");
	  deref = emit_base_store (pool, byte_size, rvalue, store_loc);
	  obstack_printf (pool, "\n");
	  break;

	default:
	  /* Could handle this generating call to memcpy equivalent.  */
	  FAIL (*input, N_("store is larger than base integer types"));
	  break;
	}

      struct location *loc = new_synthetic_loc (*input, deref);
      loc->type = loc_final;
      (*input)->next = loc;
      *input = loc;
    }
}

/* Translate a fragment to fetch the value of variable or member DIE
   at the *INPUT location and store it in rvalue RVALUE.  */

void
c_translate_store (struct obstack *pool, int indent,
		   Dwarf_Addr dwbias __attribute__ ((unused)),
		   Dwarf_Die *die, Dwarf_Die *typedie,
		   struct location **input, const char *rvalue)
{
  struct location_context *const ctx = (*input)->context;
  assert (ctx->pool == pool);

  ++indent;

  Dwarf_Attribute size_attr;
  Dwarf_Word byte_size;
  if (dwarf_attr_integrate (die, DW_AT_byte_size, &size_attr) == NULL
      || dwarf_formudata (&size_attr, &byte_size) != 0)
    byte_size = base_byte_size (typedie, *input);

  Dwarf_Attribute encoding_attr;
  Dwarf_Word encoding;
  if (dwarf_attr_integrate (die, DW_AT_encoding, &encoding_attr) == NULL
      || dwarf_formudata (&encoding_attr, &encoding) != 0)
    encoding = base_encoding (typedie, *input);
  bool signed_p = (encoding == DW_ATE_signed
                   || encoding == DW_ATE_signed_char);

  *input = discontiguify (ctx, indent, *input, byte_size,
			  max_fetch_size (*input, die));

  struct location *store_loc = *input;

  if (dwarf_hasattr_integrate (die, DW_AT_bit_offset))
    {
      /* This is a bit field.  Fetch the containing base type into a
	 temporary variable.  */

      translate_base_fetch (pool, indent, byte_size, signed_p, input, "tmp", 0);
      (*input)->type = loc_fragment;
      (*input)->address.declare = "tmp";

      Dwarf_Word bit_offset = 0;
      Dwarf_Word bit_size = 0;
      get_bitfield (*input, die, &bit_offset, &bit_size);

      obstack_printf (pool, "%*s"
		      "store_bitfield (tmp, %s, %" PRIu64 ", %" PRIu64 ");\n",
		      indent * 2, "", rvalue, bit_offset, bit_size);

      struct location *loc = new_synthetic_loc (*input, false);
      loc->type = loc_fragment;
      (*input)->next = loc;
      *input = loc;

      /* We have mixed RVALUE into the bits in "tmp".
	 Now we'll store "tmp" back whence we fetched it.  */
      rvalue = "tmp";
    }

  translate_base_store (pool, indent, byte_size, input, store_loc, rvalue, 0);
}

/* Translate a fragment to dereference the given pointer type,
   where *INPUT is the location of the pointer with that type.

   We chain on a loc_address program that yields this pointer value
   (i.e. the location of what it points to).  */

void
c_translate_pointer (struct obstack *pool, int indent,
		     Dwarf_Addr dwbias __attribute__ ((unused)),
		     Dwarf_Die *typedie, struct location **input)
{
  assert (dwarf_tag (typedie) == DW_TAG_pointer_type ||
          dwarf_tag (typedie) == DW_TAG_reference_type ||
          dwarf_tag (typedie) == DW_TAG_rvalue_reference_type);

  Dwarf_Attribute attr_mem;
  Dwarf_Word byte_size;
  if (dwarf_attr_integrate (typedie, DW_AT_byte_size, &attr_mem) == NULL)
    byte_size = 0;
  else if (dwarf_formudata (&attr_mem, &byte_size) != 0)
    FAIL (*input,
	  N_("cannot get byte_size attribute for type %s: %s"),
	  dwarf_diename (typedie) ?: "<anonymous>",
	  dwarf_errmsg (-1));

  if ((*input)->type == loc_implicit_pointer)
    {
      struct location *const target = (*input)->pointer.target;
      const Dwarf_Sword offset = (*input)->pointer.offset;
      (*input)->next = target;
      if (offset != 0)
	translate_offset (indent, NULL, 0, 0, NULL, &(*input)->next, offset);
      *input = (*input)->next;
    }
  else
    {
      bool signed_p = false;	/* XXX: Does not matter? */

      translate_base_fetch (pool, indent + 1, byte_size, signed_p, input,
			    "addr", 0);
      (*input)->type = loc_address;
    }
}


void
c_translate_addressof (struct obstack *pool, int indent,
		       Dwarf_Addr dwbias __attribute__ ((unused)),
		       Dwarf_Die *die,
		       Dwarf_Die *typedie __attribute__ ((unused)),
		       struct location **input, const char *target)
{
  ++indent;

  if (dwarf_hasattr_integrate (die, DW_AT_bit_offset)
      || dwarf_hasattr_integrate (die, DW_AT_data_bit_offset))
    FAIL (*input, N_("cannot take the address of a bit field"));

  switch ((*input)->type)
    {
    case loc_address:
      obstack_printf (pool, "%*s%s = addr;\n", indent * 2, "", target);
      (*input)->next = new_synthetic_loc (*input, false);
      (*input)->next->type = loc_final;
      break;

    case loc_register:
      FAIL (*input, N_("cannot take address of object in register"));
      break;
    case loc_noncontiguous:
      FAIL (*input, N_("cannot take address of noncontiguous object"));
      break;
    case loc_value:
      FAIL (*input, N_("cannot take address of computed value"));
      break;
    case loc_constant:
      FAIL (*input, N_("cannot take address of constant value"));
      break;
    case loc_unavailable:
      FAIL (*input, N_("cannot take address of unavailable value"));
      break;
    case loc_implicit_pointer:
      FAIL (*input, N_("cannot take address of implicit pointer"));
      break;

    default:
      abort ();
      break;
    }
}


/* Translate a fragment to write the given pointer value,
   where *INPUT is the location of the pointer with that type.
*/

void
c_translate_pointer_store (struct obstack *pool, int indent,
                           Dwarf_Addr dwbias __attribute__ ((unused)),
                           Dwarf_Die *typedie, struct location **input,
                           const char *rvalue)
{
  assert (dwarf_tag (typedie) == DW_TAG_pointer_type);

  Dwarf_Attribute attr_mem;
  Dwarf_Word byte_size;
  if (dwarf_attr_integrate (typedie, DW_AT_byte_size, &attr_mem) == NULL)
    byte_size = 0;
  else if (dwarf_formudata (&attr_mem, &byte_size) != 0)
    FAIL (*input,
	  N_("cannot get byte_size attribute for type %s: %s"),
	  dwarf_diename (typedie) ?: "<anonymous>",
	  dwarf_errmsg (-1));

  translate_base_store (pool, indent + 1, byte_size,
                        input, *input, rvalue, 0);

  // XXX: what about multiple-location lvalues?
}

/* Determine the element stride of a pointer to a type.  */
static Dwarf_Word
pointer_stride (Dwarf_Die *typedie, struct location *origin)
{
  Dwarf_Attribute attr_mem;
  Dwarf_Die die_mem = *typedie;
  int typetag = dwarf_tag (&die_mem);
  while (typetag == DW_TAG_typedef ||
	 typetag == DW_TAG_const_type ||
	 typetag == DW_TAG_volatile_type)
    {
      if (dwarf_attr_integrate (&die_mem, DW_AT_type, &attr_mem) == NULL
	  || dwarf_formref_die (&attr_mem, &die_mem) == NULL)
        //TRANSLATORS: This refers to the basic type, (stripped of const/volatile/etc.)
	FAIL (origin, N_("cannot get inner type of type %s: %s"),
	      dwarf_diename (&die_mem) ?: "<anonymous>",
	      dwarf_errmsg (-1));
      typetag = dwarf_tag (&die_mem);
    }

  if (dwarf_attr_integrate (&die_mem, DW_AT_byte_size, &attr_mem) != NULL)
    {
      Dwarf_Word stride;
      if (dwarf_formudata (&attr_mem, &stride) == 0)
	return stride;
      FAIL (origin,
	    N_("cannot get byte_size attribute for array element type %s: %s"),
	    dwarf_diename (&die_mem) ?: "<anonymous>",
	    dwarf_errmsg (-1));
    }

  FAIL (origin, N_("confused about array element size"));
  return 0;
}

/* Determine the element stride of an array type.  */
static Dwarf_Word
array_stride (Dwarf_Die *typedie, struct location *origin)
{
  Dwarf_Attribute attr_mem;
  if (dwarf_attr_integrate (typedie, DW_AT_byte_stride, &attr_mem) != NULL)
    {
      Dwarf_Word stride;
      if (dwarf_formudata (&attr_mem, &stride) == 0)
	return stride;
      FAIL (origin, N_("cannot get byte_stride attribute array type %s: %s"),
	    dwarf_diename (typedie) ?: "<anonymous>",
	    dwarf_errmsg (-1));
    }

  Dwarf_Die die_mem;
  if (dwarf_attr_integrate (typedie, DW_AT_type, &attr_mem) == NULL
      || dwarf_formref_die (&attr_mem, &die_mem) == NULL)
    FAIL (origin, N_("cannot get element type of array type %s: %s"),
	  dwarf_diename (typedie) ?: "<anonymous>",
	  dwarf_errmsg (-1));

  return pointer_stride (&die_mem, origin);
}

static void
translate_array (struct obstack *pool, int indent,
		 Dwarf_Die *anydie, Dwarf_Word stride,
		 struct location **input,
		 const char *idx, Dwarf_Word const_idx)
{
  ++indent;

  struct location *loc = *input;
  while (loc->type == loc_noncontiguous)
    {
      if (idx != NULL)
	FAIL (*input, N_("cannot dynamically index noncontiguous array"));
      else
	{
	  Dwarf_Word offset = const_idx * stride;
	  struct location *piece = loc->pieces;
	  while (piece != NULL && offset >= piece->byte_size)
	    {
	      offset -= piece->byte_size;
	      piece = piece->next;
	    }
	  if (piece == NULL)
            //TRANSLATORS: The index is constant
	    FAIL (*input, N_("constant index is outside noncontiguous array"));
	  if (offset % stride != 0 || piece->byte_size < stride)
	    FAIL (*input, N_("noncontiguous array splits elements"));
	  const_idx = offset / stride;
	  loc = piece;
	}
    }

  switch (loc->type)
    {
    case loc_address:
      ++indent;
      if (idx != NULL)
	obstack_printf (pool, "%*saddr += %s * " UFORMAT ";\n",
			indent * 2, "", idx, stride);
      else
	obstack_printf (pool, "%*saddr += " UFORMAT " * " UFORMAT ";\n",
			indent * 2, "", const_idx, stride);
      loc = new_synthetic_loc (loc, false);
      break;

    case loc_register:
      if (idx != NULL)
	FAIL (*input, N_("cannot index array stored in a register"));
      else if (const_idx > max_fetch_size (loc, anydie) / stride)
	FAIL (*input, N_("constant index is outside array held in register"));
      else
	{
	  loc->reg.offset += const_idx * stride;
	  return;
	}
      break;

    case loc_constant:
      if (idx != NULL)
	FAIL (*input, N_("cannot index into constant value"));
      else if (const_idx > loc->byte_size / stride)
        //TRANSLATORS: The index is constant
	FAIL (*input, N_("constant index is outside constant array value"));
      else
	{
	  loc->byte_size = stride;
	  loc->constant_block += const_idx * stride;
	  return;
	};
      break;

    case loc_implicit_pointer:
      if (idx != NULL)
	FAIL (*input, N_("cannot index into implicit pointer"));
      else
	loc->pointer.offset += const_idx * stride;
      break;

    case loc_value:
      if (idx != NULL || const_idx != 0)
	FAIL (*input, N_("cannot index into computed value"));
      break;

    case loc_unavailable:
      if (idx != NULL || const_idx != 0)
	FAIL (*input, N_("cannot index into unavailable value"));
      break;

    default:
      abort ();
      break;
    }

  (*input)->next = loc;
  *input = (*input)->next;
}

void
c_translate_array (struct obstack *pool, int indent,
		   Dwarf_Addr dwbias __attribute__ ((unused)),
		   Dwarf_Die *typedie, struct location **input,
		   const char *idx, Dwarf_Word const_idx)
{
  assert (dwarf_tag (typedie) == DW_TAG_array_type ||
          dwarf_tag (typedie) == DW_TAG_pointer_type);

  return translate_array (pool, indent, typedie,
			  array_stride (typedie, *input),
			  input, idx, const_idx);
}

void
c_translate_array_pointer (struct obstack *pool, int indent,
			   Dwarf_Die *typedie, struct location **input,
			   const char *idx, Dwarf_Word const_idx)
{
  return translate_array (pool, indent, typedie,
			  pointer_stride (typedie, *input),
			  input, idx, const_idx);
}

/* Emitting C code for finalized fragments.  */

#define emit(fmt, ...) fprintf (out, fmt, ## __VA_ARGS__)

/* Open a block with a comment giving the original DWARF expression.  */
static void
emit_header (FILE *out, struct location *loc, unsigned int hindent)
{
  if (loc->ops == NULL)
    emit ("%*s{ // synthesized\n", hindent * 2, "");
  else
    {
      emit ("%*s{ // DWARF expression:", hindent * 2, "");
      size_t i;
      for (i = 0; i < loc->nops; ++i)
	{
	  emit (" %#x", loc->ops[i].atom);
	  if (loc->ops[i].number2 == 0)
	    {
	      if (loc->ops[i].number != 0)
		emit ("(%" PRId64 ")", loc->ops[i].number);
	    }
	  else
	    emit ("(%" PRId64 ",%" PRId64 ")",
		  loc->ops[i].number, loc->ops[i].number2);
	}
      emit ("\n");
    }
}

/* Emit a code fragment to assign the target variable to a register value.  */
static void
emit_loc_register (FILE *out, struct location *loc, unsigned int indent,
		   const char *target)
{
  assert (loc->type == loc_register);

  if (loc->reg.offset != 0)
    FAIL (loc, N_("cannot handle offset into register in fetch"));

  emit ("%*s%s = fetch_register (%u);\n",
	indent * 2, "", target, loc->reg.regno);
}

/* Emit a code fragment to assign the target variable to an address.  */
static void
emit_loc_address (FILE *out, struct location *loc, unsigned int indent,
		  const char *target)
{
  assert (loc->type == loc_address || loc->type == loc_value);

  if (loc->address.stack_depth == 0)
    /* Synthetic program.  */
    emit ("%s", loc->address.program);
  else
    {
      emit ("%*s{\n", indent * 2, "");
      emit ("%*s%s " STACKFMT, (indent + 1) * 2, "",
	    stack_slot_type (loc, false), 0);
      unsigned i;
      for (i = 1; i < loc->address.stack_depth; ++i)
	emit (", " STACKFMT, i);
      emit (";\n");

      emit ("%s%*s%s = " STACKFMT ";\n", loc->address.program,
	    (indent + 1) * 2, "", target, 0);
      emit ("%*s}\n", indent * 2, "");
    }
}

/* Emit a code fragment to declare the target variable and
   assign it to an address-sized value.  */
static void
emit_loc_value (FILE *out, struct location *loc, unsigned int indent,
		const char *target, bool declare,
		bool *used_deref, unsigned int *max_stack)
{
  if (declare)
    emit ("%*s%s %s;\n", indent * 2, "", stack_slot_type (loc, false), target);

  emit_header (out, loc, indent++);

  switch (loc->type)
    {
    default:
      abort ();
      break;

    case loc_register:
      emit_loc_register (out, loc, indent, target);
      break;

    case loc_address:
    case loc_value:
      emit_loc_address (out, loc, indent, target);
      *used_deref = *used_deref || loc->address.used_deref;
      if (loc->address.stack_depth > *max_stack)
	*max_stack = loc->address.stack_depth;
      break;
    }

  emit ("%*s}\n", --indent * 2, "");
}

bool
c_emit_location (FILE *out, struct location *loc, int indent,
		 unsigned int *max_stack)
{
  emit ("%*s{\n", indent * 2, "");

  bool declared_addr = false;
  struct location *l;
  for (l = loc; l != NULL; l = l->next)
    switch (l->type)
      {
      case loc_decl:
	emit ("%s", l->address.program);
	break;

      case loc_address:
      case loc_value:
	if (declared_addr)
	  break;
	declared_addr = true;
	l->address.declare = "addr";
      case loc_fragment:
      case loc_final:
	if (l->address.declare != NULL)
	  {
	    if (l->byte_size == 0 || l->byte_size == (Dwarf_Word) -1)
	      emit ("%*s%s %s;\n", (indent + 1) * 2, "",
		    stack_slot_type (l, false), l->address.declare);
	    else
	      emit ("%*suint%" PRIu64 "_t %s;\n", (indent + 1) * 2, "",
		    l->byte_size * 8, l->address.declare);
	  }

      default:
	break;
      }

  bool deref = false;
  *max_stack = 0;

  for (l = loc; l != NULL; l = l->next)
    if (l->frame_base != NULL)
      {
	emit_loc_value (out, l->frame_base, indent, "frame_base", true,
			&deref, max_stack);
	break;
      }

  for (; loc->next != NULL; loc = loc->next)
    switch (loc->type)
      {
      case loc_address:
      case loc_value:
	/* Emit the program fragment to calculate the address.  */
	emit_loc_value (out, loc, indent + 1, "addr", false, &deref, max_stack);
	break;

      case loc_fragment:
	emit ("%s", loc->address.program);
	deref = deref || loc->address.used_deref;
	break;

      case loc_decl:
      case loc_register:
      case loc_noncontiguous:
      case loc_constant:
      case loc_implicit_pointer:
	/* These don't produce any code directly.
	   The next address/final record incorporates the value.  */
	break;

      case loc_final:		/* Should be last in chain!  */
      default:
	abort ();
	break;
      }

  if (loc->type != loc_final)	/* Unfinished chain.  */
    abort ();

  emit ("%s%*s}\n", loc->address.program, indent * 2, "");

  if (loc->address.stack_depth > *max_stack)
    *max_stack = loc->address.stack_depth;

  return deref || loc->address.used_deref;
}

#undef emit

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
