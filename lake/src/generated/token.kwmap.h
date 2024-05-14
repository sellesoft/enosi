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
		 case "not"_hashed: return tok::Not;
		 case "for"_hashed: return tok::For;
		 case "in"_hashed: return tok::In;
		 case "while"_hashed: return tok::While;
		 case "break"_hashed: return tok::Break;
		 case "repeat"_hashed: return tok::Repeat;
		 case "until"_hashed: return tok::Until;
		 case "do"_hashed: return tok::Do;
		 case "if"_hashed: return tok::If;
		 case "else"_hashed: return tok::Else;
		 case "elseif"_hashed: return tok::ElseIf;
		 case "then"_hashed: return tok::Then;
		 case "true"_hashed: return tok::True;
		 case "false"_hashed: return tok::False;
		 case "nil"_hashed: return tok::Nil;
		 case "and"_hashed: return tok::And;
		 case "or"_hashed: return tok::Or;
		 case "local"_hashed: return tok::Local;
		 case "function"_hashed: return tok::Function;
		 case "return"_hashed: return tok::Return;
		 case "end"_hashed: return tok::End;
	}

	return tok::Identifier;
}
