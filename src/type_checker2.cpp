
func s_node* get_type_by_name(char* str)
{
	foreach(type_i, type, g_type_check_data.types)
	{
		if(type->ntype.name.equals(str)) { return type; }
	}
	return null;
}


func s_node* node_to_func(s_node* node)
{
	assert(node);
	switch(node->type)
	{
		case e_node_identifier:
		{
			foreach(f_i, f, g_type_check_data.funcs)
			{
				if(node->identifier.name.equals(&f->func_decl.name))
				{
					return f;
				}
			}
		} break;

		invalid_default_case;
	}

	assert(false);
	return null;
}


func s_node* node_to_var(s_node* node)
{
	switch(node->type)
	{
		case e_node_identifier:
		{
			for(int scope_i = g_type_check_data.curr_scope; scope_i >= 0; scope_i--)
			{
				for(int var_i = g_type_check_data.var_count2[scope_i] - 1; var_i >= 0; var_i--)
				{
					s_node* var = &g_type_check_data.vars2[scope_i][var_i];
					switch(var->type)
					{
						case e_node_var_decl:
						{
							if(node->identifier.name.equals(&var->var_decl.name))
							{
								return var;
							}
						} break;

						case e_node_func_arg:
						{
							if(node->identifier.name.equals(&var->func_arg.name))
							{
								return var;
							}
						} break;
					}
				}
			}
			return null;
		} break;

		invalid_default_case;
	}
	return null;
}

func s_node* node_to_type(s_node* node)
{
	switch(node->type)
	{
		case e_node_identifier:
		{
			assert(false); // @Note(tkap, 26/07/2023): Do we even need this case??
			foreach(type_i, type, g_type_check_data.types)
			{
				if(node->identifier.name.equals(&type->ntype.name))
				{
					return type;
				}
			}
			return null;
		} break;

		case e_node_type:
		{
			foreach(type_i, type, g_type_check_data.types)
			{
				if(node->ntype.name.equals(&type->ntype.name))
				{
					return type;
				}
			}
			return null;

		} break;

		case e_node_integer:
		{
			return get_type_by_name("int");
		} break;

		invalid_default_case;
	}
	return null;
}

func void type_check_expr(s_node* node, char* file, s_node* func_decl)
{
	// @TODO(tkap, 31/07/2023):
	switch(node->type)
	{
		case e_node_identifier:
		{
			s_node* var = node_to_var(node);
			assert(var);
			node->stack_offset = var->stack_offset;
		} break;

		case e_node_func_call:
		{
			for_node(n, node->func_call.args)
			{
				type_check_expr(n, file, null);
			}
			node->func_node = node_to_func(node->func_call.left);
			assert(node->func_node);
		} break;

		case e_node_integer:
		{
			node->type_node = get_type_by_name("int");
		} break;

		case e_node_add:
		{
			// @TODO(tkap, 31/07/2023): need to set type_node here?

			type_check_expr(node->arithmetic.left, file, null);
			type_check_expr(node->arithmetic.right, file, null);
		} break;

		invalid_default_case;
	}
}

func void type_check_statement(s_node* node, char* file, s_node* func_decl)
{
	switch(node->type)
	{
		case e_node_func_arg:
		{
			type_check_statement(node->func_arg.type, file, null);
			node->stack_offset = func_decl->func_decl.bytes_used_by_args;
			func_decl->func_decl.bytes_used_by_args += node->func_arg.type->size;

			add_var(*node);
		} break;

		case e_node_type:
		{
			s_node* type = node_to_type(node);
			assert(type);
			node->type_node = type;
			node->size = type->ntype.size_in_bytes;
		} break;

		case e_node_var_decl:
		{
			type_check_statement(node->var_decl.ntype, file, null);
			type_check_expr(node->var_decl.val, file, null);
			node->stack_offset = func_decl->func_decl.bytes_used_by_args + func_decl->func_decl.bytes_used_by_local_variables;
			func_decl->func_decl.bytes_used_by_local_variables += node->var_decl.ntype->size;

			add_var(*node);
		} break;

		case e_node_compound:
		{
			for_node(n, node->compound.statements)
			{
				type_check_statement(n, file, func_decl);
			}
		} break;

		case e_node_for:
		{
			type_check_expr(node->nfor.expr, file, null);
			node->stack_offset = func_decl->func_decl.bytes_used_by_args + func_decl->func_decl.bytes_used_by_local_variables;
			func_decl->func_decl.bytes_used_by_local_variables += 8; // @Fixme(tkap, 31/07/2023): just size of int?? not sure

			s_node new_node = zero;
			new_node.type = e_node_var_decl;
			new_node.stack_offset = node->stack_offset;
			new_node.var_decl.ntype = node_to_type(node->nfor.expr);
			if(node->nfor.name.len > 0)
			{
				new_node.var_decl.name = node->nfor.name;
			}
			else
			{
				new_node.var_decl.name.from_cstr("it");
			}

			type_check_push_scope();
			add_var(new_node);
			type_check_statement(node->nfor.body, file, func_decl);
			type_check_pop_scope();
		} break;

		case e_node_plus_equals:
		case e_node_times_equals:
		case e_node_assign:
		{
			type_check_expr(node->arithmetic.left, file, null);
			type_check_expr(node->arithmetic.right, file, null);
		} break;

		case e_node_return:
		{
			if(node->nreturn.expr)
			{
				type_check_expr(node->nreturn.expr, file, null);
			}
		} break;

		default:
		{
			type_check_expr(node, file, null);
		} break;

	}
}

func void type_check(s_node* ast, char* file)
{


	{
		s_node f = zero;
		f.type = e_node_func_decl;
		f.func_decl.external = true;
		f.func_decl.name.from_cstr("print");
		f.func_decl.arg_count = 1; // @Fixme(tkap, 26/07/2023):
		f.func_decl.id = g_type_check_data.next_func_id++;
		g_type_check_data.funcs.add(f);
	}

	// @TODO(tkap, 26/07/2023): This currently needs to be in the same order as the e_type enum
	{
		s_node type = zero;
		type.type = e_node_type;
		type.ntype.name.from_cstr("void");
		type.ntype.size_in_bytes = 0;
		type.ntype.id = g_type_check_data.next_type_id++;
		g_type_check_data.types.add(type);
	}

	{
		s_node type = zero;
		type.type = e_node_type;
		type.ntype.name.from_cstr("int");
		type.ntype.id = g_type_check_data.next_type_id++;
		type.ntype.size_in_bytes = 8;
		g_type_check_data.types.add(type);
	}

	{
		s_node type = zero;
		type.type = e_node_type;
		type.ntype.name.from_cstr("char");
		type.ntype.id = g_type_check_data.next_type_id++;
		type.ntype.size_in_bytes = 8;
		g_type_check_data.types.add(type);
	}

	{
		s_node type = zero;
		type.type = e_node_type;
		type.ntype.name.from_cstr("bool");
		type.ntype.id = g_type_check_data.next_type_id++;
		type.ntype.size_in_bytes = 8;
		g_type_check_data.types.add(type);
	}

	{
		s_node type = zero;
		type.type = e_node_type;
		type.ntype.name.from_cstr("u8");
		type.ntype.size_in_bytes = 8;
		type.ntype.id = g_type_check_data.next_type_id++;
		g_type_check_data.types.add(type);
	}


	for_node(node, ast)
	{
		switch(node->type)
		{
			case e_node_func_decl:
			{

				node->func_decl.id = g_type_check_data.next_func_id++;
				g_type_check_data.funcs.add(*node);

				type_check_push_scope();
				auto func_decl = &node->func_decl;
				for_node(arg, func_decl->args)
				{
					type_check_statement(arg, file, node);
				}
				type_check_statement(func_decl->body, file, node);
				type_check_pop_scope();
			} break;
		}
	}
}

func void type_check_push_scope()
{
	g_type_check_data.curr_scope += 1;
	g_type_check_data.var_count2[g_type_check_data.curr_scope] = 0;
}

func void type_check_pop_scope()
{
	g_type_check_data.var_count2[g_type_check_data.curr_scope] = 0;
	g_type_check_data.curr_scope -= 1;
}

func void add_var(s_node var)
{
	int* var_count = &g_type_check_data.var_count2[g_type_check_data.curr_scope];
	g_type_check_data.vars2[g_type_check_data.curr_scope][*var_count] = var;
	*var_count += 1;
}