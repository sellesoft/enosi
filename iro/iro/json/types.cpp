#include "types.h"
#include "../io/format.h"

namespace iro::json
{

/* ------------------------------------------------------------------------------------------------
 */
b8 Array::init(s32 init_space, mem::Allocator* allocator)
{
  values = ValueArray::create(init_space, allocator);
  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
void Array::deinit()
{
  values.destroy();
}

/* ------------------------------------------------------------------------------------------------
 */
void Array::push(Value* v)
{
  values.push(v);
}

/* ------------------------------------------------------------------------------------------------
 */
Value* Array::pop()
{
  Value* v = *(values.arr + values.len() - 1);
  values.pop();
  return v;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Array::pushNumber(JSON* json, f32 number)
{
  Value* v = json->newValue(Value::Kind::Number);
  if (!v)
    return false;

  v->number = number;
  push(v);

  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Object::init(mem::Allocator* allocator)
{
  if (!members.init(allocator))
    return false;
  if (!pool.init(allocator))
    return false;
  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
void Object::deinit()
{
  members.deinit();
  pool.deinit();
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Object::addMember(str name, Value* value)
{
  Member* m = pool.add();
  if (!m)
    return false;

  m->name = name;
  m->hash = name.hash();
  m->value = value;

  members.insert(m);

  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
Value* Object::findMember(str name)
{
  if (Member* member = members.find(name.hash()))
    return member->value;
  return nullptr;
}

/* ------------------------------------------------------------------------------------------------
 */
Array* Object::addArray(
    JSON* json, 
    str name,
    s32 init_space,
    mem::Allocator* allocator)
{
  Value* v = json->newValue(Value::Kind::Array);
  if (!v) 
    return nullptr;

  if (!v->array.init(init_space, allocator))
  {
    json->pool.remove(v);
    return nullptr;
  }

  if (!addMember(name, v))
  {
    json->pool.remove(v);
    return nullptr;
  }

  return &v->array;
}

/* ------------------------------------------------------------------------------------------------
 */
Object* Object::addObject(
    JSON* json,
    str name,
    mem::Allocator* allocator)
{
  Value* v = json->newValue(Value::Kind::Object);
  if (!v)
    return nullptr;

  if (!v->object.init(allocator))
  {
    json->pool.remove(v);
    return nullptr;
  }

  if (!addMember(name, v))
  {
    json->pool.remove(v);
    return nullptr;
  }

  return &v->object;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Object::addString(JSON* json, str name, str string)
{
  Value* v = json->newValue(Value::Kind::String);
  if (!v)
    return false;

  if (!addMember(name, v))
  {
    json->pool.remove(v);
    return false;
  }

  v->string = string;

  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Object::addNull(JSON* json, str name)
{
  Value* v = json->newValue(Value::Kind::Null);
  if (!v)
    return false;

  if (!addMember(name, v))
  {
    json->pool.remove(v);
    return false;
  }

  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 JSON::init()
{
  if (!string_buffer.init())
    return false;
  pool = ValuePool::create();
  root = nullptr;
  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
void JSON::deinit()
{
  pool.deinit();
  root = nullptr;
}

/* ------------------------------------------------------------------------------------------------
 */
str JSON::cacheString(str s)
{
  return s.allocateCopy(&string_buffer);
}

/* ------------------------------------------------------------------------------------------------
 */
Value* JSON::newValue(Value::Kind kind)
{
  Value* v = pool.add();
  if (!v)
    return nullptr;
  v->kind = kind;
  return v;
}

/* ------------------------------------------------------------------------------------------------
 */
void prettyPrintRecur(io::IO* out, Value* value, s32 depth)
{
  using enum Value::Kind;

  switch (value->kind)
  {
  case Null: 
    out->write("null"_str);
    return;
  case Boolean:
    out->write(value->boolean? "true"_str : "false"_str);
    return;
  case Number:
    io::format(out, value->number);
    return;
  case String:
    io::formatv(out, "\"", value->string, "\"");
    return;
  case Array:
    out->write("[\n"_str);
    for (s32 i = 0, len = value->array.values.len(); i < len; i++)
    {
      for (s32 i = 0; i < depth + 1; i++)
        out->write(" "_str);
      prettyPrintRecur(out, value->array.values[i], depth + 2);
      if (i != len - 1)
        out->write(","_str);
      out->write("\n"_str);
    }
    for (s32 i = 0; i < depth-1; i++)
      out->write(" "_str);
    out->write("]"_str);
    return;
  case Object:
    out->write("{\n"_str);
    if (value->object.members.root)
    {
      auto last = // incredible
        value->object.members.maximum(value->object.members.root)->data;
      for (auto& member : value->object.members)
      {
        for (s32 i = 0; i < depth + 1; i++)
          out->write(" "_str);
        io::formatv(out, "\"", member.name, "\": ");
        prettyPrintRecur(out, member.value, depth + 2);
        if (&member != last)
          out->write(","_str);
        out->write("\n"_str);
      }
    }
    for (s32 i = 0; i < depth-1; i++)
      out->write(" "_str);
    out->write("}"_str);
    return;
  }
}

/* ------------------------------------------------------------------------------------------------
 */
s64 JSON::prettyPrint(io::IO* out)
{
  prettyPrintRecur(out, root, 0);
  return 0; // TODO(sushi) return number written later... maybe
}

} // namespace json
