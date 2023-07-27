
func s_node* parse(s_tokenizer tokenizer, char* file)
{
	s_error_reporter reporter = zero;
	s_node* result = null;
	s_node** target = &result;
	s_token token = zero;
	s_parse_result pr = zero;
	while(true)
	{
		if(consume_token(e_token_eof, &tokenizer, &token)) { break; }

		pr = parse_struct(tokenizer, &reporter, file);
		if(pr.success)
		{
			tokenizer = pr.tokenizer;
			target = node_set_and_advance(target, pr.node);
			continue;
		}

		pr = parse_func_decl(tokenizer, &reporter, file);
		if(pr.success)
		{
			tokenizer = pr.tokenizer;
			target = node_set_and_advance(target, pr.node);
			continue;
		}

		reporter.fatal(0, file, "We did not find a statement");
	}

	return result;
}

func s_parse_result parse_type(s_tokenizer tokenizer, s_error_reporter* reporter)
{
	unreferenced(reporter);
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	result.node.line = tokenizer.line_num;
	result.node.type = e_node_type;

	if(!consume_token(e_token_identifier, &tokenizer, &token)) { goto end; }

	if(token_is_keyword(token)) { goto end; }

	result.node.ntype.name.from_data(token.at, token.length);

	while(true)
	{
		if(consume_token("*", &tokenizer))
		{
			result.node.ntype.pointer_level += 1;
		}
		else { break; }
	}

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_func_decl(s_tokenizer tokenizer, s_error_reporter* reporter, char* file)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	s_node** target = &result.node.func_decl.args;
	b8 found_comma = true;
	result.node.line = tokenizer.line_num;

	if(consume_token("func", &tokenizer)) {}
	else if(consume_token("external_func", &tokenizer)) { result.node.func_decl.external = true; }
	else { goto end; }

	result.node.type = e_node_func_decl;

	pr = parse_type(tokenizer, reporter);
	if(!pr.success)
	{
		reporter->fatal(tokenizer.line_num, file, "Expected type after 'func'");
	}
	tokenizer = pr.tokenizer;
	result.node.func_decl.return_type = make_node(pr.node);

	if(!consume_token(e_token_identifier, &tokenizer, &token))
	{
		reporter->fatal(tokenizer.line_num, file, "Expected identifier after function type");
	}
	result.node.func_decl.name.from_data(token.at, token.length);

	if(!consume_token("(", &tokenizer))
	{
		reporter->fatal(tokenizer.line_num, file, "Expected '(' after function name");
	}

	while(true)
	{
		if(consume_token(")", &tokenizer)) { break; }

		if(!found_comma)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected ')'");
		}

		s_node new_arg = zero;
		new_arg.type = e_node_func_arg;

		pr = parse_type(tokenizer, reporter);
		if(!pr.success)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected type");
		}
		tokenizer = pr.tokenizer;
		new_arg.func_arg.type = make_node(pr.node);
		new_arg.line = tokenizer.line_num;

		if(!result.node.func_decl.external)
		{
			if(!consume_token(e_token_identifier, &tokenizer, &token))
			{
				reporter->fatal(tokenizer.line_num, file, "Expected name after argument type");
			}
			new_arg.func_arg.name.from_data(token.at, token.length);
		}

		target = node_set_and_advance(target, new_arg);
		result.node.func_decl.arg_count += 1;

		if(consume_token(",", &tokenizer))
		{
			found_comma = true;
		}
		else
		{
			found_comma = false;
		}
	}

	if(result.node.func_decl.external)
	{
		if(consume_token(e_token_string, &tokenizer, &token))
		{
			result.node.func_decl.dll_str.from_data(token.at + 1, token.length - 2);
		}
		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';'"); }
	}
	else
	{
		pr = parse_statement(tokenizer, reporter, file);
		if(!pr.success)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected function body");
		}
		tokenizer = pr.tokenizer;
		result.node.func_decl.body = make_node(pr.node);
	}

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_sub_expr(s_tokenizer tokenizer, s_error_reporter* reporter, char* file)
{
	unreferenced(reporter);

	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	b8 found_comma = true;
	s_node** arg_target = &result.node.func_call.args;
	result.node.line = tokenizer.line_num;

	struct s_operator
	{
		e_unary type;
		char* str;
	};

	constexpr s_operator operators[] = {
		{.type = e_unary_dereference, .str = "*"},
		{.type = e_unary_address_of, .str = "&"},
	};

	for(int operator_i = 0; operator_i < array_count(operators); operator_i++)
	{
		s_operator op = operators[operator_i];
		if(consume_token(op.str, &tokenizer))
		{
			int operator_level = get_unary_operator_level(op.str);
			pr = parse_expr(tokenizer, operator_level, reporter, file);
			if(!pr.success)
			{
				reporter->fatal(tokenizer.line_num, file, "Expected expression");
			}
			tokenizer = pr.tokenizer;
			result.node.type = e_node_unary;
			result.node.unary.type = op.type;
			result.node.unary.expr = make_node(pr.node);
			goto success;
		}
	}

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
	else if(consume_token(e_token_string, &tokenizer, &token))
	{
		result.node.type = e_node_str;
		result.node.str.val.from_data(token.at + 1, token.length - 2);
	}
	else
	{
		goto end;
	}

	success:
	if(consume_token("(", &tokenizer))
	{
		s_node temp = zero;
		temp.type = e_node_func_call;
		temp.func_call.left = make_node(result.node);
		result.node = temp;

		while(true)
		{
			if(consume_token(")", &tokenizer))	{ break; }

			if(!found_comma)
			{
				reporter->fatal(tokenizer.line_num, file, "Expected ')'");
			}

			pr = parse_expr(tokenizer, 0, reporter, file);
			if(!pr.success)
			{
				reporter->fatal(tokenizer.line_num, file, "Expected expression");
			}
			tokenizer = pr.tokenizer;
			arg_target = node_set_and_advance(arg_target, pr.node);
			result.node.func_call.arg_count += 1;

			if(consume_token(",", &tokenizer))
			{
				found_comma = true;
			}
			else
			{
				found_comma = false;
			}
		}
	}


	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_expr(s_tokenizer tokenizer, int operator_level, s_error_reporter* reporter, char* file)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	s_node arg = zero;
	auto arithmetic = &result.node.arithmetic;

	pr = parse_sub_expr(tokenizer, reporter, file);
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
			{.type = e_node_member_access, .str = "."},
		};

		e_node type = e_node_invalid;
		for(int query_i = 0; query_i < array_count(queries); query_i++)
		{
			s_operator_query query = queries[query_i];
			if(get_operator_level(query.str) > operator_level && consume_token(query.str, &tokenizer))
			{
				pr = parse_expr(tokenizer, get_operator_level(query.str), reporter, file);
				if(!pr.success)
				{
					reporter->fatal(tokenizer.line_num, file, "Expected expression after operator");
				}
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

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_struct(s_tokenizer tokenizer, s_error_reporter* reporter, char* file)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	s_node member = zero;
	s_node** member_target = &result.node.nstruct.members;

	if(!consume_token("struct", &tokenizer)) { goto end; }
	result.node.type = e_node_struct;
	result.node.line = tokenizer.line_num;

	if(!consume_token(e_token_identifier, &tokenizer, &token))
	{
		reporter->fatal(tokenizer.line_num, file, "Expected identifier after 'struct'");
	}
	result.node.nstruct.name.from_data(token.at, token.length);

	if(!consume_token("{", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected '{' after 'struct'"); }

	while(true)
	{
		if(consume_token("}", &tokenizer)) { break; }

		// @TODO(tkap, 27/07/2023): parse a member
		pr = parse_type(tokenizer, reporter);
		if(!pr.success)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected type");
		}
		tokenizer = pr.tokenizer;
		member.struct_member.type = make_node(pr.node);

		if(!consume_token(e_token_identifier, &tokenizer, &token))
		{
			reporter->fatal(tokenizer.line_num, file, "Expected identifier struct member's type");
		}
		member.struct_member.name.from_data(token.at, token.length);

		member_target = node_set_and_advance(member_target, member);
		result.node.nstruct.member_count += 1;

		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';' after struct member"); }
	}

	result.success = true;
	result.tokenizer = tokenizer;

	end:
	return result;
}

func s_parse_result parse_statement(s_tokenizer tokenizer, s_error_reporter* reporter, char* file)
{
	s_parse_result result = zero;
	s_parse_result pr = zero;
	s_token token = zero;
	s_node** target = null;
	s_parse_result var_decl_pr = zero;
	auto var_decl = &result.node.var_decl;
	auto compound = &result.node.compound;
	auto arithmetic = &result.node.arithmetic;
	e_node assign_type;
	result.node.line = tokenizer.line_num;

	pr = parse_expr(tokenizer, 0, reporter, file);
	var_decl_pr = parse_type(tokenizer, reporter);
	if(var_decl_pr.success)
	{
		s_tokenizer temp_tokenizer = var_decl_pr.tokenizer;
		if(!consume_token(e_token_identifier, &temp_tokenizer, &token))
		{
			var_decl_pr.success = false;
		}
	}

	if(var_decl_pr.success)
	{
		tokenizer = var_decl_pr.tokenizer;
		var_decl->ntype = make_node(var_decl_pr.node);

		result.node.type = e_node_var_decl;

		if(!consume_token(e_token_identifier, &tokenizer, &token)) { reporter->fatal(tokenizer.line_num, file, "Expected identifier"); }
		var_decl->name.from_data(token.at, token.length);

		if(consume_token("=", &tokenizer))
		{
			pr = parse_expr(tokenizer, 0, reporter, file);
			if(!pr.success)
			{
				reporter->fatal(tokenizer.line_num, file, "Expected expression");
			}
			tokenizer = pr.tokenizer;
			var_decl->val = make_node(pr.node);
		}

		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';'"); }
	}

	else if(pr.success && peek_assignment_token(pr.tokenizer, &assign_type))
	{
		tokenizer = pr.tokenizer;
		next_token(&tokenizer);

		result.node.type = assign_type;
		arithmetic->left = make_node(pr.node);

		pr = parse_expr(tokenizer, 0, reporter, file);
		if(!pr.success)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected expression");
		}

		tokenizer = pr.tokenizer;
		arithmetic->right = make_node(pr.node);

		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';'"); }
	}

	else if(consume_token("break", &tokenizer))
	{
		result.node.type = e_node_break;
		result.node.nbreak.val = 1;
		if(consume_token(e_token_number, &tokenizer, &token))
		{
			result.node.nbreak.val = (int)token_to_int(token);
		}

		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';' after 'break'"); }
	}

	else if(consume_token("continue", &tokenizer))
	{
		result.node.type = e_node_continue;
		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';' after 'continue'"); }
	}

	else if(consume_token("for", &tokenizer))
	{
		result.node.type = e_node_for;

		if(consume_token("<", &tokenizer))
		{
			result.node.nfor.reverse = true;
		}

		if(peek_token(e_token_identifier, tokenizer, &token) && peek_token(":", tokenizer, 1))
		{
			next_token(&tokenizer);
			next_token(&tokenizer);
			result.node.nfor.name.from_data(token.at, token.length);
		}

		pr = parse_expr(tokenizer, 0, reporter, file);
		if(!pr.success) { reporter->fatal(tokenizer.line_num, file, "Expected expression after 'for'"); }
		tokenizer = pr.tokenizer;
		result.node.nfor.expr = make_node(pr.node);

		pr = parse_statement(tokenizer, reporter, file);
		if(!pr.success) { reporter->fatal(tokenizer.line_num, file, "Expected statement after 'for'"); }
		if(pr.node.type != e_node_compound)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected a compound statement after 'for'");
		}
		tokenizer = pr.tokenizer;
		result.node.nfor.body = make_node(pr.node);
	}

	else if(consume_token("if", &tokenizer))
	{
		result.node.type = e_node_if;

		pr = parse_expr(tokenizer, 0, reporter, file);
		if(!pr.success) { reporter->fatal(tokenizer.line_num, file, "Expected expression after 'if'"); }
		tokenizer = pr.tokenizer;
		result.node.nif.expr = make_node(pr.node);

		pr = parse_statement(tokenizer, reporter, file);
		if(!pr.success) { reporter->fatal(tokenizer.line_num, file, "Expected statement after 'if'"); }
		if(pr.node.type != e_node_compound)
		{
			reporter->fatal(tokenizer.line_num, file, "Expected compound statement after 'if''");
		}
		tokenizer = pr.tokenizer;
		result.node.nif.body = make_node(pr.node);
	}

	else if(consume_token("return", &tokenizer))
	{
		result.node.type = e_node_return;

		pr = parse_expr(tokenizer, 0, reporter, file);
		if(pr.success)
		{
			tokenizer = pr.tokenizer;
			result.node.nreturn.expr = make_node(pr.node);
		}
		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';' after return"); }
	}

	else if(consume_token("{", &tokenizer))
	{
		result.node.type = e_node_compound;

		target = &compound->statements;
		while(true)
		{
			if(consume_token("}", &tokenizer)) { break; }
			pr = parse_statement(tokenizer, reporter, file);
			if(!pr.success)
			{
				reporter->fatal(tokenizer.line_num, file, "Expected statement");
			}
			tokenizer = pr.tokenizer;
			target = node_set_and_advance(target, pr.node);
			compound->statement_count += 1;
		}
	}

	else
	{
		pr = parse_expr(tokenizer, 0, reporter, file);
		if(!pr.success) { goto end; }
		tokenizer = pr.tokenizer;
		result.node = pr.node;

		if(!consume_token(";", &tokenizer)) { reporter->fatal(tokenizer.line_num, file, "Expected ';'"); }
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

func int get_unary_operator_level(char* str)
{
	if(
		strcmp(str, "&") == 0 ||
		strcmp(str, "*") == 0
	)
	{
		return 50;
	}

	assert(false);
	return 0;
}

func int get_operator_level(char* str)
{

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
		strcmp(str, ".") == 0
	)
	{
		return 30;
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

void s_error_reporter::warning(int line, char* file, char* str, ...)
{
	unreferenced(file);
	unreferenced(line);

	if(has_warning || has_error) { return; }
	has_warning = true;

	va_list args;
	va_start(args, str);
	vsnprintf(error_str, 255, str, args);
	va_end(args);
}

void s_error_reporter::error(int line, char* file, char* str, ...)
{
	unreferenced(file);
	unreferenced(line);

	if(has_error) { return; }
	has_error = true;

	va_list args;
	va_start(args, str);
	vsnprintf(error_str, 255, str, args);
	va_end(args);
}

void s_error_reporter::fatal(int line, char* file, char* str, ...)
{
	va_list args;
	va_start(args, str);
	vsnprintf(error_str, 255, str, args);
	va_end(args);
	printf("%s (%i): ", file, line);
	printf("%s\n", error_str);
	assert(false);
	exit(-1);
}

func b8 token_is_keyword(s_token token)
{
	constexpr char* keywords[] = {
		"for", "return", "break", "continue", "func", "if", "else", "external_func",
	};

	for(int keyword_i = 0; keyword_i < array_count(keywords); keyword_i++)
	{
		if(strlen(keywords[keyword_i]) != token.length) { continue; }
		if(memcmp(keywords[keyword_i], token.at, token.length) == 0)
		{
			return true;
		}
	}
	return false;
}