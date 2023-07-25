
func s_type_check_var get_type_check_var_by_name(s_str<64> name)
{
	// @Note(tkap, 24/07/2023): We go backwards so "it" finds the lastest for loop index, instead of the first!
	for(int scope_i = g_type_check_data.curr_scope; scope_i >= 0; scope_i--)
	{
		for(int var_i = g_type_check_data.var_count[scope_i] - 1; var_i >= 0; var_i--)
		{
			s_type_check_var var = g_type_check_data.vars[scope_i][var_i];
			if(name.equals(&var.name))
			{
				return var;
			}
		}
	}
	assert(false);
	return zero;
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

func void type_check_expr(s_node* node)
{

	switch(node->type)
	{
		case e_node_identifier:
		{
			node->var_data = get_type_check_var_by_name(node->identifier.name);
		} break;

		case e_node_func_call:
		{
			s_node* func_node = node_to_func(node->func_call.left);
			assert(func_node);

			for_node(arg, node->func_call.args)
			{
				type_check_expr(arg);
			}
			// @Fixme(tkap, 24/07/2023):
			// node->var_data = get_type_check_var_by_name(node->identifier.name);
		} break;

		case e_node_integer:
		{

		} break;

		case e_node_add:
		case e_node_subtract:
		case e_node_multiply:
		case e_node_divide:
		case e_node_mod:
		{
			type_check_expr(node->arithmetic.left);
			type_check_expr(node->arithmetic.right);
		} break;

		case e_node_equals:
		{
			type_check_expr(node->arithmetic.left);
			type_check_expr(node->arithmetic.right);
		} break;

		invalid_default_case;
	}
}

func void type_check_statement(s_node* node)
{

	switch(node->type)
	{
		case e_node_compound:
		{
			for_node(statement, node->compound.statements)
			{
				type_check_statement(statement);
			}
		} break;

		case e_node_var_decl:
		{
			node->var_data.id = g_type_check_data.next_id++;

			type_check_expr(node->var_decl.val);

			{
				s_type_check_var var = zero;
				var.id = node->var_data.id;
				var.name = node->var_decl.name;
				add_type_check_var(var);
			}
		} break;

		case e_node_func_call:
		{
			for_node(arg, node->func_call.args)
			{
				type_check_expr(arg);
			}
		} break;

		case e_node_return:
		{
			if(node->nreturn.expr)
			{
				type_check_expr(node->nreturn.expr);
			}
		} break;

		case e_node_for:
		{
			g_type_check_data.curr_scope += 1;

			// @Note(tkap, 24/07/2023): Add "it" variable
			{
				s_type_check_var var = zero;
				var.id = g_type_check_data.next_id++;
				if(node->nfor.name.len > 0)
				{
					var.name = node->nfor.name;
				}
				else
				{
					var.name.from_cstr("it");
				}
				add_type_check_var(var);
			}

			type_check_expr(node->nfor.expr);

			for_node(arg, node->nfor.body)
			{
				type_check_statement(arg);
			}
			g_type_check_data.curr_scope -= 1;
		} break;

		case e_node_if:
		{
			g_type_check_data.curr_scope += 1;

			type_check_expr(node->nif.expr);

			for_node(arg, node->nif.body)
			{
				type_check_statement(arg);
			}
			g_type_check_data.curr_scope -= 1;
		} break;

		case e_node_plus_equals:
		case e_node_times_equals:
		case e_node_assign:
		{
			type_check_expr(node->arithmetic.left);
			type_check_expr(node->arithmetic.right);
		} break;

		default:
		{
			type_check_expr(node);
		} break;
	}
}

func void type_check(s_node* ast)
{
	assert(ast);

	{
		s_node f = zero;
		f.type = e_node_func_decl;
		f.func_decl.name.from_cstr("print");
		g_type_check_data.funcs.add(f);
	}

	for_node(node, ast)
	{
		type_check_statement(node);
	}
}

func void add_type_check_var(s_type_check_var var)
{
	int* var_count = &g_type_check_data.var_count[g_type_check_data.curr_scope];
	g_type_check_data.vars[g_type_check_data.curr_scope][*var_count] = var;
	*var_count += 1;
}