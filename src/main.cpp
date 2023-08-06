
#include "dyncall.h"

#include "tklib.h"
#include "parser.h"
#include "type_checker.h"
#include "code_generator.h"
#include "main.h"

global constexpr s64 c_return_value = -1234;

global s64 g_id = 0;
global s_sarray<int, 1024> g_call_stack;
global s_sarray<s_expr, 1024> g_exprs;
global s_carray<int, 1024> g_func_first_expr_index;
global s64 g_instruction_pointer = 0;
global s_carray<s_register, e_register_count> g_registers;
global e_flag g_flag;
global volatile int g_input_edited;
global s_lin_arena g_arena;
global s_type_check_data g_type_check_data = zero;
global s_code_gen_data g_code_gen_data;
global s_code_exec_data g_code_exec_data;
global HANDLE stdout_handle;
global DCCallVM* g_vm;

#include "parser.cpp"
#include "type_checker2.cpp"
#include "code_generator.cpp"

int main(int argc, char** argv)
{
	argc -= 1;
	argv += 1;

	g_vm = dcNewCallVM(4096);
	dcMode(g_vm, DC_CALL_C_DEFAULT);

	g_arena = make_lin_arena(1 * c_mb, false);
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	if(argc >= 1 && strcmp(argv[0], "test") == 0)
	{
		do_tests();
		return 0;
	}

	CreateThread(null, 0, watch_for_file_changes, null, 0, null);

	while(true)
	{
		reset_globals();

		s_tokenizer tokenizer = zero;
		my_strcpy(tokenizer.comment_str, sizeof(tokenizer.comment_str), "//");
		tokenizer.at = read_file_quick("input.tk", &g_arena);
		if(tokenizer.at)
		{
			s_node* ast = parse(tokenizer, "input.tk");
			if(!type_check(ast, "input.tk", true))
			{
				exit(1);
			}
			generate_code(ast);
		}

		while(g_instruction_pointer < g_exprs.count)
		{
			g_instruction_pointer = execute_expr(g_exprs[g_instruction_pointer]);
			if(g_instruction_pointer == c_return_value) { break; }
		}

		#ifdef m_show_asm
		printf("------\n");
		print_exprs();
		printf("@@@@@@\n");
		#endif // m_verbose

		while(true)
		{
			if(g_input_edited)
			{
				// @Note(tkap, 24/07/2023): Let's wait for a bit to hopefully get rid of all the repeats that windows sends
				Sleep(100);

				if(InterlockedCompareExchange((LONG*)&g_input_edited, 0, 1) == 1)
				{
					system("cls");
					break;
				}
			}
			Sleep(50);
		}
	}

	SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);

	return 0;
}

func void do_tests()
{
	system("cls");
	struct s_test_data
	{
		char* file;
		b8 should_fail_compilation;
		int expected_result;
	};

	constexpr s_test_data c_tests[] = {
		{.file = "tests/factorial.tk", .expected_result = 3628800},
		{.file = "tests/fibonacci.tk", .expected_result = 55},
		{.file = "tests/prime.tk", .expected_result = 79},
		{.file = "tests/break2.tk", .expected_result = 2},
		{.file = "tests/return_val1.tk", .expected_result = 11},
		{.file = "tests/return_val2.tk", .expected_result = 10},
		{.file = "tests/struct1.tk", .expected_result = 5},
		{.file = "tests/struct2.tk", .expected_result = 4},
		{.file = "tests/struct3.tk", .expected_result = 6},
		{.file = "tests/struct4.tk", .expected_result = 255},
		{.file = "tests/struct5.tk", .expected_result = 0},
		{.file = "tests/struct6.tk", .expected_result = 2},
		{.file = "tests/bool1.tk", .expected_result = 88},
		{.file = "tests/bool2.tk", .expected_result = 11},
		{.file = "tests/zero_init.tk", .expected_result = 0},
		{.file = "tests/zero_init_struct.tk", .expected_result = 0},
		{.file = "tests/float_into_int.tk", .should_fail_compilation = true},
		{.file = "tests/cast_float_to_int.tk", .expected_result = 1234},
		{.file = "tests/cast1.tk", .expected_result = 333},
		{.file = "tests/cast2.tk", .expected_result = 456},
		{.file = "tests/add_to_float.tk", .expected_result = 1235},
		{.file = "tests/float_func.tk", .expected_result = 7},
	};

	for(int test_i = 0; test_i < array_count(c_tests); test_i++)
	{
		SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);

		s_test_data test = c_tests[test_i];
		s_parse_file_and_execute_result result = parse_file_and_execute(test.file, false);

		#ifdef m_verbose
		printf("------\n");
		print_exprs();
		printf("@@@@@@\n");
		#endif

		b8 pass = true;
		if(test.should_fail_compilation && result.compilation_succeded) { pass = false; }
		else if(!test.should_fail_compilation && test.expected_result != result.return_val) { pass = false; }

		if(pass)
		{
			SetConsoleTextAttribute(stdout_handle, FOREGROUND_GREEN);
			printf("TEST %i %s PASSED!\n", test_i + 1, test.file);
		}
		else
		{
			SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED);
			printf("\n--------------------\n\n");
			printf("TEST %i %s FAILED!\n", test_i + 1, test.file);
			if(!result.compilation_succeded)
			{
				printf("Did not compile\n");
			}
			else
			{
				printf("Expected %i but got %lli\n", test.expected_result, result.return_val);
			}
			printf("\n");
			printf("--------------------\n\n");
		}
	}
	SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
}

func s_parse_file_and_execute_result parse_file_and_execute(char* file, b8 print_errors)
{
	s_parse_file_and_execute_result result = zero;

	reset_globals();

	s_tokenizer tokenizer = zero;
	my_strcpy(tokenizer.comment_str, sizeof(tokenizer.comment_str), "//");
	tokenizer.at = read_file_quick(file, &g_arena);
	if(!tokenizer.at) { return result; }

	s_node* ast = parse(tokenizer, file);
	if(!ast) { return result; }
	if(!type_check(ast, file, print_errors)) { return result; }
	generate_code(ast);

	while(g_instruction_pointer < g_exprs.count)
	{
		g_instruction_pointer = execute_expr(g_exprs[g_instruction_pointer]);
		if(g_instruction_pointer == c_return_value) { break; }
	}

	result.compilation_succeded = true;
	result.return_val = g_registers[e_register_eax].val_s64;
	return result;
}


func s64 execute_expr(s_expr expr)
{
	s64 result = g_instruction_pointer + 1;
	switch(expr.type)
	{

		case e_expr_reg_to_address:
		{
			s64* val = (s64*)&g_code_exec_data.stack[g_registers[expr.a.val_s64].val_s64];

			dprint(
				"deref [stack_base + %lli](%lli) = %s(%lli)\n",
				g_registers[expr.a.val_s64].val_s64, *val,
				register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);

			*val = g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_pointer_to_reg:
		{
			// dprint("lea %s var%lli\n", register_to_str(expr.a.val_s64), expr.b.val_s64);
			dprint("%s = %p\n", register_to_str(expr.a.val_s64), expr.b.val_ptr);
			g_registers[expr.a.val_s64].val_ptr = expr.b.val_ptr;
		} break;

		case e_expr_call:
		{
			g_call_stack.add(result);
			dprint("call %lli\n", expr.a.val_s64);
			result = g_func_first_expr_index[expr.a.val_s64];
		} break;

		case e_expr_call_external:
		{
			s_func f = get_func_by_id(expr.a.val_s64);

			dprint("call external %lli\n", expr.a.val_s64);

			dcReset(g_vm);

			s_sarray<int, 128> offsets;
			int temp_offset = g_code_exec_data.stack_pointer;
			// foreach_raw(arg_i, arg, f.args)
			for(int arg_i = f.args.count - 1; arg_i >= 0; arg_i--)
			{
				s_type arg = f.args[arg_i];
				if(arg.pointer_level > 0)
				{
					temp_offset -= 8;
					offsets.insert(0, temp_offset);
				}
				else
				{
					switch(arg.type->ntype.id)
					{
						case e_type_int:
						{
							temp_offset -= 4;
							offsets.insert(0, temp_offset);
						} break;

						case e_type_float:
						{
							temp_offset -= 4;
							offsets.insert(0, temp_offset);
						} break;

						case e_type_char:
						{
							temp_offset -= 1;
							offsets.insert(0, temp_offset);
						} break;

						default:
						{
							assert(arg.type->type == e_node_struct);
							auto nstruct = arg.type->nstruct;
							for_node(member, nstruct.members)
							{
								temp_offset -= get_size(member);
								offsets.insert(0, temp_offset);
							}
						} break;
					}
				}
			}
			g_code_exec_data.stack_pointer = temp_offset;
			assert(g_code_exec_data.stack_pointer >= 0);

			int offset_index = 0;
			foreach_raw(arg_i, arg, f.args)
			{
				void* val = (void*)&g_code_exec_data.stack[offsets[offset_index]];

				if(arg.pointer_level > 0)
				{
					char* temp = *(char**)val;
					dcArgPointer(g_vm, temp);
				}
				else
				{
					switch(arg.type->ntype.id)
					{
						case e_type_int:
						{
							dcArgInt(g_vm, *(int*)val);
						} break;

						case e_type_float:
						{
							dcArgFloat(g_vm, *(float*)val);
						} break;

						case e_type_char:
						{
							dcArgChar(g_vm, *(char*)val);
						} break;

						default:
						{
							assert(arg.type->type == e_node_struct);

							auto nstruct = arg.type->nstruct;

							u8 data[1024];
							u8* cursor = data;

							int i = 0;
							DCaggr* dcs = dcNewAggr(4, 4);
							dcAggrField(dcs, DC_SIGCHAR_UCHAR, 0, 1);
							dcAggrField(dcs, DC_SIGCHAR_UCHAR, 1, 1);
							dcAggrField(dcs, DC_SIGCHAR_UCHAR, 2, 1);
							dcAggrField(dcs, DC_SIGCHAR_UCHAR, 3, 1);

							for_node(member, nstruct.members)
							{
								// if(i == 0) { dcStructField(dcs, DC_SIGCHAR_UCHAR, offsetof(s_color, r), 1); }
								// if(i == 1) { dcStructField(dcs, DC_SIGCHAR_UCHAR, offsetof(s_color, g), 1); }
								// if(i == 2) { dcStructField(dcs, DC_SIGCHAR_UCHAR, offsetof(s_color, b), 1); }
								// if(i == 3) { dcStructField(dcs, DC_SIGCHAR_UCHAR, offsetof(s_color, a), 1); }
								// dcStructField(dcs, DC_SIGCHAR_UCHAR, DEFAULT_ALIGNMENT, 0);

								val = (void*)&g_code_exec_data.stack[offsets[offset_index]];
								switch(member->struct_member.type->type_node->ntype.id)
								{
									case e_type_u8:
									{
										*cursor = *(u8*)val;
										cursor += 1;
									} break;
									invalid_default_case;
								}
								offset_index += 1;
								i += 1;
							}
							dcCloseAggr(dcs);
							dcArgAggr(g_vm, dcs, data);
							dcFreeAggr(dcs);

							// @Hack(tkap, 01/08/2023): Because we increment at the end
							offset_index -= 1;
						} break;
					}
				}
				offset_index += 1;
			}
			// @TODO(tkap, 26/07/2023): return value
			switch(f.return_type.type->ntype.id)
			{
				case e_type_void:
				{
					dcCallVoid(g_vm, (DCpointer)f.ptr);
				} break;

				case e_type_int:
				{
					s64 func_result = dcCallInt(g_vm, (DCpointer)f.ptr);
					g_registers[expr.b.val_s64].val_s64 = 0;
					g_registers[expr.b.val_s64].val_s32 = func_result;
				} break;

				case e_type_float:
				{
					float func_result = dcCallFloat(g_vm, (DCpointer)f.ptr);
					g_registers[expr.b.val_s64].val_s64 = 0;
					g_registers[expr.b.val_s64].val_float = func_result;
				} break;

				case e_type_bool:
				{
					s64 func_result = dcCallBool(g_vm, (DCpointer)f.ptr);
					g_registers[expr.b.val_s64].val_s64 = 0;
					g_registers[expr.b.val_s64].val_s8 = func_result;
				} break;

				default:
				{
					struct s_foo
					{
						float x;
						float y;
					};
					s_foo foo = zero;

					DCaggr* aggr = dcNewAggr(2, 8);
					dcAggrField(aggr, DC_SIGCHAR_FLOAT, 0, 1);
					dcAggrField(aggr, DC_SIGCHAR_FLOAT, 4, 1);
					dcCloseAggr(aggr);

					// u8 result[1024];
					dcCallAggr(g_vm, (DCpointer)f.ptr, aggr, &foo);
					dcFreeAggr(aggr);

					g_registers[expr.b.val_s64].val_s64 = 0;
					g_registers[expr.b.val_s64 + 1].val_s64 = 0;
					g_registers[expr.b.val_s64].val_float = foo.x;
					g_registers[expr.b.val_s64 + 1].val_float = foo.y;
				} break;
			}
			// dcFree(vm);

		} break;

		case e_expr_push_reg:
		{
			s64 size = expr.b.val_s64;
			assert(size > 0);
			void* val = &g_code_exec_data.stack[g_code_exec_data.stack_pointer];
			dprint("push %s(%lli)\n", register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64);
			g_code_exec_data.stack_pointer += size;
			memcpy(val, &g_registers[expr.a.val_s64], size);
		} break;

		// case e_expr_pop_reg:
		// {
		// 	dprint("pop %s(%lli)\n", register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64);
		// 	g_registers[expr.a.val_s64].val_s64 = g_code_exec_data.stack.pop().val_s64;
		// } break;

		// case e_expr_pop_var:
		// {
		// 	s_var* var = get_var(expr.a.val_s64);
		// 	dprint("pop var%lli(%lli)\n", var->id, var->val.val_s64);
		// 	var->val = g_code_exec_data.stack.pop();
		// } break;

		case e_expr_return:
		{
			if(g_call_stack.count == 0)
			{
				result = c_return_value;
			}
			else
			{
				result = g_call_stack.pop();
			}
			dprint("return\n");
		} break;

		case e_expr_cmp_var_reg_32:
		{
			s32 val = *(s32*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"cmp [stack_base + %lli](%i), %s(%lli)\n",
				expr.a.val_s64, val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			do_compare(val, g_registers[expr.b.val_s64].val_s32);
		} break;

		case e_expr_cmp_var_reg_64:
		{
			s64 val = *(s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"cmp [stack_base + %lli](%lli), %s(%lli)\n",
				expr.a.val_s64, val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			do_compare(val, g_registers[expr.b.val_s64].val_s64);
		} break;

		case e_expr_cmp_var_immediate:
		{
			s64 val = *(s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			do_compare(val, expr.b.val_s64);
		} break;

		case e_expr_cmp_reg_reg:
		{
			dprint(
				"cmp %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			do_compare(g_registers[expr.a.val_s64].val_s64, g_registers[expr.b.val_s64].val_s64);
		} break;

		case e_expr_cmp_reg_reg_float:
		{
			dprint(
				"cmpf %s(%f) %s(%f)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_float
			);
			do_compare(g_registers[expr.a.val_s64].val_float, g_registers[expr.b.val_s64].val_float);
		} break;

		case e_expr_cmp_reg_immediate:
		{
			dprint(
				"cmp %s(%lli) %lli\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, expr.b.val_s64
			);
			do_compare(g_registers[expr.a.val_s64].val_s64, expr.b.val_s64);
		} break;

		case e_expr_cmp_reg_immediate_float:
		{
			dprint(
				"cmpf %s(%f) %f\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float, expr.b.val_float
			);
			do_compare(g_registers[expr.a.val_s64].val_float, expr.b.val_float);
		} break;

		case e_expr_jump_greater:
		{
			if(g_flag == e_flag_greater)
			{
				result = expr.a.val_s64;
			}
		} break;

		case e_expr_jump_lesser:
		{
			if(g_flag == e_flag_lesser)
			{
				result = expr.a.val_s64;
			}
		} break;

		case e_expr_jump_greater_or_equal:
		{
			dprint("jge %lli ", expr.a.val_s64);
			if(g_flag == e_flag_equal || g_flag == e_flag_greater)
			{
				result = expr.a.val_s64;
				dprint("YES\n");
			}
			else
			{
				dprint("NO\n");
			}
		} break;

		case e_expr_jump_less_or_equal:
		{
			dprint("jle %lli ", expr.a.val_s64);
			if(g_flag == e_flag_equal || g_flag == e_flag_lesser)
			{
				result = expr.a.val_s64;
				dprint("YES\n");
			}
			else
			{
				dprint("NO\n");
			}
		} break;

		case e_expr_jump_not_equal:
		{
			dprint("jne %lli (", expr.a.val_s64);
			if(g_flag != e_flag_equal)
			{
				result = expr.a.val_s64;
				dprint("jumped)\n");
			}
			else
			{
				dprint("didn't jump)\n");
			}
		} break;

		case e_expr_jump_equal:
		{
			dprint("je %lli (", expr.a.val_s64);
			if(g_flag == e_flag_equal)
			{
				result = expr.a.val_s64;
				dprint("jumped)\n");
			}
			else
			{
				dprint("didn't jump)\n");
			}
		} break;

		case e_expr_reg_inc:
		{
			dprint(
				"inc %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_s64 += 1;
		} break;

		case e_expr_reg_dec:
		{
			dprint(
				"dec %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_s64 -= 1;
		} break;

		case e_expr_immediate_to_var:
		{
			s64* where = (s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%lli) = %lli\n",
				expr.a.val_s64, *where, expr.b.val_s64
			);
			*where = expr.b.val_s64;
		} break;

		case e_expr_var_to_reg_8:
		{
			s8 val = *(s8*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.b.val_s64];
			dprint(
				"%s(%i) = [stack_base + %lli](%i)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s8, expr.b.val_s64, val
			);
			g_registers[expr.a.val_s64].val_s64 = 0;
			g_registers[expr.a.val_s64].val_s8 = val;
		} break;

		case e_expr_var_to_reg_32:
		{
			s32 val = *(s32*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.b.val_s64];
			dprint(
				"%s(%i) = [stack_base + %lli](%i)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s32, expr.b.val_s64, val
			);
			g_registers[expr.a.val_s64].val_s64 = 0;
			g_registers[expr.a.val_s64].val_s32 = val;
		} break;

		case e_expr_var_to_reg_64:
		{
			s64 val = *(s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.b.val_s64];
			dprint(
				"%s(%lli) = [stack_base + %lli](%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, expr.b.val_s64, val
			);
			g_registers[expr.a.val_s64].val_s64 = val;
		} break;

		case e_expr_var_to_reg_float:
		{
			float val = *(float*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.b.val_s64];
			dprint(
				"%s(%f) = [stack_base + %lli](%f)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float, expr.b.val_s64, val
			);
			g_registers[expr.a.val_s64].val_float = val;
		} break;

		case e_expr_add_reg_reg:
		{
			dprint(
				"add %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_s64 += g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_sub_reg_reg:
		{
			dprint(
				"sub %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_s64 -= g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_sub_reg_reg_float:
		{
			dprint(
				"subf %s(%f) %s(%f)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_float
			);
			g_registers[expr.a.val_s64].val_float -= g_registers[expr.b.val_s64].val_float;
		} break;

		case e_expr_add_reg_to_var_32:
		{
			s32* val = (s32*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%i) += %s(%i)\n",
				expr.a.val_s64, *val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s32
			);
			*val += g_registers[expr.b.val_s64].val_s32;
		} break;

		case e_expr_add_reg_to_var_64:
		{
			s64* val = (s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%lli) += %s(%lli)\n",
				expr.a.val_s64, *val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			*val += g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_add_reg_to_var_float:
		{
			float* val = (float*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%f) += %s(%f)\n",
				expr.a.val_s64, *val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_float
			);
			*val += g_registers[expr.b.val_s64].val_float;
		} break;

		case e_expr_sub_reg_from_var:
		{
			s64* val = (s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%lli) -= %s(%lli)\n",
				expr.a.val_s64, *val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			*val -= g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_sub_reg_from_var_float:
		{
			float* val = (float*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%f) -= %s(%f)\n",
				expr.a.val_s64, *val, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_float
			);
			*val -= g_registers[expr.b.val_s64].val_float;
		} break;

		case e_expr_divide_reg_reg:
		{
			dprint(
				"div %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_s64 /= g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_reg_mod_reg:
		{
			dprint(
				"mod %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			assert(g_registers[expr.b.val_s64].val_s64 != 0);
			g_registers[expr.a.val_s64].val_s64 %= g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_multiply_reg_var:
		{
			s_var var = *get_var(expr.b.val_s64);
			dprint(
				"imul2 %s(%lli), %lli\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, var.val.val_s64
			);
			g_registers[expr.a.val_s64].val_s64 *= var.val.val_s64;
		} break;

		case e_expr_multiply_reg_reg:
		{
			dprint(
				"imul2 %s(%lli), %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_s64 *= g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_multiply_reg_reg_float:
		{
			dprint(
				"imul2f %s(%f), %s(%f)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_float
			);
			g_registers[expr.a.val_s64].val_float *= g_registers[expr.b.val_s64].val_float;
		} break;

		case e_expr_reg_to_var_32:
		{
			s32* where = (s32*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%i) = %s(%i)\n",
				expr.a.val_s64, *where, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s32
			);
			*where = g_registers[expr.b.val_s64].val_s32;
		} break;

		case e_expr_reg_to_var_64:
		{
			s64* where = (s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + expr.a.val_s64];
			dprint(
				"[stack_base + %lli](%lli) = %s(%lli)\n",
				expr.a.val_s64, *where, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			*where = g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_reg_to_var_from_reg_8:
		{
			s64 reg_val = g_registers[expr.a.val_s64].val_s64;
			s8* where = (s8*)&g_code_exec_data.stack[g_code_exec_data.stack_base + reg_val];
			dprint(
				"[stack_base + %lli](%i) = %s(%i)\n",
				reg_val, *where, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s8
			);
			*where = g_registers[expr.b.val_s64].val_s8;
		} break;

		case e_expr_reg_to_var_from_reg_32:
		{
			s64 reg_val = g_registers[expr.a.val_s64].val_s64;
			s32* where = (s32*)&g_code_exec_data.stack[g_code_exec_data.stack_base + reg_val];
			dprint(
				"[stack_base + %lli](%i) = %s(%i)\n",
				reg_val, *where, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s32
			);
			*where = g_registers[expr.b.val_s64].val_s32;
		} break;

		case e_expr_reg_to_var_from_reg_64:
		{
			s64 reg_val = g_registers[expr.a.val_s64].val_s64;
			s64* where = (s64*)&g_code_exec_data.stack[g_code_exec_data.stack_base + reg_val];
			dprint(
				"[stack_base + %lli](%lli) = %s(%lli)\n",
				reg_val, *where, register_to_str(expr.b.val_s64), g_registers[expr.b.val_s64].val_s64
			);
			*where = g_registers[expr.b.val_s64].val_s64;
		} break;

		case e_expr_set_stack_base:
		{
			dprint(
				"stack_base = stack_pointer(%lli)\n",
				g_code_exec_data.stack_pointer
			);
			g_code_exec_data.stack_base = g_code_exec_data.stack_pointer;
		} break;

		// @Fixme(tkap, 24/07/2023): remove
		case e_expr_var_decl:
		{
			assert(false);
			// @TODO(tkap, 24/07/2023): We should have a way to say that the variable is not initialized
			// s_var var = zero;
			// var.id = g_id++;
			// g_vars.add(var);
		} break;


		case e_expr_jump:
		{
			result = expr.a.val_s64;
		} break;

		case e_expr_immediate_to_reg:
		{
			dprint(
				"%s(%lli) = %lli\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64, expr.b.val_s64
			);
			g_registers[expr.a.val_s64].val_s64 = expr.b.val_s64;
		} break;

		case e_expr_reg_float_to_int:
		{
			dprint(
				"float_to_int %s(%f)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float
			);
			g_registers[expr.a.val_s64].val_s64 = (s64)g_registers[expr.a.val_s64].val_float;
		} break;

		case e_expr_reg_int_to_float:
		{
			dprint(
				"int_to_float %s(%lli)\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_s64
			);
			g_registers[expr.a.val_s64].val_float = (float)g_registers[expr.a.val_s64].val_s64;
		} break;

		case e_expr_immediate_float_to_reg:
		{
			dprint(
				"%s(%f) = %f\n",
				register_to_str(expr.a.val_s64), g_registers[expr.a.val_s64].val_float, expr.b.val_float
			);
			g_registers[expr.a.val_s64].val_float = expr.b.val_float;
		} break;

		case e_expr_add_stack_pointer:
		{
			dprint(
				"stack_pointer(%lli) += %lli\n",
				g_code_exec_data.stack_pointer, expr.a.val_s64
			);
			g_code_exec_data.stack_pointer += expr.a.val_s64;
		} break;

		invalid_default_case;
	}
	return result;
}


DWORD WINAPI watch_for_file_changes(void* param)
{
	unreferenced(param);
	while(true)
	{
		DWORD bytes_returned;
		HANDLE handle = CreateFile(".", GENERIC_READ, FILE_SHARE_READ, null, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, null);
		FILE_NOTIFY_INFORMATION data[16] = zero;
		ReadDirectoryChangesW(handle, data, sizeof(data), true, FILE_NOTIFY_CHANGE_LAST_WRITE, &bytes_returned, null, null);
		if(wcscmp(data[0].FileName, L"input.tk") == 0)
		{
			InterlockedExchange((LONG*)&g_input_edited, 1);
		}
	}
	return 0;
}

func void print_exprs()
{
	foreach_raw(expr_i, expr, g_exprs)
	{
		printf("%i: ", expr_i);
		switch(expr.type)
		{
			case e_expr_var_decl:
			{
				printf("var_decl\n");
			} break;


			case e_expr_jump:
			{
				printf("jump %lli\n", expr.a.val_s64);
			} break;

			case e_expr_immediate_to_var:
			{
				printf("[stack_base + %lli] = %lli\n", expr.a.val_s64, expr.b.val_s64);
			} break;

			case e_expr_var_to_reg_8:
			case e_expr_var_to_reg_16:
			case e_expr_var_to_reg_32:
			case e_expr_var_to_reg_64:
			case e_expr_var_to_reg_float:
			{
				printf("%s = [stack_base + %lli]\n", register_to_str(expr.a.val_s64), expr.b.val_s64);
			} break;

			case e_expr_reg_to_var_8:
			case e_expr_reg_to_var_16:
			case e_expr_reg_to_var_32:
			case e_expr_reg_to_var_64:
			{
				printf("[stack_base + %lli] = %s\n", g_code_exec_data.stack_base + expr.a.val_s64, register_to_str(expr.b.val_s64));
			} break;

			case e_expr_reg_to_var_from_reg_8:
			case e_expr_reg_to_var_from_reg_16:
			case e_expr_reg_to_var_from_reg_32:
			case e_expr_reg_to_var_from_reg_64:
			{
				printf("[stack_base + %s] = %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_jump_greater:
			{
				printf("jg %lli\n", expr.a.val_s64);
			} break;

			case e_expr_jump_greater_or_equal:
			{
				printf("jge %lli\n", expr.a.val_s64);
			} break;

			case e_expr_jump_equal:
			{
				printf("je %lli\n", expr.a.val_s64);
			} break;

			case e_expr_jump_not_equal:
			{
				printf("jne %lli\n", expr.a.val_s64);
			} break;

			case e_expr_jump_less_or_equal:
			{
				printf("jle %lli\n", expr.a.val_s64);
			} break;

			case e_expr_return:
			{
				printf("return\n");
			} break;

			case e_expr_reg_inc:
			{
				printf("inc %s\n", register_to_str(expr.a.val_s64));
			} break;

			case e_expr_cmp_reg_reg:
			{
				printf("cmp %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_cmp_reg_immediate:
			{
				printf("cmp %s %lli\n", register_to_str(expr.a.val_s64), expr.b.val_s64);
			} break;

			case e_expr_cmp_reg_reg_float:
			{
				printf("cmpf %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_multiply_reg_reg:
			{
				printf("mul %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_multiply_reg_reg_float:
			{
				printf("mulf %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_immediate_to_reg:
			{
				printf("%s = %lli\n", register_to_str(expr.a.val_s64), expr.b.val_s64);
			} break;

			case e_expr_cmp_var_reg_8:
			case e_expr_cmp_var_reg_16:
			case e_expr_cmp_var_reg_32:
			case e_expr_cmp_var_reg_64:
			{
				printf("cmp [stack_base + %lli] %s\n", expr.a.val_s64, register_to_str(expr.b.val_s64));
			} break;

			case e_expr_divide_reg_reg:
			{
				printf("div %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_reg_mod_reg:
			{
				printf("mod %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_sub_reg_reg:
			{
				printf("sub %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_sub_reg_reg_float:
			{
				printf("subf %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_add_reg_reg:
			{
				printf("add %s %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_call:
			{
				printf("call %lli\n", expr.a.val_s64);
			} break;

			case e_expr_push_reg:
			{
				printf("push %s (%lli bytes)\n", register_to_str(expr.a.val_s64), expr.b.val_s64);
			} break;

			case e_expr_set_stack_base:
			{
				printf("set stack base\n");
			} break;

			case e_expr_add_stack_pointer:
			{
				printf("stack_pointer += %lli\n", expr.a.val_s64);
			} break;

			case e_expr_add_reg_to_var_8:
			case e_expr_add_reg_to_var_16:
			case e_expr_add_reg_to_var_32:
			case e_expr_add_reg_to_var_64:
			case e_expr_add_reg_to_var_float:
			{
				printf("[stack_base + %lli] += %s\n", expr.a.val_s64, register_to_str(expr.b.val_s64));
			} break;

			case e_expr_reg_to_address:
			{
				printf("[%s] = %s\n", register_to_str(expr.a.val_s64), register_to_str(expr.b.val_s64));
			} break;

			case e_expr_pointer_to_reg:
			{
				printf("%s = %p\n", register_to_str(expr.a.val_s64), expr.b.val_ptr);
			} break;

			case e_expr_sub_reg_from_var_float:
			{
				printf("[stack_base + %lli] += %s\n", expr.a.val_s64, register_to_str(expr.b.val_s64));
			} break;

			case e_expr_reg_float_to_int:
			{
				printf("float_to_int %s\n", register_to_str(expr.a.val_s64));
			} break;

			case e_expr_reg_int_to_float:
			{
				printf("int_to_float %s\n", register_to_str(expr.a.val_s64));
			} break;

			case e_expr_immediate_float_to_reg:
			{
				printf("%s = %f\n", register_to_str(expr.a.val_s64), expr.b.val_float);
			} break;

			case e_expr_call_external:
			{
				printf("call %lli\n", expr.a.val_s64);
			} break;


			invalid_default_case;
		}
	}
}

func char* register_to_str(int reg)
{
	char buff[4] = zero;
	buff[0] = 'e';
	buff[1] = 'a' + reg;
	buff[2] = 'x';
	return format_text("%s", buff);
}

func void reset_globals()
{
	g_exprs.count = 0;
	g_id = 0;
	g_instruction_pointer = 0;
	g_type_check_data = zero;
	g_code_gen_data = zero;
	g_call_stack.count = 0;
	g_code_exec_data = zero;
	memset(g_registers.elements, 0, sizeof(s64) * e_register_count);
}

func s_func get_func_by_id(int id)
{
	foreach_raw(f_i, f, g_code_gen_data.external_funcs)
	{
		if(f.id == id) { return f; }
	}
	assert(false);
	return zero;
}

func s_var* get_var(s64 id)
{
	unreferenced(id);
	assert(false);
	return null;
}

template <typename t0>
func void do_compare(t0 a, t0 b)
{
	if(a == b)
	{
		g_flag = e_flag_equal;
	}
	else if(a > b)
	{
		g_flag = e_flag_greater;
	}
	else if(a < b)
	{
		g_flag = e_flag_lesser;
	}
}
