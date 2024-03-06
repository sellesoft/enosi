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
		 case static_string_hash("Not"): return tok::Not;
		 case static_string_hash("For"): return tok::For;
		 case static_string_hash("Do"): return tok::Do;
		 case static_string_hash("If"): return tok::If;
		 case static_string_hash("Else"): return tok::Else;
		 case static_string_hash("ElseIf"): return tok::ElseIf;
		 case static_string_hash("Then"): return tok::Then;
		 case static_string_hash("True"): return tok::True;
		 case static_string_hash("False"): return tok::False;
		 case static_string_hash("Nil"): return tok::Nil;
		 case static_string_hash("And"): return tok::And;
		 case static_string_hash("Or"): return tok::Or;
		 case static_string_hash("Local"): return tok::Local;
	}

	return tok::Identifier;
}
