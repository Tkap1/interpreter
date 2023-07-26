
func s_gen_data generate_expr(s_node* node, int base_register)
{
	s_gen_data result = zero;
	assert(base_register < e_register_count);
	switch(node->type)
	{
		case e_node_func_call:
		{
			// @Fixme(tkap, 25/07/2023):
			if(node->var_data.func_node->func_decl.name.equals("print"))
			{
				if(node->func_call.args->type == e_node_integer)
				{
					add_expr({.type = e_expr_print_immediate, .a = {.val = node->func_call.args->integer.val}});
				}
				else if(node->func_call.args->type == e_node_identifier)
				{
					add_expr({.type = e_expr_print_var, .a = {.val = node->func_call.args->var_data.id}});
				}
				invalid_else;
			}
			else
			{
				int i = 0;
				for_node(expr, node->func_call.args)
				{
					generate_expr(expr, base_register + i);
					add_expr({.type = e_expr_push_reg, .a = {.val = base_register + i}});
					i += 1;
				}
				add_expr({.type = e_expr_call, .a = {.val = node->var_data.func_node->func_decl.id}});
			}
		} break;

		case e_node_add:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_add_reg_reg, .a = {.val = base_register}, .b = {.val = base_register + 1}});
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
			auto nfor = node->nfor;
			auto expr = nfor.expr;

			// @Note(tkap, 24/07/2023): Create the loop variable
			s_var var = zero;
			var.id = g_id++;
			g_vars.add(var);

			int comparison_index = -1;
			int jump_index = -1;

			if(nfor.reverse)
			{
				generate_expr(expr, base_register + 1);
				add_expr({.type = e_expr_register_dec, .a = {.val = base_register + 1}});
				add_expr({.type = e_expr_register_to_var, .a = {.val = var.id}, .b = {.val = base_register + 1}});

				comparison_index = add_expr(
					{.type = e_expr_cmp_var_immediate, .a = {.val = var.id}, .b = {.val = 0}}
				);

				// @Note(tkap, 24/07/2023): Go to end of loop. We don't yet know to which instruction we have to jump, hence the -1
				jump_index = add_expr({.type = e_expr_jump_lesser, .a = {.val = -1}});
			}
			else
			{
				add_expr({.type = e_expr_immediate_to_var, .a = {.val = var.id}, .b = {.val = 0}});

				// @TODO(tkap, 24/07/2023): If we can know the value of the comparand at compile time, then we just place it there.
				// Otherwise, we need to reference a variable
				generate_expr(expr, base_register + 1);
				comparison_index = add_expr(
					{.type = e_expr_cmp_var_register, .a = {.val = var.id}, .b = {.val = base_register + 1}}
				);

				// @Note(tkap, 24/07/2023): Go to end of loop. We don't yet know to which instruction we have to jump, hence the -1
				jump_index = add_expr({.type = e_expr_jump_greater_or_equal, .a = {.val = -1}});
			}

			// @Note(tkap, 24/07/2023): Do the for body
			generate_statement(nfor.body, base_register + 2);

			// @Note(tkap, 24/07/2023): Increment loop variable, go back to compare
			int inc_loop_index = add_expr(var_to_register((e_register)(base_register), var.id));
			if(nfor.reverse)
			{
				add_expr({.type = e_expr_register_dec, .a = {.val = base_register}});
			}
			else
			{
				add_expr({.type = e_expr_register_inc, .a = {.val = base_register}});
			}

			add_expr({.type = e_expr_register_to_var, .a = {.val = var.id}, .b = {.val = base_register}});
			int temp_index = add_expr({.type = e_expr_jump, .a = {.val = comparison_index}});

			foreach(break_index_i, break_index, g_code_gen_data.break_indices)
			{
				assert(g_exprs[break_index->index].a.val == -1);
				break_index->val -= 1;
				if(break_index->val <= 0)
				{
					g_exprs[break_index->index].a.val = temp_index + 1;
					g_code_gen_data.break_indices.remove_and_swap(break_index_i--);
				}
			}

			foreach_raw(continue_index_i, continue_index, g_code_gen_data.continue_indices)
			{
				assert(g_exprs[continue_index].a.val == -1);
				g_exprs[continue_index].a.val = inc_loop_index;
			}
			g_code_gen_data.continue_indices.count = 0;

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

		case e_node_return:
		{
			if(node->nreturn.expr)
			{
				generate_expr(node->nreturn.expr, e_register_eax);
			}
			add_expr({.type = e_expr_return});
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
			add_expr({.type = e_expr_imul2_reg_reg, .a = {.val = base_register}, .b = {.val = base_register + 1}});
			add_expr({.type = e_expr_register_to_var, .a = {.val = node->arithmetic.left->var_data.id}, .b = {.val = base_register}});
		} break;

		case e_node_break:
		{
			int index = add_expr({.type = e_expr_jump, .a = {.val = -1}});
			s_break_index break_index = zero;
			break_index.val = node->nbreak.val;
			break_index.index = index;
			g_code_gen_data.break_indices.add(break_index);
		} break;

		case e_node_continue:
		{
			int index = add_expr({.type = e_expr_jump, .a = {.val = -1}});
			g_code_gen_data.continue_indices.add(index);
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

	// @Note(tkap, 25/07/2023): Call main. We still need to figure out where the first instruction of main is
	add_expr({.type = e_expr_call, .a = {.val = -1}});
	add_expr({.type = e_expr_return});

	for_node(node, ast)
	{
		switch(node->type)
		{
			case e_node_func_decl:
			{
				auto func_decl = &node->func_decl;

				if(func_decl->name.equals("main"))
				{
					g_exprs[0].a.val = func_decl->id;
				}

				g_func_first_expr_index[func_decl->id] = g_exprs.count;

				for_node(arg, func_decl->args)
				{
					s_var var = zero;
					var.id = g_id++;
					g_vars.add(var);
				}

				// @Note(tkap, 26/07/2023): Pop into arguments in reverse
				for(int i = 0; i < func_decl->arg_count; i++)
				{
					int index = g_vars.count - 1 - i;
					assert(index >= 0);
					add_expr({.type = e_expr_pop_var, .a = {.val = index}});
				}

				generate_statement(func_decl->body, e_register_eax);
				add_expr({.type = e_expr_return});

			} break;
		}
	}
}

func int add_expr(s_expr expr)
{
	return g_exprs.add(expr);
}
