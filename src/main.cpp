

#include "tklib.h"
#include "parser.h"
#include "type_checker.h"
#include "code_generator.h"
#include "main.h"

global constexpr s64 c_return_value = -1234;

global s64 g_id = 0;
global s_sarray<s_var, 1024> g_vars;
global s_sarray<s_expr, 1024> g_exprs;
global s64 g_expr_index = 0;
global s_carray<s_register, e_register_count> g_registers;
global e_flag g_flag;
global volatile int g_input_edited;
global s_lin_arena g_arena;
global s_type_check_data g_type_check_data = zero;
global s_code_gen_data g_code_gen_data;
global HANDLE stdout_handle;

#include "parser.cpp"
#include "type_checker.cpp"
#include "code_generator.cpp"

int main(int argc, char** argv)
{
	argc -= 1;
	argv += 1;

	g_arena = make_lin_arena(1 * c_mb, false);
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	do_tests();

	CreateThread(null, 0, watch_for_file_changes, null, 0, null);


	while(true)
	{
		system("cls");

		reset_globals();

		#if 0
		// @Note(tkap, 24/07/2023): i
		{
			s_var var = zero;
			var.id = g_id++;
			var.val = 1;
			g_vars.add(var);
		}

		// @Note(tkap, 24/07/2023): result
		{
			s_var var = zero;
			var.id = g_id++;
			var.val = 1;
			g_vars.add(var);
		}

		add_expr({.type = e_expr_cmp, .a = {.val = 0}, .b = {.val = 10}});
		add_expr({.type = e_expr_jump_greater, .a = {.val = 9}});
		add_expr(var_to_register(e_register_eax, 1));
		add_expr({.type = e_expr_imul2, .a = {.val = e_register_eax}, .b = {.operand = e_operand_var, .val = 0}});
		add_expr({.type = e_expr_register_to_var, .a = {.val = 1}, .b = {.val = e_register_eax}});
		add_expr(var_to_register(e_register_eax, 0));
		add_expr({.type = e_expr_register_inc, .a = {.val = e_register_eax}});
		add_expr({.type = e_expr_register_to_var, .a = {.val = 0}, .b = {.val = e_register_eax}});
		add_expr({.type = e_expr_jump, .a = {.val = 0}});
		add_expr({.type = e_expr_print, .a = {.val = 1}});
		// add_expr({.type = e_expr_jump, .a = {.val = 0}});
		#else

		s_tokenizer tokenizer = zero;
		tokenizer.at = read_file_quick("input.tk", &g_arena);
		if(tokenizer.at)
		{
			s_node* ast = parse(tokenizer);
			type_check(ast);
			generate_code(ast);
		}
		#endif

		while(g_expr_index < g_exprs.count)
		{
			g_expr_index = execute_expr(g_exprs[g_expr_index]);
			if(g_expr_index == c_return_value) { break; }
		}
		printf("------\n");
		print_exprs();
		printf("@@@@@@\n");

		while(true)
		{
			if(g_input_edited)
			{
				// @Note(tkap, 24/07/2023): Let's wait for a bit to hopefully get rid of all the repeats that windows sends
				Sleep(100);

				if(InterlockedCompareExchange((LONG*)&g_input_edited, 0, 1) == 1)
				{
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
	struct s_test_data
	{
		char* file;
		int expected_result;
	};

	constexpr s_test_data c_tests[] = {
		{.file = "tests/factorial.tk", .expected_result = 3628800},
		{.file = "tests/fibonacci.tk", .expected_result = 55},
		{.file = "tests/prime.tk", .expected_result = 79},
	};

	for(int test_i = 0; test_i < array_count(c_tests); test_i++)
	{
		s_test_data test = c_tests[test_i];
		s64 result = parse_file_and_execute(test.file);
		if(result == test.expected_result)
		{
			SetConsoleTextAttribute(stdout_handle, FOREGROUND_GREEN);
			printf("TEST %i %s PASSED!\n", test_i + 1, test.file);
		}
		else
		{
			SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED);
			printf("--------------------\n\n");
			printf("TEST %i %s FAILED!\n\n", test_i + 1, test.file);
			printf("--------------------\n\n");
		}
	}
	SetConsoleTextAttribute(stdout_handle, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_GREEN);
}

func s64 parse_file_and_execute(char* file)
{
	reset_globals();

	s_tokenizer tokenizer = zero;
	tokenizer.at = read_file_quick(file, &g_arena);
	if(!tokenizer.at) { return -1; }

	s_node* ast = parse(tokenizer);
	if(!ast) { return -1; }
	type_check(ast);
	generate_code(ast);

	while(g_expr_index < g_exprs.count)
	{
		g_expr_index = execute_expr(g_exprs[g_expr_index]);
		if(g_expr_index == c_return_value) { break; }
	}
	return g_registers[e_register_eax].val_s64;
}


func s64 execute_expr(s_expr expr)
{
	s64 result = g_expr_index + 1;
	switch(expr.type)
	{
		case e_expr_cmp:
		{
			s_var var = *get_var(expr.a.val);
			if(var.val == expr.b.val)
			{
				g_flag = e_flag_equal;
			}
			else if(var.val > expr.b.val)
			{
				g_flag = e_flag_greater;
			}
			else if(var.val < expr.b.val)
			{
				g_flag = e_flag_lesser;
			}
		} break;

		case e_expr_return:
		{
			dprint("return\n");
			result = c_return_value;
		} break;

		case e_expr_cmp_var_register:
		{
			s_var var = *get_var(expr.a.val);
			if(var.val == g_registers[expr.b.val].val_s64)
			{
				g_flag = e_flag_equal;
			}
			else if(var.val > g_registers[expr.b.val].val_s64)
			{
				g_flag = e_flag_greater;
			}
			else if(var.val < g_registers[expr.b.val].val_s64)
			{
				g_flag = e_flag_lesser;
			}
		} break;

		case e_expr_cmp_reg_reg:
		{
			dprint(
				"cmp %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			if(g_registers[expr.a.val].val_s64 == g_registers[expr.b.val].val_s64)
			{
				g_flag = e_flag_equal;
			}
			else if(g_registers[expr.a.val].val_s64 > g_registers[expr.b.val].val_s64)
			{
				g_flag = e_flag_greater;
			}
			else if(g_registers[expr.a.val].val_s64 < g_registers[expr.b.val].val_s64)
			{
				g_flag = e_flag_lesser;
			}
		} break;

		case e_expr_jump_greater:
		{
			if(g_flag == e_flag_greater)
			{
				result = expr.a.val;
			}
		} break;

		case e_expr_jump_greater_or_equal:
		{
			if(g_flag == e_flag_equal || g_flag == e_flag_greater)
			{
				result = expr.a.val;
			}
		} break;

		case e_expr_jump_not_equal:
		{
			dprint("jne %lli (", expr.a.val);
			if(g_flag != e_flag_equal)
			{
				result = expr.a.val;
				dprint("jumped)\n");
			}
			else
			{
				dprint("didn't jump)\n");
			}
		} break;

		case e_expr_jump_equal:
		{
			dprint("je %lli (", expr.a.val);
			if(g_flag == e_flag_equal)
			{
				result = expr.a.val;
				dprint("jumped)\n");
			}
			else
			{
				dprint("didn't jump)\n");
			}
		} break;

		case e_expr_register_inc:
		{
			dprint(
				"inc %s(%lli)\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64
			);
			g_registers[expr.a.val].val_s64 += 1;
		} break;

		case e_expr_immediate_to_var:
		{
			s_var* var = get_var(expr.a.val);
			dprint(
				"mov var%lli(%lli) %lli\n",
				var->id, var->val, expr.b.val
			);
			var->val = expr.b.val;
		} break;

		case e_expr_var_to_register:
		{
			s_var var = *get_var(expr.b.val);
			dprint(
				"mov %s(%lli) %lli\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, var.val
			);
			g_registers[expr.a.val].val_s64 = var.val;
		} break;

		case e_expr_add_reg_reg:
		{
			dprint(
				"add %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			g_registers[expr.a.val].val_s64 += g_registers[expr.b.val].val_s64;
		} break;

		case e_expr_add_register_to_var:
		{
			s_var* var = get_var(expr.a.val);
			dprint(
				"add var%lli(%lli) %s(%lli)\n",
				var->id, var->val, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			var->val += g_registers[expr.b.val].val_s64;
		} break;

		case e_expr_divide_reg_reg:
		{
			dprint(
				"div %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			g_registers[expr.a.val].val_s64 /= g_registers[expr.b.val].val_s64;
		} break;

		case e_expr_register_mod_register:
		{
			dprint(
				"mod %s(%lli) %s(%lli)\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			assert(g_registers[expr.b.val].val_s64 != 0);
			g_registers[expr.a.val].val_s64 %= g_registers[expr.b.val].val_s64;
		} break;

		case e_expr_imul2_reg_var:
		{
			s_var var = *get_var(expr.b.val);
			dprint(
				"imul2 %s(%lli), %lli\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, var.val
			);
			g_registers[expr.a.val].val_s64 *= var.val;
		} break;

		case e_expr_imul2_reg_reg:
		{
			dprint(
				"imul2 %s(%lli), %s(%lli)\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			g_registers[expr.a.val].val_s64 *= g_registers[expr.b.val].val_s64;
		} break;

		case e_expr_imul3:
		{
			s_var var = *get_var(expr.b.val);
			g_registers[expr.a.val].val_s64 = var.val * expr.c.val;
		} break;

		case e_expr_register_to_var:
		{
			s_var* var = get_var(expr.a.val);
			dprint(
				"mov var%lli(%lli) %s(%lli)\n",
				var->id, var->val, register_to_str(expr.b.val), g_registers[expr.b.val].val_s64
			);
			var->val = g_registers[expr.b.val].val_s64;
		} break;

		// @Fixme(tkap, 24/07/2023): remove
		case e_expr_var_decl:
		{
			assert(false);
			// @TODO(tkap, 24/07/2023): We should have a way to say that the variable is not initialized
			s_var var = zero;
			var.id = g_id++;
			g_vars.add(var);
		} break;

		case e_expr_plus_equals:
		{
			s_var* left = get_var(expr.a.val);
			s_var* right = get_var(expr.b.val);
			left->val += right->val;
		} break;

		case e_expr_print_immediate:
		{
			printf("%lli\n", expr.a.val);
		} break;

		case e_expr_print_var:
		{
			s_var* var = get_var(expr.a.val);
			printf("%lli\n", var->val);
		} break;

		case e_expr_jump:
		{
			result = expr.a.val;
		} break;

		case e_expr_val_to_register:
		{
			dprint(
				"mov %s(%lli) %lli\n",
				register_to_str(expr.a.val), g_registers[expr.a.val].val_s64, expr.b.val
			);
			g_registers[expr.a.val].val_s64 = expr.b.val;
		} break;

		invalid_default_case;
	}
	return result;
}

func s_var* get_var(s64 id)
{
	foreach(var_i, var, g_vars)
	{
		if(var->id == id) { return var; }
	}
	assert(false);
	return null;
}

func s_expr var_to_register(int reg, s64 index)
{
	assert(reg < e_register_count);

	s_expr expr = zero;
	expr.type = e_expr_var_to_register;
	expr.a.val = reg;
	expr.b.val = index;
	return expr;
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

			case e_expr_print_immediate:
			{
				printf("print %lli\n", expr.a.val);
			} break;

			case e_expr_print_var:
			{
				printf("print var id %lli\n", expr.a.val);
			} break;

			case e_expr_jump:
			{
				printf("jump %lli\n", expr.a.val);
			} break;

			case e_expr_immediate_to_var:
			{
				printf("immediate to var\n");
			} break;

			case e_expr_var_to_register:
			{
				printf("var to register\n");
			} break;

			case e_expr_register_to_var:
			{
				printf("register to var\n");
			} break;

			case e_expr_cmp:
			{
				printf("cmp\n");
			} break;

			case e_expr_jump_greater:
			{
				printf("jg %lli\n", expr.a.val);
			} break;

			case e_expr_jump_greater_or_equal:
			{
				printf("jge %lli\n", expr.a.val);
			} break;

			case e_expr_jump_equal:
			{
				printf("je %lli\n", expr.a.val);
			} break;

			case e_expr_jump_not_equal:
			{
				printf("jne %lli\n", expr.a.val);
			} break;

			case e_expr_return:
			{
				printf("return");
			} break;

			case e_expr_register_inc:
			{
				printf("inc %s\n", register_to_str(expr.a.val));
			} break;

			case e_expr_cmp_reg_reg:
			{
				printf("cmp %s %s\n", register_to_str(expr.a.val), register_to_str(expr.b.val));
			} break;

			case e_expr_imul2_reg_reg:
			{
				printf("imul reg reg\n");
			} break;

			case e_expr_imul3:
			{
				printf("imul\n");
			} break;

			case e_expr_val_to_register:
			{
				printf("val to %s\n", register_to_str(expr.a.val));
			} break;

			case e_expr_cmp_var_register:
			{
				printf("cmp var %s\n", register_to_str(expr.b.val));
			} break;

			case e_expr_divide_reg_reg:
			{
				printf("div %s %s\n", register_to_str(expr.a.val), register_to_str(expr.b.val));
			} break;

			case e_expr_register_mod_register:
			{
				printf("mod %s %s\n", register_to_str(expr.a.val), register_to_str(expr.b.val));
			} break;

			case e_expr_add_reg_reg:
			{
				printf("add %s %s\n", register_to_str(expr.a.val), register_to_str(expr.b.val));
			} break;


			invalid_default_case;
		}
	}
}

func char* register_to_str(int reg)
{
	switch(reg)
	{
		case e_register_eax:
		{
			return "eax";
		} break;
		case e_register_ebx:
		{
			return "ebx";
		} break;
		case e_register_ecx:
		{
			return "ecx";
		} break;
		case e_register_edx:
		{
			return "edx";
		} break;
		case e_register_eex:
		{
			return "eex";
		} break;
		case e_register_efx:
		{
			return "efx";
		} break;
		case e_register_egx:
		{
			return "egx";
		} break;

		invalid_default_case;
	}
	return "";
}

func void reset_globals()
{
	g_vars.count = 0;
	g_exprs.count = 0;
	g_id = 0;
	g_expr_index = 0;
	g_type_check_data = zero;
	g_code_gen_data = zero;
	memset(g_registers.elements, 0, sizeof(s64) * e_register_count);
}