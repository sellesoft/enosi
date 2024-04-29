/*
 * Structures representing JSON types and an api for manipulating them.
 *
 */

#ifndef _iro_json_types_h
#define _iro_json_types_h

#include "common.h"
#include "unicode.h"
#include "containers/avl.h"
#include "containers/list.h"
#include "containers/array.h"
#include "io/io.h"

namespace iro::json
{

struct Value;

/* ================================================================================================ json::Array
 *  An array of json::Values, or really, references to json::Values. The objects are expected 
 *  not to move in memory.
 */
struct Array
{
	typedef iro::Array<Value*> ValueArray;
	
	ValueArray values;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	b8   init();
	void deinit();

	void   push(Value* o);
	Value* pop();
};

/* ================================================================================================ json::Object
 */
struct Object
{
	struct Member
	{
		str name;
		u64 hash;
		Value* value;

		static u64 get_key(const Member* x) { return x->hash; }
	};

	typedef AVL<Member, Member::get_key> MemberMap;
	typedef Pool<Member> MemberPool;

	MemberMap  members;
	MemberPool pool;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	b8   init();
	void deinit();

	b8 add_member(str name, Value* value);
	Value* find_member(str name);
};

/* ================================================================================================ json::Value
 */
struct Value
{
	enum class Kind
	{
		Null,
		False,
		True,
		Object,
		Array,
		Number,
		String,
	};

	Kind kind;

	union
	{
		Object object;
		Array  array;
		f64    number;
		str    string;
	};

	
	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */

	b8   init();
	void deinit();
	

	void print();
};

/* ================================================================================================ json::JSON
 *  Type representing a single JSON text and which manages the memory of all values 
 *  contained in it.
 *
 *  This may either be created via json::Parser or created manually through the api.
 *  These can be transformed into JSON source via json::Generator
 */
struct JSON
{
	typedef Pool<Value>  ValuePool;
	typedef DList<Value> ValueList;

	ValuePool pool;
	Value*    root;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */

	
	b8   init();
	void deinit();

	// allocates a value of the given kind from the pool and returns a pointer to it
	Value* new_value(Value::Kind kind);

	s64 pretty_print(io::IO* out);
};

} // namespace json

namespace iro::io
{

static s64 format(IO* io, json::JSON json)
{
	return json.pretty_print(io);
}

}

#endif // _lpp_json_types_h
