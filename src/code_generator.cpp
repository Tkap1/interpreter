
func s_gen_data generate_expr_address(s_node* node, int base_register)
{
	assert(base_register < e_register_count);

	s_gen_data result = zero;
	result.members = 1;
	if(node->type_node)
	{
		result.sizes[0] = get_size(node);
	}

	switch(node->type)
	{
		case e_node_member_access:
		case e_node_identifier:
		{
			if(node->type_node->type == e_node_struct)
			{
				result.members = node->type_node->nstruct.member_count;
				int i = 0;
				for_node(member, node->type_node->nstruct.members)
				{
					add_expr({.type = e_expr_immediate_to_reg, .a = {.val_s64 = base_register + i}, .b = {.val_s64 = node->stack_offset + member->stack_offset}});
					result.sizes[i] = get_size(member);
					i += 1;
				}
			}
			else
			{
				add_expr({.type = e_expr_immediate_to_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->stack_offset}});
			}
		} break;

		invalid_default_case;
	}

	return result;
}

func s_gen_data generate_expr(s_node* node, int base_register)
{
	s_gen_data result = zero;
	result.members = 1;
	result.need_compare = true;
	if(node->type_node)
	{
		result.sizes[0] = get_size(node);
	}
	assert(base_register < e_register_count);
	switch(node->type)
	{
		case e_node_func_call:
		{
			int i = 0;
			for_node(expr, node->func_call.args)
			{
				s_gen_data temp_data = generate_expr(expr, base_register + i);
				result.members = temp_data.members;
				for(int j = 0; j < temp_data.members; j++)
				{
					add_expr({.type = e_expr_push_reg, .a = {.val_s64 = base_register + i + j}, .b = {.val_s64 = temp_data.sizes[j]}});
				}
				i += temp_data.members;
			}
			if(node->func_node->func_decl.external)
			{
				// @TODO(tkap, 06/08/2023): Could there be a problem when functions return structs and use more than 1 register??
				add_expr({.type = e_expr_call_external, .a = {.val_s64 = node->func_node->func_decl.id}, .b = {.val_s64 = base_register}});
			}
			else
			{
				add_expr({.type = e_expr_call, .a = {.val_s64 = node->func_node->func_decl.id}});
			}
		} break;

		case e_node_str:
		{
			int index = g_code_gen_data.str_literals.add(node->str.val);
			add_expr({.type = e_expr_pointer_to_reg, .a = {.val_s64 = base_register}, .b = {.val_ptr = g_code_gen_data.str_literals[index].data}});
		} break;

		case e_node_add:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_add_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
		} break;

		case e_node_subtract:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);

			switch(node->arithmetic.right->type_node->ntype.id)
			{
				case e_type_float:
				{
					add_expr({.type = e_expr_sub_reg_reg_float, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
				} break;

				default:
				{
					add_expr({.type = e_expr_sub_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
				} break;
			}
		} break;

		case e_node_multiply:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);

			switch(node->arithmetic.right->type_node->ntype.id)
			{
				case e_type_float:
				{
					add_expr({.type = e_expr_multiply_reg_reg_float, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
				} break;

				default:
				{
					add_expr({.type = e_expr_multiply_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
				} break;
			}

		} break;

		case e_node_divide:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_divide_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
		} break;

		case e_node_mod:
		{
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_reg_mod_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
		} break;

		case e_node_integer:
		{
			add_expr({.type = e_expr_immediate_to_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->integer.val}});
		} break;

		case e_node_float:
		{
			add_expr({.type = e_expr_immediate_float_to_reg, .a = {.val_s64 = base_register}, .b = {.val_float = node->nfloat.val}});
		} break;

		case e_node_identifier:
		{
			if(node->type_node->type == e_node_struct)
			{
				result.members = node->type_node->nstruct.member_count;
				int i = 0;
				for_node(member, node->type_node->nstruct.members)
				{
					e_expr expr = adjust_expr_based_on_size(e_expr_var_to_reg_8, get_size(member));
					add_expr({.type = expr, .a = {.val_s64 = base_register + i}, .b = {.val_s64 = node->stack_offset + member->stack_offset}});
					result.sizes[i] = get_size(member);
					assert(result.sizes[i] > 0);
					i += 1;
				}
			}
			else
			{
				e_expr expr = adjust_expr_based_on_size(e_expr_var_to_reg_8, get_size(node));
				add_expr({.type = expr, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->stack_offset}});
			}
		} break;

		case e_node_equals:
		{
			result.comparison = e_node_equals;
			result.need_compare = false;
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_cmp_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
		} break;

		case e_node_greater_than:
		{
			result.comparison = e_node_greater_than;
			result.need_compare = false;
			generate_expr(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + 1);
			switch(node->arithmetic.left->type_node->ntype.id)
			{
				case e_type_float:
				{
					add_expr({.type = e_expr_cmp_reg_reg_float, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
				} break;

				default:
				{
					add_expr({.type = e_expr_cmp_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});
				} break;
			}
		} break;

		case e_node_unary:
		{
			auto unary = &node->unary;
			switch(unary->type)
			{
				case e_unary_dereference:
				{
					assert(false);
					// s64 var_id = get_var_id(unary->expr);
					// add_expr({.type = e_expr_var_to_reg_dereference, .a = {.val_s64 = base_register}, .b = {.val_s64 = var_id}});
				} break;

				case e_unary_address_of:
				{
					assert(false);
					// s64 var_id = get_var_id(unary->expr);
					// add_expr({.type = e_expr_lea_reg_var, .a = {.val_s64 = base_register}, .b = {.val_s64 = var_id}});
				} break;

				case e_unary_logical_not:
				{
					generate_expr(unary->expr, base_register);
					result.comparison = e_node_equals;
				} break;

				case e_unary_cast:
				{
					generate_expr(unary->expr, base_register);
					assert(unary->expr->type_node);
					switch(unary->cast_type->type_node->ntype.id)
					{
						case e_type_int:
						{
							switch(unary->expr->type_node->ntype.id)
							{
								case e_type_int:
								{
								} break;

								case e_type_float:
								{
									add_expr({.type = e_expr_reg_float_to_int, .a = {.val_s64 = base_register}});
								} break;

								invalid_default_case;
							}
						} break;

						case e_type_float:
						{
							switch(unary->expr->type_node->ntype.id)
							{
								case e_type_int:
								{
									add_expr({.type = e_expr_reg_int_to_float, .a = {.val_s64 = base_register}});
								} break;

								case e_type_float:
								{
								} break;

								invalid_default_case;
							}
						} break;

						invalid_default_case;
					}
				} break;

				invalid_default_case;

			}
		} break;

		case e_node_member_access:
		{
			switch(node->type_node->ntype.id)
			{
				case e_type_float:
				{
					e_expr expr = adjust_expr_based_on_size(e_expr_var_to_reg_float_32, get_size(node));
					add_expr({.type = expr, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->stack_offset}});
				} break;

				default:
				{
					e_expr expr = adjust_expr_based_on_size(e_expr_var_to_reg_8, get_size(node));
					add_expr({.type = expr, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->stack_offset}});
				} break;
			}
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
			if(node->var_decl.val)
			{
				generate_expr(node->var_decl.val, base_register);
				if(node->type_node->type == e_node_struct)
				{
					int i = 0;
					for_node(member, node->type_node->nstruct.members)
					{
						e_expr expr = adjust_expr_based_on_size(e_expr_reg_to_var_8, get_size(member));
						add_expr({.type = expr, .a = {.val_s64 = node->stack_offset + member->stack_offset}, .b = {.val_s64 = base_register + i}});
						i += 1;
					}
				}
				else
				{
					e_expr expr = adjust_expr_based_on_size(e_expr_reg_to_var_8, get_size(node));
					add_expr({.type = expr, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = base_register}});
				}
			}
			else
			{
				// @TODO(tkap, 04/08/2023): Take into account structs!
				if(node->type_node->type == e_node_struct)
				{
					for_node(member, node->type_node->nstruct.members)
					{
						add_expr({.type = e_expr_immediate_to_var, .a = {.val_s64 = node->stack_offset + member->stack_offset}, .b = {.val_s64 = 0}});
					}
				}
				else
				{
					add_expr({.type = e_expr_immediate_to_var, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = 0}});
				}
			}
		} break;

		case e_node_for:
		{
			auto nfor = node->nfor;
			auto for_expr = nfor.expr;

			// @Note(tkap, 24/07/2023): Create the loop variable
			// @Fixme(tkap, 31/07/2023):
			// s_var var = zero;
			// var.id = g_id++;
			// g_vars.add(var);

			int comparison_index = -1;
			int jump_index = -1;

			if(nfor.reverse)
			{
				generate_expr(for_expr, base_register + 1);
				add_expr({.type = e_expr_reg_dec, .a = {.val_s64 = base_register + 1}});

				{
					e_expr expr = adjust_expr_based_on_size(e_expr_reg_to_var_8, get_size(for_expr));
					add_expr({.type = expr, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = base_register + 1}});
				}

				comparison_index = add_expr(
					{.type = e_expr_cmp_var_immediate, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = 0}}
				);

				// @Note(tkap, 24/07/2023): Go to end of loop. We don't yet know to which instruction we have to jump, hence the -1
				jump_index = add_expr({.type = e_expr_jump_lesser, .a = {.val_s64 = -1}});
			}
			else
			{
				add_expr({.type = e_expr_immediate_to_var, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = 0}});

				// @TODO(tkap, 24/07/2023): If we can know the value of the comparand at compile time, then we just place it there.
				// Otherwise, we need to reference a variable
				generate_expr(for_expr, base_register + 1);
				{
					e_expr expr = adjust_expr_based_on_size(e_expr_cmp_var_reg_8, get_size(for_expr));
					comparison_index = add_expr(
						{.type = expr, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = base_register + 1}}
					);
				}

				// @Note(tkap, 24/07/2023): Go to end of loop. We don't yet know to which instruction we have to jump, hence the -1
				jump_index = add_expr({.type = e_expr_jump_greater_or_equal, .a = {.val_s64 = -1}});
			}

			// @Note(tkap, 24/07/2023): Do the for body
			generate_statement(nfor.body, base_register + 2);


			// @Note(tkap, 24/07/2023): Increment loop variable, go back to compare
			int inc_loop_index = add_expr({.type = e_expr_var_to_reg_32, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->stack_offset}});
			if(nfor.reverse)
			{
				add_expr({.type = e_expr_reg_dec, .a = {.val_s64 = base_register}});
			}
			else
			{
				add_expr({.type = e_expr_reg_inc, .a = {.val_s64 = base_register}});
			}

			add_expr({.type = e_expr_reg_to_var_32, .a = {.val_s64 = node->stack_offset}, .b = {.val_s64 = base_register}});
			int temp_index = add_expr({.type = e_expr_jump, .a = {.val_s64 = comparison_index}});

			foreach(break_index_i, break_index, g_code_gen_data.break_indices)
			{
				assert(g_exprs[break_index->index].a.val_s64 == -1);
				break_index->val -= 1;
				if(break_index->val <= 0)
				{
					g_exprs[break_index->index].a.val_s64 = temp_index + 1;
					g_code_gen_data.break_indices.remove_and_swap(break_index_i--);
				}
			}

			foreach_raw(continue_index_i, continue_index, g_code_gen_data.continue_indices)
			{
				assert(g_exprs[continue_index].a.val_s64 == -1);
				g_exprs[continue_index].a.val_s64 = inc_loop_index;
			}
			g_code_gen_data.continue_indices.count = 0;

			// @Note(tkap, 24/07/2023): Now we modify the jump instruction that is supposed to take us to the end of the for body
			g_exprs[jump_index].a.val_s64 = temp_index + 1;

		} break;

		case e_node_if:
		{
			s_gen_data gen_data = generate_expr(node->nif.expr, base_register);
			int jump_index = 0;
			if(gen_data.need_compare)
			{
				switch(node->nif.expr->type_node->ntype.id)
				{
					case e_type_float:
					{
						add_expr({.type = e_expr_cmp_reg_immediate_float, .a = {.val_s64 = base_register}, .b = {.val_float = 0.0f}});
					} break;

					default:
					{
						add_expr({.type = e_expr_cmp_reg_immediate, .a = {.val_s64 = base_register}, .b = {.val_s64 = 0}});
					} break;
				}
			}
			switch(gen_data.comparison)
			{
				case e_node_equals:
				{
					jump_index = add_expr({.type = e_expr_jump_not_equal, .a = {.val_s64 = -1}});
				} break;

				case e_node_not_equals:
				{
					jump_index = add_expr({.type = e_expr_jump_equal, .a = {.val_s64 = -1}});
				} break;

				case e_node_greater_than:
				{
					jump_index = add_expr({.type = e_expr_jump_less_or_equal, .a = {.val_s64 = -1}});
				} break;

				default:
				{
					jump_index = add_expr({.type = e_expr_jump_equal, .a = {.val_s64 = -1}});
				} break;
			}
			generate_statement(node->nif.body, base_register);
			g_exprs[jump_index].a.val_s64 = g_exprs.count;
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
				// @Note(tkap, 27/07/2023): Not sure
				// generate_expr(node->nreturn.expr, base_register);
				generate_expr(node->nreturn.expr, e_register_eax);
			}
			add_expr({.type = e_expr_return});
		} break;

		case e_node_assign:
		{
			s_gen_data data = generate_expr_address(node->arithmetic.left, base_register);
			generate_expr(node->arithmetic.right, base_register + data.members + 1);

			for(int i = 0; i < data.members; i++)
			{
				e_expr expr = adjust_expr_based_on_size(e_expr_reg_to_var_from_reg_8, data.sizes[i]);
				add_expr({.type = expr, .a = {.val_s64 = base_register + i}, .b = {.val_s64 = base_register + data.members + i + 1}});
			}

		} break;

		case e_node_plus_equals:
		{
			generate_expr(node->arithmetic.right, base_register);

			// @TODO(tkap, 04/08/2023): Need to do this for other operations
			switch(node->arithmetic.right->type_node->ntype.id)
			{
				case e_type_float:
				{
					add_expr({.type = e_expr_add_reg_to_var_float, .a = {.val_s64 = node->arithmetic.left->stack_offset}, .b = {.val_s64 = base_register}});
				} break;

				default:
				{
					e_expr expr = adjust_expr_based_on_size(e_expr_add_reg_to_var_8, get_size(node->arithmetic.left));
					add_expr({.type = expr, .a = {.val_s64 = node->arithmetic.left->stack_offset}, .b = {.val_s64 = base_register}});
				} break;
			}
		} break;

		case e_node_minus_equals:
		{
			generate_expr(node->arithmetic.right, base_register);

			switch(node->arithmetic.right->type_node->ntype.id)
			{
				case e_type_float:
				{
					add_expr({.type = e_expr_sub_reg_from_var_float, .a = {.val_s64 = node->arithmetic.left->stack_offset}, .b = {.val_s64 = base_register}});
				} break;

				default:
				{
					add_expr({.type = e_expr_sub_reg_from_var, .a = {.val_s64 = node->arithmetic.left->stack_offset}, .b = {.val_s64 = base_register}});
				} break;
			}
		} break;

		case e_node_times_equals:
		{
			{
				e_expr expr = adjust_expr_based_on_size(e_expr_var_to_reg_8, get_size(node->arithmetic.left));
				add_expr({.type = expr, .a = {.val_s64 = base_register}, .b = {.val_s64 = node->arithmetic.left->stack_offset}});
			}

			generate_expr(node->arithmetic.right, base_register + 1);
			add_expr({.type = e_expr_multiply_reg_reg, .a = {.val_s64 = base_register}, .b = {.val_s64 = base_register + 1}});

			{
				e_expr expr = adjust_expr_based_on_size(e_expr_reg_to_var_8, get_size(node->arithmetic.left));
				add_expr({.type = expr, .a = {.val_s64 = node->arithmetic.left->stack_offset}, .b = {.val_s64 = base_register}});
			}
		} break;

		case e_node_break:
		{
			int index = add_expr({.type = e_expr_jump, .a = {.val_s64 = -1}});
			s_break_index break_index = zero;
			break_index.val = node->nbreak.val;
			break_index.index = index;
			g_code_gen_data.break_indices.add(break_index);
		} break;

		case e_node_continue:
		{
			int index = add_expr({.type = e_expr_jump, .a = {.val_s64 = -1}});
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
	add_expr({.type = e_expr_call, .a = {.val_s64 = -1}});
	add_expr({.type = e_expr_return});

	for_node(node, ast)
	{
		switch(node->type)
		{
			case e_node_func_decl:
			{
				auto func_decl = &node->func_decl;
				if(func_decl->external)
				{
					s_func f = zero;
					f.id = node->func_decl.id;
					f.return_type.pointer_level = node->func_decl.return_type->pointer_level;
					assert(node->func_decl.return_type->type_node);
					f.return_type.type = node->func_decl.return_type->type_node;

					// @Fixme(tkap, 26/07/2023): I don't think these types match
					// f.return_type.type = (e_type)return_type.type->ntype.id;

					for_node(arg, func_decl->args)
					{
						s_type new_arg = zero;
						new_arg.pointer_level = arg->func_arg.type->pointer_level;
						// new_arg.type = (e_type)arg->func_arg.type->type_node->ntype.id;
						new_arg.type = arg->func_arg.type->type_node;
						f.args.add(new_arg);
					}

					if(node->func_decl.dll_str.len > 0)
					{
						b8 already_loaded = false;
						foreach_raw(dll_i, dll, g_code_gen_data.loaded_dlls)
						{
							if(dll.name.equals(&node->func_decl.dll_str))
							{
								already_loaded = true;
								f.ptr = GetProcAddress(dll.handle, node->func_decl.name.data);
								assert(f.ptr);
								break;
							}
						}
						if(!already_loaded)
						{
							s_dll new_dll = zero;
							new_dll.name = node->func_decl.dll_str;
							new_dll.handle = LoadLibrary(node->func_decl.dll_str.data);
							assert(new_dll.handle);
							g_code_gen_data.loaded_dlls.add(new_dll);
							f.ptr = GetProcAddress(new_dll.handle, node->func_decl.name.data);
							assert(f.ptr);
						}
					}
					else
					{
						HMODULE dll = GetModuleHandle(null);
						f.ptr = GetProcAddress(dll, node->func_decl.name.data);
						assert(f.ptr);
					}

					g_code_gen_data.external_funcs.add(f);
				}
				else
				{
					g_func_first_expr_index[func_decl->id] = g_exprs.count;
					add_expr({.type = e_expr_set_stack_base});
					add_expr({.type = e_expr_add_stack_pointer, .a = {.val_s64 = func_decl->bytes_used_by_local_variables}});
					if(func_decl->name.equals("main"))
					{
						g_exprs[0].a.val_s64 = func_decl->id;
					}


					generate_statement(func_decl->body, e_register_eax);
					add_expr({.type = e_expr_return});
				}
			} break;
		}
	}
}

func int add_expr(s_expr expr)
{
	return g_exprs.add(expr);
}

func e_expr adjust_expr_based_on_size(e_expr type, int size)
{
	int extra = 0;
	if(size == 1) {}
	else if(size == 2) { extra = 1; }
	else if(size == 4) { extra = 2; }
	else if(size == 8) { extra = 3; }
	invalid_else;

	switch(type)
	{
		case e_expr_var_to_reg_8:
		case e_expr_var_to_reg_16:
		case e_expr_var_to_reg_32:
		case e_expr_var_to_reg_64:
		{
			return (e_expr)(e_expr_var_to_reg_8 + extra);
		} break;

		case e_expr_reg_to_var_8:
		case e_expr_reg_to_var_16:
		case e_expr_reg_to_var_32:
		case e_expr_reg_to_var_64:
		{
			return (e_expr)(e_expr_reg_to_var_8 + extra);
		} break;

		case e_expr_var_to_reg_float_32:
		case e_expr_var_to_reg_float_64:
		{
			if(size == 4) { return e_expr_var_to_reg_float_32; }
			else if(size == 8) { return e_expr_var_to_reg_float_64; }
			invalid_else;
		} break;

		case e_expr_reg_to_var_float_32:
		case e_expr_reg_to_var_float_64:
		{
			if(size == 4) { return e_expr_reg_to_var_float_32; }
			else if(size == 8) { return e_expr_reg_to_var_float_64; }
			invalid_else;
		} break;

		case e_expr_cmp_var_reg_8:
		case e_expr_cmp_var_reg_16:
		case e_expr_cmp_var_reg_32:
		case e_expr_cmp_var_reg_64:
		{
			return (e_expr)(e_expr_cmp_var_reg_8 + extra);
		} break;

		case e_expr_add_reg_to_var_8:
		case e_expr_add_reg_to_var_16:
		case e_expr_add_reg_to_var_32:
		case e_expr_add_reg_to_var_64:
		{
			return (e_expr)(e_expr_add_reg_to_var_8 + extra);
		} break;

		case e_expr_reg_to_var_from_reg_8:
		case e_expr_reg_to_var_from_reg_16:
		case e_expr_reg_to_var_from_reg_32:
		case e_expr_reg_to_var_from_reg_64:
		{
			return (e_expr)(e_expr_reg_to_var_from_reg_8 + extra);
		} break;

		invalid_default_case;
	}
	return (e_expr)0;
}
