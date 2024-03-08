struct KWElem
{
	u64 hash;
	tok kind;
};

static tok is_keyword_or_identifier(str s)
{
	u64 hash = s.hash();
	switch (hash)
	{
		 case static_string_hash("not"): return tok::Not;
		 case static_string_hash("for"): return tok::For;
		 case static_string_hash("in"): return tok::In;
		 case static_string_hash("while"): return tok::While;
		 case static_string_hash("break"): return tok::Break;
		 case static_string_hash("repeat"): return tok::Repeat;
		 case static_string_hash("until"): return tok::Until;
		 case static_string_hash("do"): return tok::Do;
		 case static_string_hash("if"): return tok::If;
		 case static_string_hash("else"): return tok::Else;
		 case static_string_hash("elseif"): return tok::ElseIf;
		 case static_string_hash("then"): return tok::Then;
		 case static_string_hash("true"): return tok::True;
		 case static_string_hash("false"): return tok::False;
		 case static_string_hash("nil"): return tok::Nil;
		 case static_string_hash("and"): return tok::And;
		 case static_string_hash("or"): return tok::Or;
		 case static_string_hash("local"): return tok::Local;
		 case static_string_hash("function"): return tok::Function;
		 case static_string_hash("return"): return tok::Return;
		 case static_string_hash("end"): return tok::End;
	}

	return tok::Identifier;
}
