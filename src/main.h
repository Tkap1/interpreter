
#define for_node(name, base) for(s_node* name = (base); (name); name = name->next)

// #define m_verbose 1
// #define m_show_asm 1

#ifdef m_verbose
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
	s64 stack_pointer;
	s64 stack_base;
	u8 stack[8192];
};


func s64 execute_expr(s_expr expr);
func s_var* get_var(s64 id);
DWORD WINAPI watch_for_file_changes(void* param);
func void print_exprs();
func char* register_to_str(int reg);
func void do_tests();
func s64 parse_file_and_execute(char* file);
func void reset_globals();
func s_func get_func_by_id(int id);
func s_var* get_var(s64 id);
func void do_compare(s64 a, s64 b);