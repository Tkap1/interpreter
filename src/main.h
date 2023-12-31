
#define for_node(name, base) for(s_node* name = (base); (name); name = name->next)



#if 0
#define dprint(...) printf(__VA_ARGS__)
#else
#define dprint(...)
#endif



union s_val
{
	s64 val_s64;
	void* val_ptr;
};

typedef s_val s_register;

// @TODO(tkap, 26/07/2023): Do we need data that tells us if this is a pointer????
struct s_var
{
	s64 id;
	s_val val;
};

struct s_code_exec_data
{
	s_sarray<s_val, 1024> stack;
};


func s64 execute_expr(s_expr expr);
func s_var* get_var(s64 id);
func s_expr var_to_register(int reg, s64 index);
DWORD WINAPI watch_for_file_changes(void* param);
func void print_exprs();
func char* register_to_str(int reg);
func void do_tests();
func s64 parse_file_and_execute(char* file);
func void reset_globals();
func s_func get_func_by_id(int id);