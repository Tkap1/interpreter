
func s_node* node_to_struct_member(s_node* nstruct, s_node* node)
{
	assert(nstruct->type == e_node_struct);
	switch(node->type)
	{
		case e_node_identifier:
		{
			for_node(member, nstruct->nstruct.members)
			{
				if(node->identifier.name.equals(&member->struct_member.name))
				{
					return member;
				}
			}
			return null;
		} break;

		invalid_default_case;
	}
	return null;
}

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
				for(int var_i = g_type_check_data.var_count[scope_i] - 1; var_i >= 0; var_i--)
				{
					s_node* var = &g_type_check_data.vars[scope_i][var_i];
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
		case e_node_possible_type:
		{
			foreach(type_i, type, g_type_check_data.types)
			{
				if(node->possible_type.name.equals(&type->ntype.name))
				{
					return type;
				}
			}

			foreach(type_i, type, g_type_check_data.structs)
			{
				if(node->possible_type.name.equals(&type->nstruct.name))
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

		case e_node_add:
		{
			// @TODO(tkap, 31/07/2023): actual type
			return get_type_by_name("int");
		} break;

		invalid_default_case;
	}
	return null;
}

func void type_check_expr(s_node* node, char* file, s_error_reporter* reporter, s_node* func_decl)
{
	unreferenced(func_decl);

	switch(node->type)
	{
		case e_node_identifier:
		{
			s_node* var = node_to_var(node);
			assert(var);
			assert(var->type_node);
			node->type_node = var->type_node;
			node->stack_offset = var->stack_offset;
		} break;

		case e_node_func_call:
		{
			for_node(n, node->func_call.args)
			{
				type_check_expr(n, file, reporter, null);
			}
			node->func_node = node_to_func(node->func_call.left);
			assert(node->func_node);
			assert(node->func_node->func_decl.return_type->type_node);
			node->type_node = node->func_node->func_decl.return_type->type_node;
		} break;

		case e_node_integer:
		{
			node->type_node = get_type_by_name("int");
		} break;

		case e_node_float:
		{
			node->type_node = get_type_by_name("float");
		} break;

		case e_node_add:
		case e_node_subtract:
		case e_node_multiply:
		case e_node_divide:
		case e_node_mod:
		case e_node_equals:
		{
			// @TODO(tkap, 04/08/2023): We need to handle type promotion and all that shit
			// @TODO(tkap, 04/08/2023): Check that types make sense
			type_check_expr(node->arithmetic.left, file, reporter, null);
			type_check_expr(node->arithmetic.right, file, reporter, null);

			if(node->arithmetic.right->type_node->ntype.id == e_type_float && node->arithmetic.left->type_node->ntype.id == e_type_int)
			{
				node->type_node = node->arithmetic.right->type_node;
			}
			else
			{
				node->type_node = node->arithmetic.left->type_node;
			}
		} break;

		case e_node_member_access:
		{
			// @TODO(tkap, 31/07/2023): Check that left side is struct
			// @TODO(tkap, 31/07/2023): Check that right side exists in right side struct
			s_node* var = node_to_var(node->arithmetic.left);
			assert(var);
			s_node* type = var->type_node;
			assert(type);
			s_node* member = node_to_struct_member(type, node->arithmetic.right);
			assert(member);
			node->stack_offset = var->stack_offset + member->stack_offset;
			assert(member->struct_member.type->type_node);
			node->type_node = member->struct_member.type->type_node;

		} break;

		case e_node_str:
		{
			node->type_node = get_type_by_name("char");
			node->pointer_level = 1;
		} break;

		case e_node_greater_than:
		{
			type_check_expr(node->arithmetic.left, file, reporter, null);
			type_check_expr(node->arithmetic.right, file, reporter, null);
		} break;

		case e_node_unary:
		{
			switch(node->unary.type)
			{
				case e_unary_logical_not:
				{
					type_check_expr(node->unary.expr, file, reporter, null);
					node->type_node = node->unary.expr->type_node;
					// @TODO(tkap, 02/08/2023): check that this can be used as bool
				} break;

				case e_unary_cast:
				{
					type_check_expr(node->unary.expr, file, reporter, null);
					type_check_statement(node->unary.cast_type, file, reporter, null);
					node->type_node = node->unary.cast_type->type_node;
					// @TODO(tkap, 04/08/2023): Check that cast makes sense
				} break;

				invalid_default_case;
			}

		} break;

		invalid_default_case;
	}
}

func void type_check_statement(s_node* node, char* file, s_error_reporter* reporter, s_node* func_decl_or_struct)
{
	switch(node->type)
	{
		case e_node_struct_member:
		{
			// @Note(tkap, 31/07/2023): We may want to set node->struct_member.type.struct_node = func_decl_or_struct
			// or something like that. We'll see
			type_check_statement(node->struct_member.type, file, reporter, null);
			node->stack_offset = func_decl_or_struct->nstruct.bytes_used_by_members;
			assert(node->struct_member.type->type_node);
			node->type_node = node->struct_member.type->type_node;
			func_decl_or_struct->nstruct.bytes_used_by_members += node->type_node->ntype.size_in_bytes;
		} break;

		case e_node_func_arg:
		{
			type_check_statement(node->func_arg.type, file, reporter, null);
			assert(node->func_arg.type->type_node);
			node->type_node = node->func_arg.type->type_node;
			func_decl_or_struct->func_decl.bytes_used_by_args += node->type_node->ntype.size_in_bytes;
			node->stack_offset = -func_decl_or_struct->func_decl.bytes_used_by_args;

			add_var(*node);
		} break;

		case e_node_possible_type:
		{
			s_node* type = node_to_type(node);
			assert(type);
			node->type_node = type;
			node->pointer_level = node->possible_type.pointer_level;
		} break;

		case e_node_var_decl:
		{
			type_check_statement(node->var_decl.type, file, reporter, null);
			assert(node->var_decl.type->type_node);
			node->type_node = node->var_decl.type->type_node;

			if(node->var_decl.val)
			{
				type_check_expr(node->var_decl.val, file, reporter, null);
			}
			node->stack_offset = func_decl_or_struct->func_decl.bytes_used_by_args + func_decl_or_struct->func_decl.bytes_used_by_local_variables;
			assert(node->type_node->ntype.size_in_bytes > 0);
			func_decl_or_struct->func_decl.bytes_used_by_local_variables += node->type_node->ntype.size_in_bytes;

			add_var(*node);

			if(node->var_decl.val)
			{
				if(!left_can_have_right_assigned_to_it(node, node->var_decl.val))
				{
					reporter->error(
						node->line, file, "Can't assign '%s' of type '%s' to '%s' of type '%s'",
						expr_to_str(node->var_decl.val), type_to_str(node->var_decl.val->type_node),
						expr_to_str(node), type_to_str(node->type_node)
					);
				}
			}

		} break;

		case e_node_compound:
		{
			for_node(n, node->compound.statements)
			{
				type_check_statement(n, file, reporter, func_decl_or_struct);
			}
		} break;

		case e_node_for:
		{
			type_check_expr(node->nfor.expr, file, reporter, null);
			node->stack_offset = func_decl_or_struct->func_decl.bytes_used_by_args + func_decl_or_struct->func_decl.bytes_used_by_local_variables;
			func_decl_or_struct->func_decl.bytes_used_by_local_variables += 8; // @Fixme(tkap, 31/07/2023): just size of int?? not sure

			s_node new_node = zero;
			new_node.type = e_node_var_decl;
			new_node.stack_offset = node->stack_offset;
			new_node.var_decl.type = node_to_type(node->nfor.expr);
			new_node.type_node = get_type_by_name("int");
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
			type_check_statement(node->nfor.body, file, reporter, func_decl_or_struct);
			type_check_pop_scope();
		} break;

		case e_node_plus_equals:
		case e_node_minus_equals:
		case e_node_times_equals:
		case e_node_assign:
		{
			type_check_expr(node->arithmetic.left, file, reporter, null);
			type_check_expr(node->arithmetic.right, file, reporter, null);

			if(!left_can_have_right_assigned_to_it(node->arithmetic.left, node->arithmetic.right))
			{
				reporter->error(
					node->line, file, "Can't assign '%s' of type '%s' to '%s' of type '%s'",
					expr_to_str(node->arithmetic.right), type_to_str(node->arithmetic.right->type_node),
					expr_to_str(node->arithmetic.left), type_to_str(node->arithmetic.left->type_node)
				);
			}

		} break;

		case e_node_return:
		{
			if(node->nreturn.expr)
			{
				type_check_expr(node->nreturn.expr, file, reporter, null);
			}
		} break;

		case e_node_break:
		{
			// @TODO(tkap, 25/07/2023): We should check that we are inside a for loop
			// and that the value matches how many loops deep we are in
		} break;

		case e_node_if:
		{
			type_check_push_scope();

			type_check_expr(node->nif.expr, file, reporter, null);

			for_node(arg, node->nif.body)
			{
				type_check_statement(arg, file, reporter, func_decl_or_struct);
			}

			type_check_pop_scope();
		} break;

		default:
		{
			type_check_expr(node, file, reporter, null);
		} break;

	}
}

func b8 type_check(s_node* ast, char* file, b8 print_errors)
{

	s_error_reporter reporter = zero;

	// @Fixme(tkap, 02/08/2023): remove this DOG
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

	{
		s_node type = zero;
		type.type = e_node_type;
		type.ntype.name.from_cstr("float");
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

				type_check_push_scope();
				auto func_decl = &node->func_decl;
				type_check_statement(func_decl->return_type, file, &reporter, null);
				for_node(arg, func_decl->args)
				{
					type_check_statement(arg, file, &reporter, node);
				}

				if(!func_decl->external)
				{
					type_check_statement(func_decl->body, file, &reporter, node);
				}
				type_check_pop_scope();

				g_type_check_data.funcs.add(*node);

				if(reporter.has_error)
				{
					if(print_errors)
					{
						SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED);
						printf("%s\n", reporter.error_str);
						SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
					}
					return false;
				}

			} break;

			case e_node_struct:
			{
				auto nstruct = &node->nstruct;
				foreach_raw(f_i, f, g_type_check_data.funcs)
				{
					if(f.func_decl.name.equals(&nstruct->name))
					{
						reporter.fatal(
							node->line, file, "Struct '%s' cannot have that name because function '%s' already exists",
							nstruct->name.data, nstruct->name.data
						);
					}
				}

				foreach_raw(type_i, type, g_type_check_data.types)
				{
					if(type.ntype.name.equals(&nstruct->name))
					{
						reporter.fatal(
							node->line, file, "Struct '%s' cannot have that name because type '%s' already exists",
							nstruct->name.data, nstruct->name.data
						);
					}
				}

				foreach_raw(struct2_i, struct2, g_type_check_data.structs)
				{
					if(struct2.nstruct.name.equals(&nstruct->name))
					{
						reporter.fatal(
							node->line, file, "Struct '%s' already exists",
							nstruct->name.data
						);
					}
				}

				for_node(member, nstruct->members)
				{
					type_check_statement(member, file, &reporter, node);
				}

				// @TODO(tkap, 27/07/2023): check if any structs, functions, types, or globals have the same name
				// @TODO(tkap, 27/07/2023): Check the same for each struct member
				// @TODO(tkap, 27/07/2023): Check if any struct has the same name as any other
				// @TODO(tkap, 27/07/2023): Check if the types exists

				// @Fixme(tkap, 27/07/2023): We are not using next_struct_id
				g_type_check_data.structs.add(*node);
			} break;

			invalid_default_case;
		}
	}

	return true;
}

func void type_check_push_scope()
{
	g_type_check_data.curr_scope += 1;
	g_type_check_data.var_count[g_type_check_data.curr_scope] = 0;
}

func void type_check_pop_scope()
{
	g_type_check_data.var_count[g_type_check_data.curr_scope] = 0;
	g_type_check_data.curr_scope -= 1;
}

func void add_var(s_node var)
{
	int* var_count = &g_type_check_data.var_count[g_type_check_data.curr_scope];
	g_type_check_data.vars[g_type_check_data.curr_scope][*var_count] = var;
	*var_count += 1;
}

func char* expr_to_str(s_node* node)
{
	switch(node->type)
	{
		case e_node_integer:
		{
			return format_text("%lli", node->integer.val);
		} break;

		case e_node_float:
		{
			return format_text("%f", node->nfloat.val);
		} break;

		case e_node_member_access:
		{
			return format_text("%s.%s", expr_to_str(node->arithmetic.left), expr_to_str(node->arithmetic.right));
		} break;

		case e_node_identifier:
		{
			return format_text("%s", node->identifier.name.data);
		} break;

		case e_node_var_decl:
		{
			return format_text("%s", node->var_decl.name.data);
		} break;

		case e_node_subtract:
		{
			return format_text("%s - %s", expr_to_str(node->arithmetic.left), expr_to_str(node->arithmetic.right));
		} break;

		invalid_default_case;
	}
	return null;
}

func char* type_to_str(s_node* node)
{
	switch(node->type)
	{
		case e_node_type:
		{
			return node->ntype.name.data;
		} break;

		invalid_default_case;
	}
	return null;
}

func b8 left_can_have_right_assigned_to_it(s_node* left, s_node* right)
{
	assert(left->type_node);
	assert(right->type_node);

	s_node* tleft = left->type_node;
	s_node* tright = right->type_node;
	if(
		tleft->ntype.id != tright->ntype.id ||
		left->pointer_level != right->pointer_level
	)
	{
		if(tleft->ntype.id == e_type_u8 && tright->ntype.id == e_type_int)
		{
			return true;
		}
		// @Note(tkap, 04/08/2023): Allow "float foo = 10;"
		// if(
		// 	type_left->ntype.id == e_type_float && type_right->ntype.id == e_type_int &&
		// 	node->var_decl.val->type == e_node_integer
		// )
		// {
		// 	node->var_decl.val->type_node = get_type_by_name("float");
		// 	node->var_decl.val->type = e_node_float;
		// 	node->var_decl.val->nfloat.val = (float)node->var_decl.val->integer.val;
		// }
		// else
		{
			return false;
		}
	}
	return true;
}
