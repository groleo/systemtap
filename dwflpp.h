// C++ interface to dwfl
// Copyright (C) 2005-2013 Red Hat Inc.
// Copyright (C) 2005-2007 Intel Corporation.
// Copyright (C) 2008 James.Bottomley@HansenPartnership.com
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef DWFLPP_H
#define DWFLPP_H

#include "config.h"
#include "dwarf_wrappers.h"
#include "elaborate.h"
#include "session.h"
#include "unordered.h"
#include "setupdwfl.h"

#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" {
#include <elfutils/libdwfl.h>
#include <regex.h>
}


struct func_info;
struct inline_instance_info;
struct symbol_table;
struct base_query;
struct dwarf_query;

enum line_t { ABSOLUTE, RELATIVE, RANGE, WILDCARD };
enum info_status { info_unknown, info_present, info_absent };

// module -> cu die[]
typedef unordered_map<Dwarf*, std::vector<Dwarf_Die>*> module_cu_cache_t;

// An instance of this type tracks whether the type units for a given
// Dwarf have been read.
typedef std::set<Dwarf*> module_tus_read_t;

// typename -> die
typedef unordered_map<std::string, Dwarf_Die> cu_type_cache_t;

// cu die -> (typename -> die)
typedef unordered_map<void*, cu_type_cache_t*> mod_cu_type_cache_t;

// function -> die
typedef unordered_multimap<std::string, Dwarf_Die> cu_function_cache_t;
typedef std::pair<cu_function_cache_t::iterator,
                  cu_function_cache_t::iterator>
        cu_function_cache_range_t;

// cu die -> (function -> die)
typedef unordered_map<void*, cu_function_cache_t*> mod_cu_function_cache_t;

// module -> (function -> die)
typedef unordered_map<Dwarf*, cu_function_cache_t*> mod_function_cache_t;

// inline function die -> instance die[]
typedef unordered_map<void*, std::vector<Dwarf_Die>*> cu_inl_function_cache_t;

// die -> parent die
typedef unordered_map<void*, Dwarf_Die> cu_die_parent_cache_t;

// cu die -> (die -> parent die)
typedef unordered_map<void*, cu_die_parent_cache_t*> mod_cu_die_parent_cache_t;

typedef std::vector<func_info> func_info_map_t;
typedef std::vector<inline_instance_info> inline_instance_map_t;


/* XXX FIXME functions that dwflpp needs from tapsets.cxx */
func_info_map_t *get_filtered_functions(dwarf_query *q);
inline_instance_map_t *get_filtered_inlines(dwarf_query *q);


struct
module_info
{
  Dwfl_Module* mod;
  const char* name;
  std::string elf_path;
  Dwarf_Addr addr;
  Dwarf_Addr bias;
  symbol_table *sym_table;
  info_status dwarf_status;     // module has dwarf info?
  info_status symtab_status;    // symbol table cached?

  void get_symtab(dwarf_query *q);
  void update_symtab(cu_function_cache_t *funcs);

  module_info(const char *name) :
    mod(NULL),
    name(name),
    addr(0),
    bias(0),
    sym_table(NULL),
    dwarf_status(info_unknown),
    symtab_status(info_unknown)
  {}

  ~module_info();
};


struct
module_cache
{
  std::map<std::string, module_info*> cache;
  bool paths_collected;
  bool dwarf_collected;

  module_cache() : paths_collected(false), dwarf_collected(false) {}
  ~module_cache();
};


struct func_info
{
  func_info()
    : decl_file(NULL), decl_line(-1), addr(0), entrypc(0), prologue_end(0),
      weak(false), descriptor(false)
  {
    std::memset(&die, 0, sizeof(die));
  }
  std::string name;
  char const * decl_file;
  int decl_line;
  Dwarf_Die die;
  Dwarf_Addr addr;
  Dwarf_Addr entrypc;
  Dwarf_Addr prologue_end;
  bool weak, descriptor;
};


struct inline_instance_info
{
  inline_instance_info()
    : decl_file(NULL), decl_line(-1), entrypc(0)
  {
    std::memset(&die, 0, sizeof(die));
  }
  bool operator<(const inline_instance_info& other) const;
  std::string name;
  char const * decl_file;
  int decl_line;
  Dwarf_Addr entrypc;
  Dwarf_Die die;
};


struct dwflpp
{
  systemtap_session & sess;

  // These are "current" values we focus on.
  Dwfl_Module * module;
  Dwarf_Addr module_bias;
  module_info * mod_info;

  // These describe the current module's PC address range
  Dwarf_Addr module_start;
  Dwarf_Addr module_end;

  Dwarf_Die * cu;

  std::string module_name;
  std::string function_name;

  dwflpp(systemtap_session & session, const std::string& user_module, bool kernel_p);
  dwflpp(systemtap_session & session, const std::vector<std::string>& user_modules, bool kernel_p);
  ~dwflpp();

  void get_module_dwarf(bool required = false, bool report = true);

  void focus_on_module(Dwfl_Module * m, module_info * mi);
  void focus_on_cu(Dwarf_Die * c);
  void focus_on_function(Dwarf_Die * f);

  std::string cu_name(void);

  Dwarf_Die *query_cu_containing_address(Dwarf_Addr a);

  bool module_name_matches(const std::string& pattern);
  static bool name_has_wildcard(const std::string& pattern);
  bool module_name_final_match(const std::string& pattern);

  bool function_name_matches_pattern(const std::string& name, const std::string& pattern);
  bool function_name_matches(const std::string& pattern);
  bool function_scope_matches(const std::vector<std::string>& scopes);

  void iterate_over_modules(int (* callback)(Dwfl_Module *, void **,
                                             const char *, Dwarf_Addr,
                                             void *),
                            void *data);

  void iterate_over_cus (int (*callback)(Dwarf_Die * die, void * arg),
                         void * data, bool want_types);

  bool func_is_inline();

  bool func_is_exported();

  void iterate_over_inline_instances (int (* callback)(Dwarf_Die * die, void * arg),
                                      void * data);

  std::vector<Dwarf_Die> getscopes_die(Dwarf_Die* die);
  std::vector<Dwarf_Die> getscopes(Dwarf_Die* die);
  std::vector<Dwarf_Die> getscopes(Dwarf_Addr pc);

  Dwarf_Die *declaration_resolve(Dwarf_Die *type);
  Dwarf_Die *declaration_resolve(const std::string& name);
  Dwarf_Die *declaration_resolve_other_cus(const std::string& name);

  int iterate_over_functions (int (* callback)(Dwarf_Die * func, base_query * q),
                              base_query * q, const std::string& function);

  int iterate_single_function (int (* callback)(Dwarf_Die * func, base_query * q),
                               base_query * q, const std::string& function);

  void iterate_over_srcfile_lines (char const * srcfile,
                                   int lines[2],
                                   bool need_single_match,
                                   enum line_t line_type,
                                   void (* callback) (const dwarf_line_t& line,
                                                      void * arg),
                                   const std::string& func_pattern,
                                   void *data);

  void iterate_over_labels (Dwarf_Die *begin_die,
                            const std::string& sym,
                            const std::string& function,
                            dwarf_query *q,
                            void (* callback)(const std::string &,
                                              const char *,
                                              const char *,
                                              int,
                                              Dwarf_Die *,
                                              Dwarf_Addr,
                                              dwarf_query *));

  int iterate_over_notes (void *object,
			  void (*callback)(void *object, int type,
					   const char *data, size_t len));

  void iterate_over_libraries (void (*callback)(void *object,
      const char *data), void *data);


  int iterate_over_plt (void *object,
			  void (*callback)(void *object, const char *name, size_t address));

  GElf_Shdr * get_section(std::string section_name, GElf_Shdr *shdr_mem,
                          Elf **elf_ret=NULL);

  void collect_srcfiles_matching (std::string const & pattern,
                                  std::set<std::string> & filtered_srcfiles);

  void resolve_prologue_endings (func_info_map_t & funcs);

  bool function_entrypc (Dwarf_Addr * addr);
  bool die_entrypc (Dwarf_Die * die, Dwarf_Addr * addr);

  void function_die (Dwarf_Die *d);
  void function_file (char const ** c);
  void function_line (int *linep);

  bool die_has_pc (Dwarf_Die & die, Dwarf_Addr pc);
  bool inner_die_containing_pc(Dwarf_Die& scope, Dwarf_Addr addr,
                               Dwarf_Die& result);

  std::string literal_stmt_for_local (std::vector<Dwarf_Die>& scopes,
                                      Dwarf_Addr pc,
                                      std::string const & local,
                                      const target_symbol *e,
                                      bool lvalue,
                                      exp_type & ty);
  Dwarf_Die* type_die_for_local (std::vector<Dwarf_Die>& scopes,
                                 Dwarf_Addr pc,
                                 std::string const & local,
                                 const target_symbol *e,
                                 Dwarf_Die *die_mem);

  std::string literal_stmt_for_return (Dwarf_Die *scope_die,
                                       Dwarf_Addr pc,
                                       const target_symbol *e,
                                       bool lvalue,
                                       exp_type & ty);
  Dwarf_Die* type_die_for_return (Dwarf_Die *scope_die,
                                  Dwarf_Addr pc,
                                  const target_symbol *e,
                                  Dwarf_Die *die_mem);

  std::string literal_stmt_for_pointer (Dwarf_Die *type_die,
                                        const target_symbol *e,
                                        bool lvalue,
                                        exp_type & ty);
  Dwarf_Die* type_die_for_pointer (Dwarf_Die *type_die,
                                   const target_symbol *e,
                                   Dwarf_Die *die_mem);

  bool blacklisted_p(const std::string& funcname,
                     const std::string& filename,
                     int line,
                     const std::string& module,
                     Dwarf_Addr addr,
                     bool has_return);

  Dwarf_Addr relocate_address(Dwarf_Addr addr, std::string& reloc_section);

  void resolve_unqualified_inner_typedie (Dwarf_Die *typedie,
                                          Dwarf_Die *innerdie,
                                          const target_symbol *e);


private:
  DwflPtr dwfl_ptr;

  // These are "current" values we focus on.
  Dwarf * module_dwarf;
  Dwarf_Die * function;

  void setup_kernel(const std::string& module_name, systemtap_session &s, bool debuginfo_needed = true);
  void setup_kernel(const std::vector<std::string>& modules, bool debuginfo_needed = true);
  void setup_user(const std::vector<std::string>& modules, bool debuginfo_needed = true);

  module_cu_cache_t module_cu_cache;
  module_tus_read_t module_tus_read;
  mod_cu_function_cache_t cu_function_cache;
  mod_function_cache_t mod_function_cache;

  std::set<void*> cu_inl_function_cache_done; // CUs that are already cached
  cu_inl_function_cache_t cu_inl_function_cache;
  void cache_inline_instances (Dwarf_Die* die);

  mod_cu_die_parent_cache_t cu_die_parent_cache;
  void cache_die_parents(cu_die_parent_cache_t* parents, Dwarf_Die* die);
  cu_die_parent_cache_t *get_die_parents();

  Dwarf_Die* get_parent_scope(Dwarf_Die* die);

  /* The global alias cache is used to resolve any DIE found in a
   * module that is stubbed out with DW_AT_declaration with a defining
   * DIE found in a different module.  The current assumption is that
   * this only applies to structures and unions, which have a global
   * namespace (it deliberately only traverses program scope), so this
   * cache is indexed by name.  If other declaration lookups were
   * added to it, it would have to be indexed by name and tag
   */
  mod_cu_type_cache_t global_alias_cache;
  static int global_alias_caching_callback(Dwarf_Die *die, bool has_inner_types,
                                           const std::string& prefix, void *arg);
  static int global_alias_caching_callback_cus(Dwarf_Die *die, void *arg);
  static int iterate_over_globals (Dwarf_Die *,
                                   int (* callback)(Dwarf_Die *, bool,
                                                    const std::string&, void *),
                                   void * data);
  static int iterate_over_types (Dwarf_Die *, bool, const std::string&,
                                 int (* callback)(Dwarf_Die *, bool,
                                                  const std::string&, void *),
                                 void * data);

  static int mod_function_caching_callback (Dwarf_Die* func, void *arg);
  static int cu_function_caching_callback (Dwarf_Die* func, void *arg);

  bool has_single_line_record (dwarf_query * q, char const * srcfile, int lineno);

  static void loc2c_error (void *, const char *fmt, ...) __attribute__ ((noreturn));

  // This function generates code used for addressing computations of
  // target variables.
  void emit_address (struct obstack *pool, Dwarf_Addr address);
  static void loc2c_emit_address (void *arg, struct obstack *pool,
                                  Dwarf_Addr address);

  void print_locals(std::vector<Dwarf_Die>& scopes, std::ostream &o);
  void print_locals_die(Dwarf_Die &die, std::ostream &o);
  void print_members(Dwarf_Die *vardie, std::ostream &o,
                     std::set<std::string> &dupes);

  Dwarf_Attribute *find_variable_and_frame_base (std::vector<Dwarf_Die>& scopes,
                                                 Dwarf_Addr pc,
                                                 std::string const & local,
                                                 const target_symbol *e,
                                                 Dwarf_Die *vardie,
                                                 Dwarf_Attribute *fb_attr_mem);

  struct location *translate_location(struct obstack *pool,
                                      Dwarf_Attribute *attr,
                                      Dwarf_Die *die,
                                      Dwarf_Addr pc,
                                      Dwarf_Attribute *fb_attr,
                                      struct location **tail,
                                      const target_symbol *e);

  bool find_struct_member(const target_symbol::component& c,
                          Dwarf_Die *parentdie,
                          Dwarf_Die *memberdie,
                          std::vector<Dwarf_Die>& dies,
                          std::vector<Dwarf_Attribute>& locs);

  void translate_components(struct obstack *pool,
                            struct location **tail,
                            Dwarf_Addr pc,
                            const target_symbol *e,
                            Dwarf_Die *vardie,
                            Dwarf_Die *typedie,
                            unsigned first=0);

  void translate_final_fetch_or_store (struct obstack *pool,
                                       struct location **tail,
                                       Dwarf_Addr module_bias,
                                       Dwarf_Die *vardie,
                                       Dwarf_Die *typedie,
                                       bool lvalue,
                                       const target_symbol *e,
                                       std::string &,
                                       std::string &,
                                       exp_type & ty);

  std::string express_as_string (std::string prelude,
                                 std::string postlude,
                                 struct location *head);

  regex_t blacklist_func; // function/statement probes
  regex_t blacklist_func_ret; // only for .return probes
  regex_t blacklist_file; // file name
  regex_t blacklist_section; // init/exit sections
  bool blacklist_enabled;
  void build_blacklist();
  std::string get_blacklist_section(Dwarf_Addr addr);

  // Returns the call frame address operations for the given program counter.
  Dwarf_Op *get_cfa_ops (Dwarf_Addr pc);

  Dwarf_Addr vardie_from_symtable(Dwarf_Die *vardie, Dwarf_Addr *addr);

  static int add_module_build_id_to_hash (Dwfl_Module *m,
                 void **userdata __attribute__ ((unused)),
                 const char *name,
                 Dwarf_Addr base,
                 void *arg);

public:
  Dwarf_Addr pr15123_retry_addr (Dwarf_Addr pc, Dwarf_Die* var);
};

#endif // DWFLPP_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
