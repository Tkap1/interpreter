
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

		case e_node_possible_type:
		{
			foreach(type_i, type, g_type_check_data.types)
			{
				if(node->possible_type.name.equals(&type->ntype.name))
				{
					return type;
				}
			}
			return null;

		} break;

		invalid_default_case;
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

func void type_check_expr(s_node* node, s_error_reporter* reporter, char* file)
{

	switch(node->type)
	{
		case e_node_identifier:
		{
			node->var_data = get_type_check_var_by_name(node->identifier.name);
		} break;

		case e_node_str:
		{
			node->var_data.type_node = get_type_by_name("char");
			node->var_data.pointer_level = 1;
		} break;

		case e_node_func_call:
		{
			s_node* func_node = node_to_func(node->func_call.left);
			assert(func_node);
			node->var_data.func_node = func_node;

			if(func_node->func_decl.arg_count != node->func_call.arg_count)
			{
				reporter->fatal(
					node->line, file, "Function '%s' expected %i arguments, but got %i",
					func_node->func_decl.name.data, func_node->func_decl.arg_count, node->func_call.arg_count
				);
			}

			for_node(arg, node->func_call.args)
			{
				type_check_expr(arg, reporter, file);
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
			type_check_expr(node->arithmetic.left, reporter, file);
			type_check_expr(node->arithmetic.right, reporter, file);

			s_type_instance left = get_type_instance(node->arithmetic.left);
			s_type_instance right = get_type_instance(node->arithmetic.right);

			if(
				get_type_id(left) != get_type_id(right) ||
				left.pointer_level != right.pointer_level
			)
			{
				reporter->fatal(
					node->line, file, "Can't TODO '%s' of type '%s' and '%s' of type '%s'",
					node_to_str(node->arithmetic.left), type_instance_to_str(left),
					node_to_str(node->arithmetic.right), type_instance_to_str(right)
				);
			}

		} break;

		case e_node_equals:
		{
			type_check_expr(node->arithmetic.left, reporter, file);
			type_check_expr(node->arithmetic.right, reporter, file);
		} break;

		case e_node_unary:
		{

			auto unary = &node->unary;
			switch(unary->type)
			{
				case e_unary_dereference:
				{
					type_check_expr(unary->expr, reporter, file);
				} break;

				case e_unary_address_of:
				{
					type_check_expr(unary->expr, reporter, file);
				} break;

				invalid_default_case;
			}
		} break;

		invalid_default_case;
	}
}

func void type_check_statement(s_node* node, s_error_reporter* reporter, char* file)
{

	switch(node->type)
	{
		case e_node_compound:
		{
			type_check_push_scope();
			for_node(statement, node->compound.statements)
			{
				type_check_statement(statement, reporter, file);
			}
			type_check_pop_scope();
		} break;

		case e_node_break:
		{
			// @TODO(tkap, 25/07/2023): We should check that we are inside a for loop
			// and that the value matches how many loops deep we are in
		} break;

		case e_node_continue:
		{
			// @TODO(tkap, 25/07/2023): We should check that we are inside a for loop
		} break;

		case e_node_var_decl:
		{
			node->var_data.id = g_type_check_data.next_var_id++;

			node->var_data.type_node = node_to_type(node->var_decl.ntype);
			if(!node->var_data.type_node)
			{
				reporter->fatal(
					node->line, file, "Variable '%s' has unknown type '%s'",
					node->var_decl.name.data, node->var_decl.type->possible_type.name.data
				);
			}
			node->var_data.pointer_level = node->var_decl.type->possible_type.pointer_level;

			// @Fixme(tkap, 27/07/2023): check if we have val
			type_check_expr(node->var_decl.val, reporter, file);

			s_type_check_var new_var = node->var_data;
			{
				new_var.name = node->var_decl.name;
				add_type_check_var(new_var);
			}

			{
				assert(new_var.type_node);
				s_type_instance left = {.pointer_level = new_var.pointer_level, .type = new_var.type_node};
				s_type_instance right = get_type_instance(node->var_decl.val);

				if(
					get_type_id(left) != get_type_id(right) ||
					left.pointer_level != right.pointer_level
				)
				{
					reporter->fatal(
						node->line, file, "Can't assign '%s' of type '%s' to '%s' of type '%s'",
						node_to_str(node->var_decl.val), type_instance_to_str(right),
						node->var_decl.name.data, type_instance_to_str(left)
					);
				}
			}

		} break;

		case e_node_return:
		{
			if(node->nreturn.expr)
			{
				type_check_expr(node->nreturn.expr, reporter, file);
			}
		} break;

		case e_node_for:
		{
			type_check_push_scope();

			// @Note(tkap, 24/07/2023): Add "it" variable
			{
				s_type_check_var var = zero;
				var.type_node = get_type_by_name("int");
				var.id = g_type_check_data.next_var_id++;
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

			type_check_expr(node->nfor.expr, reporter, file);

			for_node(arg, node->nfor.body)
			{
				type_check_statement(arg, reporter, file);
			}

			type_check_pop_scope();
		} break;

		case e_node_if:
		{
			type_check_push_scope();

			type_check_expr(node->nif.expr, reporter, file);

			for_node(arg, node->nif.body)
			{
				type_check_statement(arg, reporter, file);
			}

			type_check_pop_scope();
		} break;

		case e_node_plus_equals:
		case e_node_minus_equals:
		case e_node_times_equals:
		case e_node_assign:
		{
			type_check_expr(node->arithmetic.left, reporter, file);
			type_check_expr(node->arithmetic.right, reporter, file);

			s_type_instance left = get_type_instance(node->arithmetic.left);
			s_type_instance right = get_type_instance(node->arithmetic.right);

			if(
				get_type_id(left) != get_type_id(right) ||
				left.pointer_level != right.pointer_level
			)
			{
				reporter->fatal(
					node->line, file, "Can't TODO '%s' of type '%s' to '%s' of type '%s'",
					node_to_str(node->arithmetic.right), type_instance_to_str(right),
					node_to_str(node->arithmetic.left), type_instance_to_str(left)
				);
			}

		} break;

		default:
		{
			type_check_expr(node, reporter, file);
		} break;
	}
}

func b8 type_check_type(s_node* node)
{
	assert(node->type == e_node_possible_type);
	s_node* temp = node_to_type(node);
	assert(temp);
	node->var_data.id = temp->ntype.id;
	node->var_data.name = temp->ntype.name;
	node->var_data.type_node = temp;
	node->var_data.pointer_level = node->ntype.pointer_level;
	return true;
}

func b8 type_check_func_decl_arg(s_node* node)
{
	type_check_type(node->func_arg.type);
	node->var_data.name = node->func_arg.name;
	node->var_data.id = g_type_check_data.next_var_id++;
	// @Hack(tkap, 28/07/2023):
	node->var_data.type_node = node->func_arg.type->var_data.type_node;

	{
		s_type_check_var var = zero;
		var.id = node->var_data.id;
		var.type_node = node->func_arg.type->var_data.type_node;
		var.pointer_level = node->func_arg.type->var_data.pointer_level;
		var.name = node->func_arg.name;
		add_type_check_var(var);
	}
	return true;
}

func void type_check(s_node* ast, char* file)
{
	assert(ast);

	s_error_reporter reporter = zero;

	{
		s_node f = zero;
		f.type = e_node_func_decl;
		f.func_decl.external = true;
		f.func_decl.name.from_cstr("print");
		f.func_decl.arg_count = 1; // @Fixme(tkap, 26/07/2023):
		f.func_decl.id = g_type_check_data.next_func_id++;
		g_type_check_data.funcs.add(f);
	}


	// @TODO(tkap, 26/07/2023): This currently needs to be in the order as the e_type enum
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
				auto func_decl = &node->func_decl;
				node->func_decl.id = g_type_check_data.next_func_id++;
				g_type_check_data.funcs.add(*node);

				type_check_type(func_decl->return_type);

				// @Fixme(tkap, 26/07/2023): ???? do we need this??
				// if(!func_decl->external)
				{
					type_check_push_scope();
					for_node(arg, func_decl->args)
					{
						type_check_func_decl_arg(arg);
						// @TODO(tkap, 25/07/2023): Prevent duplicate argument names, including other arguments, globals, functions, structs, etc...

						func_decl->bytes_used_by_args += arg->var_data.type_node->ntype.size_in_bytes;
					}
					if(!func_decl->external)
					{
						type_check_statement(func_decl->body, &reporter, file);

						for_node(var_decl, func_decl->body->compound.statements)
						{
							if(var_decl->type == e_node_var_decl)
							{
								var_decl->var_data.stack_offset = func_decl->bytes_used_by_local_variables;

								// @Fixme(tkap, 28/07/2023): This doesn't really work for nested expressions
								var_decl->var_decl.val->var_data.stack_offset = func_decl->bytes_used_by_local_variables;

								func_decl->bytes_used_by_local_variables += var_decl->var_data.type_node->ntype.size_in_bytes;
							}
						}
					}
					type_check_pop_scope();
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
					type_check_type(member->struct_member.type);
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
}

func void add_type_check_var(s_type_check_var var)
{
	int* var_count = &g_type_check_data.var_count[g_type_check_data.curr_scope];
	g_type_check_data.vars[g_type_check_data.curr_scope][*var_count] = var;
	*var_count += 1;
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

func char* node_to_str(s_node* node)
{
	s_str_sbuilder<1024> builder;
	node_to_str_(node, &builder);
	return format_text("%s", builder.cstr());
}

func void node_to_str_(s_node* node, s_str_sbuilder<1024>* builder)
{
	switch(node->type)
	{
		case e_node_identifier:
		{
			builder->add("%s", node->identifier.name.data);
		} break;

		case e_node_integer:
		{
			builder->add("%lli", node->integer.val);
		} break;

		case e_node_unary:
		{
			auto unary = &node->unary;
			switch(unary->type)
			{
				case e_unary_address_of:
				{
					builder->add("&");
					node_to_str_(unary->expr, builder);
				} break;

				invalid_default_case;
			}

		} break;

		invalid_default_case;
	}
}

func s_type_instance get_type_instance(s_node* node)
{
	s_type_instance result = zero;
	switch(node->type)
	{
		case e_node_integer:
		{
			s_node* temp = get_type_by_name("int");
			result.type = temp;
		} break;

		case e_node_str:
		{
			result.type = node->var_data.type_node;
			result.pointer_level = node->var_data.pointer_level;
		} break;

		case e_node_identifier:
		{
			s_type_check_var var = get_type_check_var_by_name(node->identifier.name);
			result.type = var.type_node;
			result.pointer_level = var.pointer_level;
		} break;

		case e_node_add:
		case e_node_subtract:
		case e_node_multiply:
		case e_node_divide:
		case e_node_mod:
		{
			s_type_instance left = get_type_instance(node->arithmetic.left);
			s_type_instance right = get_type_instance(node->arithmetic.right);

			// @Fixme(tkap, 26/07/2023): shouldnt be an assert
			assert(left.type->ntype.id == right.type->ntype.id);
			assert(left.pointer_level == right.pointer_level);

			return left;
		} break;

		case e_node_unary:
		{
			auto unary = &node->unary;
			switch(unary->type)
			{
				case e_unary_dereference:
				{
					result = get_type_instance(unary->expr);
					result.pointer_level -= 1;
					assert(result.pointer_level >= 0);
				} break;

				case e_unary_address_of:
				{
					result = get_type_instance(unary->expr);
					result.pointer_level += 1;
					assert(result.pointer_level > 0);
				} break;

				invalid_default_case;
			}
		} break;

		case e_node_type:
		{
			result.pointer_level = node->var_data.pointer_level;

			// @TODO(tkap, 26/07/2023): maybe??
			result.type = node->var_data.type_node;
		} break;

		case e_node_func_arg:
		{
			result = get_type_instance(node->func_arg.type);
		} break;

		invalid_default_case;
	}
	return result;
}

func s_node* get_type_by_name(char* str)
{
	foreach(type_i, type, g_type_check_data.types)
	{
		if(type->ntype.name.equals(str)) { return type; }
	}
	return null;
}

func int get_type_id(s_type_instance type_instance)
{
	assert(type_instance.type);
	return type_instance.type->ntype.id;
}

func char* type_instance_to_str(s_type_instance type_instance)
{
	s_str_sbuilder<1024> builder;
	builder.add(type_instance.type->ntype.name.data);
	for(int i = 0; i < type_instance.pointer_level; i++)
	{
		builder.add("*");
	}
	return format_text("%s", builder.cstr());
}