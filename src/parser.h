
enum e_node
{
	e_node_invalid,
	e_node_func_call,
	e_node_integer,
	e_node_identifier,
	e_node_var_decl,
	e_node_for,
	e_node_func_decl,
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
};

struct s_type_check_var
{
	s64 id;
	s_str<64> name;
};

struct s_node
{
	e_node type;
	s_node* next;
	s_type_check_var var_data;

	union
	{
		struct
		{
			s_node* left;
			s_node* args;
		} func_call;

		struct
		{
			s_str<64> name;
			s_node* args; // @TODO(tkap, 24/07/2023):
		} func_decl;

		struct
		{
			s64 val;
		} integer;

		struct
		{
			s_str<64> name;
			s_node* val;
		} var_decl;

		struct
		{
			s_str<64> name;
		} identifier;

		struct
		{
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
	};
};


struct s_parse_result
{
	b8 success;
	s_tokenizer tokenizer;
	s_node node;
};


func s_node* parse(s_tokenizer tokenizer);
func s_parse_result parse_expr(s_tokenizer tokenizer);
func s_parse_result parse_statement(s_tokenizer tokenizer);
func s_node* make_node(s_node node);
func s_node** node_set_and_advance(s_node** target, s_node node);
func int get_operator_level(char* str);
func void print_parser_expr(s_node* node);
func b8 peek_assignment_token(s_tokenizer tokenizer, e_node* out_type);