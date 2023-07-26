
global constexpr int c_max_vars = 128;
global constexpr int c_max_scopes = 16;

struct s_type_check_data
{
	int curr_scope;
	s64 next_var_id;
	s64 next_func_id;
	s64 next_type_id;
	s_type_check_var vars[c_max_scopes][c_max_vars];
	s_carray<int, c_max_scopes> var_count;
	s_sarray<s_node, 1024> funcs;
	s_sarray<s_node, 1024> types;
};

func void add_type_check_var(s_type_check_var var);
func void type_check_push_scope();
func void type_check_pop_scope();
func char* node_to_str(s_node* node);