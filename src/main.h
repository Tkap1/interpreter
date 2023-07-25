
#define for_node(name, base) for(s_node* name = (base); (name); name = name->next)



#if 0
#define dprint(...) printf(__VA_ARGS__)
#else
#define dprint(...)
#endif

union s_register
{
	s64 val_s64;
};


struct s_var
{
	s64 id;
	s64 val;
};


func s64 execute_expr(s_expr expr);
func s_var* get_var(s64 id);
func s_expr var_to_register(int reg, s64 index);
DWORD WINAPI watch_for_file_changes(void* param);
func void print_exprs();
func char* register_to_str(int reg);