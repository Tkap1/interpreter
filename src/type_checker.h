
global constexpr int c_max_vars = 128;
global constexpr int c_max_scopes = 16;

struct s_type_check_data
{
	int curr_scope;
	s64 next_var_id;
	s64 next_func_id;
	s64 next_type_id;
	s64 next_struct_id;
	s_type_check_var vars[c_max_scopes][c_max_vars];
	s_carray<int, c_max_scopes> var_count;
	s_sarray<s_node, 1024> funcs;
	s_sarray<s_node, 1024> types;
	s_sarray<s_node, 1024> structs;
};

func void add_type_check_var(s_type_check_var var);
func void type_check_push_scope();
func void type_check_pop_scope();
func char* node_to_str(s_node* node);
func s_type_instance get_type_instance(s_node* node);
func s_node* get_type_by_name(char* str);
func int get_type_id(s_type_instance type_instance);
func char* type_instance_to_str(s_type_instance type_instance);
func void node_to_str_(s_node* node, s_str_sbuilder<1024>* builder);