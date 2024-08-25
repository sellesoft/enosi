/*
 * Structures representing JSON types and an api for manipulating them.
 *
 */

#ifndef _iro_json_types_h
#define _iro_json_types_h

#include "../common.h"
#include "../unicode.h"
#include "../containers/avl.h"
#include "../containers/list.h"
#include "../containers/array.h"
#include "../memory/bump.h"
#include "../io/io.h"

namespace iro::json
{

struct Value;

/* ============================================================================
 *  An array of json::Values, or really, references to json::Values. The 
 *  values are expected to not move in memory.
 */
struct Array
{
  typedef iro::Array<Value*> ValueArray;
  
  ValueArray values;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  b8 init(
    s32 init_space = 8, 
    mem::Allocator* allocator = &mem::stl_allocator);
  void deinit();

  void   push(Value* o);
  Value* pop();
};

/* ============================================================================
 */
struct Object
{
  struct Member
  {
    str name;
    u64 hash;
    Value* value;

    static u64 getKey(const Member* x) { return x->hash; }
  };

  typedef AVL<Member, Member::getKey> MemberMap;
  typedef Pool<Member> MemberPool;

  MemberMap  members;
  MemberPool pool;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  b8   init(mem::Allocator* allocator = &mem::stl_allocator);
  void deinit();

  b8 addMember(str name, Value* value);
  Value* findMember(str name);
};

/* ============================================================================
 */
struct Value
{
  enum class Kind
  {
    Null,
    Boolean,
    Object,
    Array,
    Number,
    String,
  };

  Kind kind;

  union
  {
    b8     boolean;
    Object object;
    Array  array;
    f64    number;
    str    string;
  };

  Value() : kind(Kind::Null) {}

  
  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  b8 init(mem::Allocator* allocator = &mem::stl_allocator)
  {
    switch (kind)
    {
    case Kind::Null: 
    case Kind::Boolean:
    case Kind::Number:
    case Kind::String:
      return true;
    case Kind::Object:
      return object.init();
    case Kind::Array:
      return array.init();
    }
    return false;
  }

  void deinit()
  {
    switch (kind)
    {
    case Kind::Object:
      object.deinit();
      break;
    case Kind::Array:
      array.deinit();
      break;
    }
  }

  b8 isNull() { return kind == Kind::Null; }
  b8 isBoolean() { return kind == Kind::Boolean; }
  b8 isObject() { return kind == Kind::Object; }
  b8 isArray() { return kind == Kind::Array; }
  b8 isNumber() { return kind == Kind::Number; }
  b8 isString() { return kind == Kind::String; }
};

/* ----------------------------------------------------------------------------
 */
static inline str getValueKindString(Value::Kind x)
{
  switch (x)
  {
#define z(x) case Value::Kind::x: return GLUE(STRINGIZE(x),_str);
  z(Null);
  z(Boolean);
  z(Object);
  z(Array);
  z(Number);
  z(String);
#undef z
  }
  assert(!"Invalid value kind passed");
  return {};
}

/* ============================================================================
 *  Type representing a single JSON text and which manages the memory of all 
 *  values contained in it.
 *
 *  This may either be created via json::Parser or created manually through the 
 *  api. These can be transformed into JSON source via json::Generator.
 */
struct JSON
{
  typedef Pool<Value>  ValuePool;
  typedef DList<Value> ValueList;

  ValuePool pool;
  Value*    root;

  mem::LenientBump string_buffer;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */

  
  b8   init();
  void deinit();

  str cacheString(str s);

  // Allocates a value of the given kind from the pool and returns a pointer 
  // to it.
  Value* newValue(Value::Kind kind);

  s64 prettyPrint(io::IO* out);
};

} // namespace json

namespace iro::io
{

static s64 format(IO* io, json::JSON json)
{
  return json.prettyPrint(io);
}

}

#endif // _lpp_json_types_h
