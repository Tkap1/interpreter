
func s_gen_data generate_expr(s_node* node, int base_register)
{
	s_gen_data result = zero;
	assert(base_register < e_register_count);
	switch(node->type)
	{
		case e_node_func_call:
		{
			if(node->func_call.args->type == e_node_integer)
			{
				add_expr({.type = e_expr_print, .a = {.operand = e_operand_immediate, .val = node->func_call.args->integer.val}});
			}
			else if(node->func_call.args->type == e_node_identifier)
			{
				add_expr({.type = e_expr_print, .a = {.operand = e_operand_var, .val = node->func_call.args->var_data.id}});
			}
			invalid_else;
		} break;

		case e_node_add:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_register_add, .a = {.val = base_register}, .b = {.operand = e_operand_register, .val = base_register + 1}});
		} break;

		case e_node_divide:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_divide_reg_reg, .a = {.val = base_register}, .b = {.val = base_register + 1}});
		} break;

		case e_node_mod:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_register_mod_register, .a = {.val = base_register}, .b = {.val = base_register + 1}});
		} break;

		case e_node_integer:
		{
			add_expr({.type = e_expr_val_to_register, .a = {.val = base_register}, .b = {.val = node->integer.val}});
		} break;

		case e_node_identifier:
		{
			add_expr(var_to_register((e_register)base_register, node->var_data.id));
		} break;

		case e_node_equals:
		{
			result.comparison = e_node_equals;
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_cmp_reg_reg, .a = {.val = base_register}, .b = {.val = base_register + 1}});
		} break;

		invalid_default_case;
	}

	return result;
}

func void generate_statement(s_node* node, int base_register)
{
	switch(node->type)
	{
		case e_node_var_decl:
		{
			s_var var = zero;
			var.id = g_id++;
			g_vars.add(var);

			if(node->var_decl.val)
			{
				generate_expr(node->var_decl.val, base_register);
				add_expr({.type = e_expr_register_to_var, .a = {.val = var.id}, .b = {.val = base_register}});
			}
		} break;

		case e_node_for:
		{
			auto expr = node->nfor.expr;

			// @Note(tkap, 24/07/2023): Create the loop variable
			s_var var = zero;
			var.id = g_id++;
			g_vars.add(var);

			add_expr({.type = e_expr_immediate_to_var, .a = {.val = var.id}, .b = {.val = 0}});

			// @Fixme(tkap, 24/07/2023): If we can know the value of the comparand at compile time, then we just place it there.
			// Otherwise, we need to reference a variable
			generate_expr(expr, base_register + 1);
			int comparison_index = add_expr(
				{.type = e_expr_cmp_var_register, .a = {.val = var.id}, .b = {.val = base_register + 1}}
			);

			// @Note(tkap, 24/07/2023): Go to end of loop. We don't yet know to which instruction we have to jump, hence the -1
			int jump_index = add_expr({.type = e_expr_jump_greater_or_equal, .a = {.val = -1}});

			// @Note(tkap, 24/07/2023): Do the for body
			generate_statement(node->nfor.body, base_register + 2);

			// @Note(tkap, 24/07/2023): Increment loop variable, go back to compare
			add_expr(var_to_register((e_register)(base_register), var.id));
			add_expr({.type = e_expr_register_inc, .a = {.val = base_register}});
			add_expr({.type = e_expr_register_to_var, .a = {.val = var.id}, .b = {.val = base_register}});
			int temp_index = add_expr({.type = e_expr_jump, .a = {.val = comparison_index}});

			// @Note(tkap, 24/07/2023): Now we modify the jump instruction that is supposed to take us to the end of the for body
			g_exprs[jump_index].a.val = temp_index + 1;

		} break;

		case e_node_if:
		{
			s_gen_data gen_data = generate_expr(node->nif.expr, base_register);
			int jump_index = 0;
			switch(gen_data.comparison)
			{
				case e_node_equals:
				{
					jump_index = add_expr({.type = e_expr_jump_not_equal, .a = {.val = -1}});
				} break;

				invalid_default_case;
			}
			generate_statement(node->nif.body, base_register);
			g_exprs[jump_index].a.val = g_exprs.count;
		} break;

		case e_node_compound:
		{
			for_node(statement, node->compound.statements)
			{
				generate_statement(statement, base_register);
			}
		} break;

		case e_node_assign:
		{
			generate_expr(node->arithmetic.right, base_register);
			add_expr({.type = e_expr_register_to_var, .a = {.val = node->arithmetic.left->var_data.id}, .b = {.val = base_register}});
		} break;

		case e_node_plus_equals:
		{
			generate_expr(node->arithmetic.right, base_register);
			add_expr({.type = e_expr_add_register_to_var, .a = {.val = node->arithmetic.left->var_data.id}, .b = {.val = base_register}});
		} break;

		case e_node_times_equals:
		{
			add_expr(var_to_register(base_register, node->arithmetic.left->var_data.id));
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_imul2, .a = {.val = base_register}, .b = {.operand = e_operand_register, .val = base_register + 1}});
			add_expr({.type = e_expr_register_to_var, .a = {.val = node->arithmetic.left->var_data.id}, .b = {.val = base_register}});
		} break;

		default:
		{
			generate_expr(node, base_register);
		} break;
	}
}

func void generate_code(s_node* ast)
{
	assert(ast);

	for_node(node, ast)
	{
		generate_statement(node, e_register_eax);
	}
}

func int add_expr(s_expr expr)
{
	return g_exprs.add(expr);
}
