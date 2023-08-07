
struct s_break_index
{
	int index;
	int val;
};

enum e_type
{
	e_type_void,
	e_type_int,
	e_type_char,
	e_type_bool,
	e_type_u8,
	e_type_float,
};

struct s_type
{
	int pointer_level;
	s_node* type;
};

struct s_func
{
	int id;
	void* ptr;
	s_type return_type;
	s_sarray<s_type, 16> args;
};

struct s_dll
{
	s_str<64> name;
	HMODULE handle;
};

struct s_code_gen_data
{
	s_sarray<s_break_index, 1024> break_indices;
	s_sarray<int, 1024> continue_indices;

	// @TODO(tkap, 26/07/2023): Should be dynamic
	s_sarray<s_str<128>, 128> str_literals;

	s_sarray<s_func, 128> external_funcs;
	s_sarray<s_dll, 16> loaded_dlls;
	s64 next_id;
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
	e_register_count = 128,
};

enum e_flag
{
	e_flag_invalid,
	e_flag_equal,
	e_flag_greater,
	e_flag_lesser,
};

// @Note(tkap, 06/08/2023): Keep these in order (e_expr_var_to_reg_16 has to come right after e_expr_var_to_reg_8, etc)
enum e_expr
{
	e_expr_invalid,
	e_expr_var_decl,
	e_expr_jump,
	e_expr_immediate_to_var,
	e_expr_immediate_to_reg,
	e_expr_immediate_float_to_reg,
	e_expr_var_to_reg_8,
	e_expr_var_to_reg_16,
	e_expr_var_to_reg_32,
	e_expr_var_to_reg_64,
	e_expr_var_to_reg_float_32,
	e_expr_var_to_reg_float_64,
	e_expr_reg_to_var_8,
	e_expr_reg_to_var_16,
	e_expr_reg_to_var_32,
	e_expr_reg_to_var_64,
	e_expr_reg_to_var_float_32,
	e_expr_reg_to_var_float_64,
	e_expr_reg_to_var_from_reg_8,
	e_expr_reg_to_var_from_reg_16,
	e_expr_reg_to_var_from_reg_32,
	e_expr_reg_to_var_from_reg_64,
	e_expr_reg_to_var_from_reg_float_32,
	e_expr_reg_to_var_from_reg_float_64,
	e_expr_cmp_var_reg_8,
	e_expr_cmp_var_reg_16,
	e_expr_cmp_var_reg_32,
	e_expr_cmp_var_reg_64,
	e_expr_cmp_var_immediate,
	e_expr_cmp_reg_reg,
	e_expr_cmp_reg_reg_float,
	e_expr_cmp_reg_immediate,
	e_expr_cmp_reg_immediate_float,
	e_expr_jump_greater,
	e_expr_jump_lesser,
	e_expr_jump_greater_or_equal,
	e_expr_jump_less_or_equal,
	e_expr_jump_not_equal,
	e_expr_jump_equal,
	e_expr_reg_inc,
	e_expr_reg_dec,
	e_expr_multiply_reg_reg,
	e_expr_multiply_reg_reg_float,
	e_expr_multiply_reg_var,
	e_expr_add_reg_to_var_8,
	e_expr_add_reg_to_var_16,
	e_expr_add_reg_to_var_32,
	e_expr_add_reg_to_var_64,
	e_expr_add_reg_to_var_float,
	e_expr_sub_reg_from_var,
	e_expr_sub_reg_from_var_float,
	e_expr_reg_mod_reg,
	e_expr_divide_reg_reg,
	e_expr_add_reg_reg,
	e_expr_sub_reg_reg,
	e_expr_sub_reg_reg_float,
	e_expr_return,
	e_expr_call,
	e_expr_call_external,
	e_expr_push_reg,
	e_expr_pop_reg,
	e_expr_pointer_to_reg,
	e_expr_add_stack_pointer,
	e_expr_set_stack_base,
	e_expr_reg_to_address,
	e_expr_reg_float_to_int,
	e_expr_reg_int_to_float,
};


union s_val
{
	float val_float;
	s8 val_s8;
	s16 val_s16;
	s32 val_s32;
	s64 val_s64;
	void* val_ptr;
};


struct s_gen_data
{
	b8 need_compare;
	int members;
	s_carray<s_node*, 16> nodes;
	e_node comparison;
};

struct s_expr
{
	e_expr type;
	s_val a;
	s_val b;
	s_val c;
};


func int add_expr(s_expr expr);
func void generate_code(s_node* ast);
func s64 get_var_id(s_node* node);
func e_expr adjust_expr_based_on_type_and_size(e_expr type, s_node* node);