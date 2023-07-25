
func s_node* parse(s_tokenizer tokenizer)
{
	s_node* result = null;
	s_node** target = &result;
	s_token token = zero;
	s_parse_result pr = zero;
	while(true)
	{
		if(consume_token(e_token_eof, &tokenizer, &token)) { break; }
		pr = parse_statement(tokenizer);
		if(pr.success)
		{
			tokenizer = pr.tokenizer;
			target = node_set_and_advance(target, pr.node);
			continue;
		}

		assert(false);
	}

	return result;
}

func s_parse_result parse_sub_expr(s_tokenizer tokenizer)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;

	if(consume_token(e_token_identifier, &tokenizer, &token))
	{
		result.node.type = e_node_identifier;
		result.node.identifier.name.from_data(token.at, token.length);
	}
	else if(consume_token(e_token_number, &tokenizer, &token))
	{
		result.node.type = e_node_integer;
		result.node.integer.val = token_to_int(token);
	}
	else
	{
		goto end;
	}

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_expr(s_tokenizer tokenizer, int operator_level)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	s_node arg = zero;
	auto arithmetic = &result.node.arithmetic;

	pr = parse_sub_expr(tokenizer);
	if(!pr.success) { goto end; }
	tokenizer = pr.tokenizer;
	result.node = pr.node;

	while(true)
	{
		struct s_operator_query
		{
			e_node type;
			char* str;
		};
		constexpr s_operator_query queries[] = {
			{.type = e_node_add, .str = "+"},
			{.type = e_node_subtract, .str = "-"},
			{.type = e_node_multiply, .str = "*"},
			{.type = e_node_divide, .str = "/"},
			{.type = e_node_mod, .str = "%"},
			{.type = e_node_equals, .str = "=="},
			{.type = e_node_not_equals, .str = "!="},
			{.type = e_node_greater_than_or_equal, .str = ">="},
			{.type = e_node_less_than_or_equal, .str = "<="},
			{.type = e_node_greater_than, .str = ">"},
			{.type = e_node_less_than, .str = "<"},
		};

		e_node type = e_node_invalid;
		for(int query_i = 0; query_i < array_count(queries); query_i++)
		{
			s_operator_query query = queries[query_i];
			if(get_operator_level(query.str) > operator_level && consume_token(query.str, &tokenizer))
			{
				pr = parse_expr(tokenizer, get_operator_level(query.str));
				assert(pr.success);
				tokenizer = pr.tokenizer;
				type = query.type;
				break;
			}
		}

		if(type > e_node_invalid)
		{
			s_node temp = zero;
			temp.type = type;
			arithmetic = &temp.arithmetic;
			arithmetic->left = make_node(result.node);
			arithmetic->right = make_node(pr.node);
			result.node = temp;
			continue;
		}

		break;
	}

	if(consume_token("(", &tokenizer))
	{
		pr = parse_expr(tokenizer, operator_level);
		if(pr.success)
		{
			tokenizer = pr.tokenizer;
			s_node temp = zero;
			temp.type = e_node_func_call;
			temp.func_call.left = make_node(result.node);
			temp.func_call.args = make_node(pr.node);
			result.node = temp;
		}
		if(!consume_token(")", &tokenizer)) { assert(false); }
	}

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_statement(s_tokenizer tokenizer)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	s_node** target = null;
	auto var_decl = &result.node.var_decl;
	auto compound = &result.node.compound;
	auto arithmetic = &result.node.arithmetic;
	e_node assign_type;

	pr = parse_expr(tokenizer, 0);

	if(pr.success && peek_assignment_token(pr.tokenizer, &assign_type))
	{
		tokenizer = pr.tokenizer;
		next_token(&tokenizer);

		result.node.type = assign_type;
		arithmetic->left = make_node(pr.node);

		pr = parse_expr(tokenizer, 0);
		assert(pr.success);
		tokenizer = pr.tokenizer;
		arithmetic->right = make_node(pr.node);

		if(!consume_token(";", &tokenizer)) { assert(false); }
	}

	else if(consume_token("int", &tokenizer))
	{
		result.node.type = e_node_var_decl;

		if(!consume_token(e_token_identifier, &tokenizer, &token)) { assert(false); }
		var_decl->name.from_data(token.at, token.length);

		if(!consume_token("=", &tokenizer)) { assert(false); }

		pr = parse_expr(tokenizer, 0);
		assert(pr.success);
		tokenizer = pr.tokenizer;
		var_decl->val = make_node(pr.node);

		if(!consume_token(";", &tokenizer)) { assert(false); }
	}

	else if(consume_token("for", &tokenizer))
	{
		result.node.type = e_node_for;

		if(peek_token(e_token_identifier, tokenizer, &token) && peek_token(":", tokenizer, 1))
		{
			next_token(&tokenizer);
			next_token(&tokenizer);
			result.node.nfor.name.from_data(token.at, token.length);
		}

		pr = parse_expr(tokenizer, 0);
		if(!pr.success) { assert(false); }
		tokenizer = pr.tokenizer;
		result.node.nfor.expr = make_node(pr.node);

		pr = parse_statement(tokenizer);
		if(!pr.success) { assert(false); }
		assert(pr.node.type == e_node_compound);
		tokenizer = pr.tokenizer;
		result.node.nfor.body = make_node(pr.node);
	}

	else if(consume_token("if", &tokenizer))
	{
		result.node.type = e_node_if;

		pr = parse_expr(tokenizer, 0);
		if(!pr.success) { assert(false); }
		tokenizer = pr.tokenizer;
		result.node.nif.expr = make_node(pr.node);

		pr = parse_statement(tokenizer);
		if(!pr.success) { assert(false); }
		assert(pr.node.type == e_node_compound);
		tokenizer = pr.tokenizer;
		result.node.nif.body = make_node(pr.node);
	}

	else if(consume_token("{", &tokenizer))
	{
		result.node.type = e_node_compound;

		target = &compound->statements;
		while(true)
		{
			if(consume_token("}", &tokenizer)) { break; }
			pr = parse_statement(tokenizer);
			assert(pr.success);
			tokenizer = pr.tokenizer;
			target = node_set_and_advance(target, pr.node);
			compound->statement_count += 1;
		}
	}

	else
	{
		pr = parse_expr(tokenizer, 0);
		if(!pr.success) { goto end; }
		tokenizer = pr.tokenizer;
		result.node = pr.node;

		if(!consume_token(";", &tokenizer)) { assert(false); }
	}

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}


func s_node** node_set_and_advance(s_node** target, s_node node)
{
	assert(target);
	assert(!*target);
	*target = make_node(node);
	return &(*target)->next;
}

func s_node* make_node(s_node node)
{
	s_node* new_node = (s_node*)g_arena.get(sizeof(s_node));
	*new_node = node;
	return new_node;
}

func int get_operator_level(char* str)
{
	if(
		strcmp(str, "+") == 0 ||
		strcmp(str, "-") == 0
	)
	{
		return 5;
	}

	if(
		strcmp(str, "*") == 0 ||
		strcmp(str, "/") == 0 ||
		strcmp(str, "%") == 0
	)
	{
		return 10;
	}

	if(
		strcmp(str, "==") == 0 ||
		strcmp(str, "!=") == 0
	)
	{
		return 2;
	}

	if(
		strcmp(str, ">") == 0 ||
		strcmp(str, "<") == 0 ||
		strcmp(str, ">=") == 0 ||
		strcmp(str, "<=") == 0
	)
	{
		return 3;
	}

	assert(false);
	return 0;
}

// @TODO(tkap, 24/07/2023): Incomplete
func void print_parser_expr(s_node* node)
{
	switch(node->type)
	{
		case e_node_times_equals:
		{
			printf("%s *= ", node->arithmetic.left->identifier.name.data);
			print_parser_expr(node->arithmetic.right);
		} break;

		case e_node_add:
		{
			printf("(");
			print_parser_expr(node->arithmetic.left);
			printf(" + ");
			print_parser_expr(node->arithmetic.right);
			printf(")");
		} break;

		case e_node_multiply:
		{
			printf("(");
			print_parser_expr(node->arithmetic.left);
			printf(" * ");
			print_parser_expr(node->arithmetic.right);
			printf(")");
		} break;

		case e_node_integer:
		{
			printf("%lli", node->integer.val);
		} break;

		case e_node_identifier:
		{
			printf("%s", node->identifier.name.data);
		} break;

		invalid_default_case;
	}
}

func b8 peek_assignment_token(s_tokenizer tokenizer, e_node* out_type)
{
	struct s_op
	{
		e_node type;
		char* str;
	};
	constexpr s_op ops[] = {
		{.type = e_node_plus_equals, .str = "+="},
		{.type = e_node_times_equals, .str = "*="},
		{.type = e_node_assign, .str = "="},
	};

	for(int op_i = 0; op_i < array_count(ops); op_i++)
	{
		if(consume_token(ops[op_i].str, &tokenizer))
		{
			*out_type = ops[op_i].type;
			return true;
		}
	}
	return false;
}