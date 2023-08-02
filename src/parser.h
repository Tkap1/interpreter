
enum e_node
{
	e_node_invalid,
	e_node_func_call,
	e_node_integer,
	e_node_identifier,
	e_node_var_decl,
	e_node_for,
	e_node_compound,
	e_node_plus_equals,
	e_node_times_equals,
	e_node_add,
	e_node_subtract,
	e_node_multiply,
	e_node_divide,
	e_node_mod,
	e_node_assign,
	e_node_if,
	e_node_equals,
	e_node_not_equals,
	e_node_greater_than_or_equal,
	e_node_less_than_or_equal,
	e_node_greater_than,
	e_node_less_than,
	e_node_return,
	e_node_func_decl,
	e_node_func_arg,
	e_node_break,
	e_node_continue,
	e_node_type,
	e_node_str,
	e_node_unary,
	e_node_struct,
	e_node_struct_member,
	e_node_member_access,
};

enum e_unary
{
	e_unary_dereference,
	e_unary_address_of,
};

struct s_node;
struct s_type_check_var
{
	int pointer_level;
	int stack_offset; // @Note(tkap, 28/07/2023): This is relative to the function
	s_node* func_node;
	s_node* type_node;
	s64 id;
	s_str<64> name;
};

struct s_type_instance
{
	int pointer_level;
	s_node* type;
};

struct s_node
{
	int line;
	e_node type;
	s_node* next;

	int stack_offset;
	int size;
	int pointer_level;
	s_node* type_node;
	s_node* func_node;

	union
	{
		struct
		{
			int arg_count;
			s_node* left;
			s_node* args;
		} func_call;

		struct
		{
			int bytes_used_by_local_variables;
			int bytes_used_by_args;
			s64 id;
			b8 external;
			int arg_count;
			s_str<64> dll_str;
			s_str<64> name;
			s_node* return_type;
			s_node* args;
			s_node* body;
		} func_decl;

		struct
		{
			s64 val;
		} integer;

		struct
		{
			int bytes_used_by_members;
			int member_count;
			s_node* members;
			s_str<64> name;
		} nstruct;

		struct
		{
			s_node* type;
			s_str<64> name;
		} struct_member;

		struct
		{
			// @TODO(tkap, 26/07/2023): This needs to be dynamic
			s_str<128> val;
		} str;

		struct
		{
			s_str<64> name;
			s_node* ntype;
			s_node* val;
		} var_decl;

		struct
		{
			s_str<64> name;
		} identifier;

		struct
		{
			b8 reverse;
			s_str<64> name;
			s_node* expr;
			s_node* body;
		} nfor;

		struct
		{
			s_node* expr;
			s_node* body;
		} nif;

		struct
		{
			int statement_count;
			s_node* statements;
		} compound;

		struct
		{
			s_node* left;
			s_node* right;
		} arithmetic;

		struct
		{
			s_node* expr;
		} nreturn;

		struct
		{
			int size_in_bytes;
			int id;
			int pointer_level;
			s_str<64> name;
		} ntype;

		struct
		{
			s_node* type;
			s_str<64> name;
		} func_arg;

		struct
		{
			e_unary type;
			s_node* expr;
		} unary;

		struct
		{
			int val;
		} nbreak;
	};
};


struct s_parse_result
{
	b8 success;
	s_tokenizer tokenizer;
	s_node node;
};

struct s_error_reporter
{
	b8 has_error;
	b8 has_warning;
	char error_str[256];

	void warning(int line, char* file, char* str, ...);
	void error(int line, char* file, char* str, ...);
	void fatal(int line, char* file, char* str, ...);
};


func s_node* parse(s_tokenizer tokenizer, char* file);
func s_parse_result parse_expr(s_tokenizer tokenizer, int operator_level, s_error_reporter* reporter, char* file);
func s_parse_result parse_statement(s_tokenizer tokenizer, s_error_reporter* reporter, char* file);
func s_node* make_node(s_node node);
func s_node** node_set_and_advance(s_node** target, s_node node);
func int get_operator_level(char* str);
func void print_parser_expr(s_node* node);
func b8 peek_assignment_token(s_tokenizer tokenizer, e_node* out_type);
func s_parse_result parse_type(s_tokenizer tokenizer, s_error_reporter* reporter, char* file);
func s_parse_result parse_func_decl(s_tokenizer tokenizer, s_error_reporter* reporter, char* file);
func int get_unary_operator_level(char* str);
func b8 token_is_keyword(s_token token);
func s_parse_result parse_struct(s_tokenizer tokenizer, s_error_reporter* reporter, char* file);