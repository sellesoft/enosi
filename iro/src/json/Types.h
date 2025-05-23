/*
 * Structures representing JSON types and an api for manipulating them.
 *
 */

#ifndef _iro_json_Types_h
#define _iro_json_Types_h

#include "../Common.h"
#include "../Unicode.h"
#include "../containers/AVL.h"
#include "../containers/List.h"
#include "../containers/Array.h"
#include "../memory/Bump.h"
#include "../io/IO.h"

namespace iro::json
{

struct Value;
struct JSON;

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

  b8 pushNumber(JSON* json, f32 number);
};

/* ============================================================================
 */
struct Object
{
  struct Member
  {
    String name;
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

  b8 addMember(String name, Value* value);
  Value* findMember(String name);

  Array* addArray(
    JSON* json, 
    String name,
    s32 init_space = 8,
    mem::Allocator* allocator = &mem::stl_allocator);

  Object* addObject(
    JSON* json, 
    String name,
    mem::Allocator* allocator = &mem::stl_allocator);

  b8 addString(JSON* json, String name, String string);

  b8 addNull(JSON* json, String name);
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
    String string;
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
static inline String getValueKindString(Value::Kind x)
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

  String cacheString(String s);

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
