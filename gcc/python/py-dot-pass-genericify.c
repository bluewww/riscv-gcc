/* This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3, or (at your option) any later
   version.

   GCC is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING3.  If not see
   <http://www.gnu.org/licenses/>.  */

#include "config.h"
#include "system.h"
#include "ansidecl.h"
#include "coretypes.h"
#include "tm.h"
#include "opts.h"
#include "tree.h"
#include "tree-iterator.h"
#include "tree-pass.h"
#include "gimple.h"
#include "toplev.h"
#include "debug.h"
#include "options.h"
#include "flags.h"
#include "convert.h"
#include "diagnostic-core.h"
#include "langhooks.h"
#include "langhooks-def.h"
#include "target.h"
#include "cgraph.h"

#include <gmp.h>
#include <mpfr.h>
#include <vec.h>
#include <hashtab.h>

#include "gpython.h"
#include "py-il-dot.h"
#include "py-il-tree.h"
#include "py-vec.h"
#include "py-runtime.h"

static tree gpy_dot_pass_genericify_toplevl_functor_decl (gpy_dot_tree_t * const,
							  VEC(gpy_context_t, gc) *);
static VEC(tree,gc) * gpy_dot_pass_generificify_toplevl_class_decl (gpy_dot_tree_t * const,
								    VEC(gpy_context_t, gc) *,
								    VEC(tree,gc) * );
static tree gpy_dot_pass_generificify_class_method_attrib (gpy_dot_tree_t *, const char *,
							   VEC(gpy_context_t, gc) *);

static void gpy_dot_pass_genericify_setup_runtime_globls (gpy_hash_tab_t * const, VEC(tree,gc) *);
static void gpy_dot_genericify_create_offsets_globl_context (tree, tree *, gpy_hash_tab_t * const,
							     gpy_hash_tab_t * const);
static tree gpy_dot_pass_generificify_scalar (gpy_dot_tree_t *, tree *);

char * gpy_dot_pass_genericify_gen_concat (const char * s1,
					   const char * s2)
{
  size_t s1len = strlen (s1);
  size_t s2len = strlen (s2);
  size_t tlen = s1len + s2len;
  
  char buffer[tlen+3]; 
  char * p;
  for (p = buffer; *s1 != '\0'; ++s1)
    {
      *p = *s1;
      ++p;
    }
  *p = '.';
  p++;
  for (; *s2 != '\0'; ++s2)
    {
      *p = *s2;
      ++p;
    }
  *p = '\0';

  return xstrdup (buffer);
}

static
tree gpy_dot_pass_generificify_get_module_type (const char * s, 
						gpy_hash_tab_t * modules)
{
  tree retval = error_mark_node;

  gpy_hashval_t h = gpy_dd_hash_string (s);
  gpy_hash_entry_t * e = gpy_dd_hash_lookup_table (modules, h);
  if (e)
    {
      if (e->data)
        retval = (tree) e->data;
    }

  return retval;
}

static
void gpy_dot_pass_genericify_setup_runtime_globls (gpy_hash_tab_t * context,
						   VEC(tree,gc) * decls)
{
  tree decl = build_decl (BUILTINS_LOCATION, VAR_DECL,
			  get_identifier (GPY_RR_globl_stack),
			  gpy_unsigned_char_ptr);
  TREE_STATIC (decl) = 0;
  TREE_PUBLIC (decl) = 1;
  TREE_USED (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;

  VEC_safe_push (tree, gc, decls, decl);

  gcc_assert (!gpy_dd_hash_insert (gpy_dd_hash_string (GPY_RR_globl_stack),
				   decl, context)
	      );

  decl = build_decl (BUILTINS_LOCATION, VAR_DECL,
		     get_identifier (GPY_RR_globl_stack_pointer),
		     gpy_object_type_ptr_ptr);
  TREE_STATIC (decl) = 0;
  TREE_PUBLIC (decl) = 1;
  TREE_USED (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;

  VEC_safe_push (tree, gc, decls, decl);

  gcc_assert (!gpy_dd_hash_insert (gpy_dd_hash_string (GPY_RR_globl_stack_pointer),
				   decl, context)
	      );
}

static
void gpy_dot_pass_genericify_create_offsets_globl_context (tree type, tree * cblock,
							   VEC(gpy_context_t, gc) * context)
{
  tree extend_stack = GPY_RR_extend_globl_stack (TYPE_SIZE (type));
  append_to_statement_list (extend_stack, block);

  gpy_context_t * globls = VEC_index (gpy_context_t, context, 0);
  gpy_context_t * globls_symbols = VEC_index (gpy_context_t, context, 1);
  gpy_hash_entry_t * e = gpy_dd_hash_lookup_table (globls,
						   gpy_dd_hash_string (GPY_RR_globl_stack_pointer));
  gcc_assert (e);
  tree stack_pointer = (tree)e->data;
  VEC(tree,gc) * agregate_types = VEC_alloc (tree,gc,0);

  if (globls_symbols->array)
    {
      int i;
      gpy_hash_entry_t *array = globls_symbols->array;
      for (i = 0; i < size; ++i)
	{
	  e = array[i];
	  if (e->data)
	    {
	      gpy_symbol_t * sym = e->data;
	      tree T = sym->type;

	      bool unique = true;
	      int idx; tree it = NULL_TREE;
	      for (idx = 0; VEC_iterate (tree,agregate_types, idx, it); ++idx)
		{
		  if (it == T)
		    unique = false;
		}
	      if (unique)
		VEC_safe_push (tree, gc, agregate_types, sym->type);
	    }
	}
    }

  if (VEC_length (tree, agregate_types) > 0)
    {
      int idx, current_offset = 0;
      tree it = NULL_TREE;
      for (idx = 0; VEC_iterate (tree, agregate_types, idx, it); ++idx)
	{
	  tree field;
	  for (field = TYPE_FIELDS (type); field != NULL_TREE;
	       field = DECL_CHAIN (field)
	       )
	    {
	      
	    }
    }

  tree field;
  for (field = TYPE_FIELDS (type); field != NULL_TREE;
       field = DECL_CHAIN (field)
       )
    {
      gcc_assert (TREE_CODE (field) == FIELD_DECL);
      const char * ident = IDENTIFIER_POINTER (DECL_NAME (field));
      debug ("calculating globl stack offset for <%s> from type <%s>!\n",
	     ident, IDENTIFIER_POINTER (TYPE_NAME (type)));

      tree offset = DECL_FIELD_OFFSET (field);
      /*
             *((gpy_object_t **)stack_pointer - offset)
       */
      tree offs = build2 (MINUS_EXPR, gpy_object_type_ptr_ptr, stack_pointer, offset);
      tree accessor = build_fold_indirect_ref (offs);
      
      gpy_symbol_t * Sym = xmalloc (sizeof (gpy_symbol_t));
      Sym->offset = accessor;
      Sym->field = field;
      Sym->type = type;

      gcc_assert (!gpy_dd_hash_insert (gpy_dd_hash_insert (ident), Sym,
				       globls_symbols));
    }
}

/* 
   Creates a DECL_CHAIN of stmts to fold the scalar 
   with the last tree being the address of the primitive 
*/
static
tree gpy_dot_pass_genericify_scalar (gpy_dot_tree_t * decl, tree * cblock)
{
  tree retval = error_mark_node;

  gcc_assert (DOT_TYPE (decl) == D_PRIMITIVE);
  gcc_assert (DOT_lhs_T (decl) == D_TD_COM);

  switch (DOT_lhs_TC (decl)->T)
    {
    case D_T_INTEGER:
      {
        retval = build_decl (UNKNOWN_LOCATION, VAR_DECL, create_tmp_var_name ("P"),
                             gpy_object_type_ptr);
        append_to_statement_list (build2 (MODIFY_EXPR, gpy_object_type_ptr, retval,
                                          GPY_RR_fold_integer (build_int_cst (integer_type_node,
									      DOT_lhs_TC (decl)->o.integer)
							       )),
                                  cblock);
      }
      break;

    default:
      error ("invalid scalar type!\n");
      break;
    }

  return retval;
}

static
tree gpy_dot_pass_genericify_toplevl_functor_decl (gpy_dot_tree_t * decl)
{
  tree fntype = build_function_type_list (void_type_node,
					  /* Arguments .. */
					  NULL_TREE);
  tree ident = GPY_dot_pass_generificify_gen_concat_identifier (GPY_current_module_name,
								DOT_IDENTIFIER_POINTER (DOT_FIELD (decl))
								);
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering toplevel function <%s> to <%s>!\n", DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl(BUILTINS_LOCATION, RESULT_DECL,
			    NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();

  /*
    lower the function suite here and append all initilization
  */
  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  return fndecl;
}

static
tree gpy_dot_pass_genericify_class_method_attrib (gpy_dot_tree_t * decl,
						    const char * parent_ident)
{
  tree fntype = build_function_type_list (void_type_node,
					  /* Arguments .. */
					  NULL_TREE);
  tree ident = GPY_dot_pass_generificify_gen_concat_identifier (GPY_current_module_name,
								parent_ident);
  ident = GPY_dot_pass_generificify_gen_concat_identifier (IDENTIFIER_POINTER (ident),
							   DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)));

  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering class attribute <%s> to <%s>!\n", DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl(BUILTINS_LOCATION, RESULT_DECL,
			    NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();
  /*
    lower the function suite here and append all initilization
  */
  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  return fndecl;
}

static
VEC(tree,gc) * gpy_dot_pass_genericify_toplevl_class_decl (gpy_dot_tree_t * decl)
{
  VEC(tree,gc) * lowered_decls = VEC_alloc (tree,gc,0);

  tree fntype = build_function_type_list (void_type_node,
					  /* Arguments .. */
					  NULL_TREE);
  tree ident = GPY_dot_pass_genericify_gen_concat_identifier (GPY_current_module_name,
								DOT_IDENTIFIER_POINTER (DOT_FIELD (decl))
								);
  ident = GPY_dot_pass_generificify_gen_concat_identifier (IDENTIFIER_POINTER (ident),
							   "__field_init__");
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering toplevel class <%s> to <%s>!\n", DOT_IDENTIFIER_POINTER (DOT_FIELD (decl)),
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl(BUILTINS_LOCATION, RESULT_DECL,
			    NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();

  /*
    lower the function suite here and append all initilization
  */
  gpy_dot_tree_t * class_suite = DOT_lhs_TT (decl);
  gpy_dot_tree_t * node = class_suite;
  do {
    switch (DOT_TYPE (node))
      {
      case D_STRUCT_METHOD:
	{
	  tree a = gpy_dot_pass_genericify_class_method_attrib (node,
								DOT_IDENTIFIER_POINTER (DOT_FIELD (decl))
								);
	  VEC_safe_push (tree, gc, lowered_decls, a);
	}
	break;

      default:
	break;
      }
  } while ((node = DOT_CHAIN (node)));
  
  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  VEC_safe_push (tree, gc, lowered_decls, fndecl);

  return lowered_decls;
}

static
VEC(tree,gc) * gpy_dot_pass_generificify_TU (gpy_hash_tab_t * const modules,
					     VEC(gpydot,gc) * const decls)
{
  VEC(tree,gc) * lowered_decls = VEC_alloc (tree, gc, 0);
  
  gpy_hash_tab_t toplvl, topnxt;
  gpy_dd_hash_init_table (&toplvl);
  gpy_dd_hash_init_table (&topnxt);

  tree main_init_module = gpy_dot_pass_genericify_get_module_type (GPY_current_module_name,
								   modules);
  tree pdecl_t = build_pointer_type (main_init_module);

  tree fntype = build_function_type_list (void_type_node,
					  pdecl_t,
					  NULL_TREE);
  tree ident = GPY_dot_pass_genericify_gen_concat_identifier (GPY_current_module_name,
								"__field_init__");
  tree fndecl = build_decl (BUILTINS_LOCATION, FUNCTION_DECL, ident, fntype);
  debug ("lowering toplevel module <%s> to <%s>!\n", GPY_current_module_name,
	 IDENTIFIER_POINTER (ident));

  TREE_STATIC (fndecl) = 1;
  TREE_PUBLIC (fndecl) = 1;
  
  tree argslist = NULL_TREE;
  tree self_parm_decl = build_decl (BUILTINS_LOCATION, PARM_DECL,
                                    get_identifier ("__self__"),
                                    pdecl_t);
  DECL_CONTEXT (self_parm_decl) = fndecl;
  DECL_ARG_TYPE (self_parm_decl) = TREE_VALUE (TYPE_ARG_TYPES (TREE_TYPE (fndecl)));
  TREE_READONLY (self_parm_decl) = 1;
  tree arglist = chainon (arglist, self_parm_decl);

  TREE_USED (self_parm_decl) = 1;
  DECL_ARGUMENTS (fndecl) = arglist;

  /* Define the return type (represented by RESULT_DECL) for the main functin */
  tree resdecl = build_decl (BUILTINS_LOCATION, RESULT_DECL,
			     NULL_TREE, TREE_TYPE(fntype));
  DECL_CONTEXT (resdecl) = fndecl;
  DECL_ARTIFICIAL (resdecl) = true;
  DECL_IGNORED_P (resdecl) = true;

  DECL_RESULT (fndecl) = resdecl;

  /* This is usually used for debugging purpose. this is currently unused */
  tree block = build_block (NULL_TREE, NULL_TREE, NULL_TREE, NULL_TREE);
  DECL_INITIAL (fndecl) = block;

  tree stmts = alloc_stmt_list ();

  VEC(gpy_context_t,gc) * context = VEC_alloc (gpy_context_t, gc, 0);
  VEC_safe_push (gpy_context_t, gc, context, &toplvl);
  VEC_safe_push (gpy_context_t, gc, context, &topnxt);

  gpy_dot_pass_genericify_setup_runtime_globls (&toplvl);
  gpy_dot_pass_genericify_create_offsets_globl_context (main_init_module, &stmts,
							&topnxt, &toplvl);
  int idx;
  gpy_dot_tree_t * idtx = NULL_DOT;
  /*
    Iterating over the DOT IL to lower/generate the GENERIC code
    required to compile the stmts and decls
  */
  for (idx = 0; VEC_iterate (gpydot, decls, idx, idtx); ++idx)
    {
      if (DOT_T_FIELD (idtx) ==  D_D_EXPR)
	{
	  // append to stmt list as this goes into the module initilizer...
          // gpy_stmt_decl_lower_expr (idtx, &stmts, context);
          continue;
	}

      switch (DOT_TYPE (idtx))
        {
	case D_PRINT_STMT:
	  {
	    gpy_dot_pass_genericify_print_stmt (idtx, context, &block);
	  }
	  break;

        case D_STRUCT_METHOD:
          {
	    tree t = gpy_dot_pass_genericify_toplevl_functor_decl (idtx, context);
	    VEC_safe_push (tree, gc, lowered_decls, t);
          }
          break;

	case D_STRUCT_CLASS:
	  {
	    VEC(tree,gc) * cdecls = gpy_dot_pass_genericify_toplevl_class_decl (idtx, context,
										modules);
	    GPY_VEC_stmts_append (tree, lowered_decls, cdecls);
	  }
	  break;

        default:
          fatal_error ("unhandled dot tree code <%i>!\n", DOT_TYPE (idtx));
          break;
        }
    }

  tree declare_vars = resdecl;

  tree bind = NULL_TREE;
  tree bl = build_block (declare_vars, NULL_TREE, fndecl, NULL_TREE);
  DECL_INITIAL (fndecl) = bl;
  TREE_USED (bl) = 1;

  bind = build3 (BIND_EXPR, void_type_node, BLOCK_VARS(bl),
		 NULL_TREE, bl);
  TREE_SIDE_EFFECTS (bind) = 1;

  /* Finalize the main function */
  BIND_EXPR_BODY(bind) = stmts;
  stmts = bind;
  DECL_SAVED_TREE (fndecl) = stmts;
  
  gimplify_function_tree (fndecl);
  cgraph_finalize_function (fndecl, false);

  VEC_safe_push (tree, gc, lowered_decls, fndecl);

  return lowered_decls;
}

VEC(tree,gc) * gpy_dot_pass_genericify (VEC(tree,gc) *modules,
					VEC(gpydot,gc) *decls)
{
  VEC(tree,gc) * retval = VEC_alloc (tree, gc, 0);

  gpy_hash_tab_t module_ctx;
  gpy_dd_hash_init_table (&module_ctx);

  int idx;
  tree itx = NULL_TREE;
  for (idx = 0; VEC_iterate (tree, modules, idx, itx); ++idx)
    {
      debug ("hashing module name <%s>!\n", IDENTIFIER_POINTER (TYPE_NAME(itx)));
      gpy_hashval_t h = gpy_dd_hash_string (IDENTIFIER_POINTER (TYPE_NAME(itx)));
      void ** e = gpy_dd_hash_insert (h, itx, &module_ctx);
      
      if (e)
        error ("module <%s> is already defined!\n",
	       IDENTIFIER_POINTER (DECL_NAME (itx))
	       );
    }

  debug ("genericification...!\n");
  retval = gpy_dot_pass_genericify_TU (&module_ctx, decls);
  debug ("finishing genericification!\n");

  if (module_ctx.array)
    free (module_ctx.array);

  return retval;
}
