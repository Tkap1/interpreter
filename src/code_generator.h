
struct s_code_gen_data
{
	s64 next_id;
	s64 next_id_before_exec;
};


enum e_register
{
	e_register_eax,
	e_register_ebx,
	e_register_ecx,
	e_register_edx,
	e_register_eex,
	e_register_efx,
	e_register_egx,
	e_register_count,
};

enum e_flag
{
	e_flag_invalid,
	e_flag_equal,
	e_flag_greater,
	e_flag_lesser,
};


enum e_expr
{
	e_expr_invalid,
	e_expr_plus_equals,
	e_expr_var_decl,
	e_expr_print_immediate,
	e_expr_print_var,
	e_expr_jump,
	e_expr_immediate_to_var,
	e_expr_var_to_register,
	e_expr_register_to_var,
	e_expr_cmp,
	e_expr_cmp_var_register,
	e_expr_cmp_reg_reg,
	e_expr_jump_greater,
	e_expr_jump_greater_or_equal,
	e_expr_jump_not_equal,
	e_expr_jump_equal,
	e_expr_register_inc,
	e_expr_imul2_reg_reg,
	e_expr_imul2_reg_var,
	e_expr_imul3,
	e_expr_val_to_register,
	e_expr_add_register_to_var,
	e_expr_register_mod_register,
	e_expr_divide_reg_reg,
	e_expr_add_reg_reg,
	e_expr_return,
};

enum e_operand
{
	e_operand_var,
	e_operand_register,
	e_operand_immediate,
};

struct s_foo
{
	s64 val;
};

struct s_gen_data
{
	e_node comparison;
};

struct s_expr
{
	e_expr type;
	s_foo a;
	s_foo b;
	s_foo c;
};


func int add_expr(s_expr expr);
func void generate_code(s_node* ast);
