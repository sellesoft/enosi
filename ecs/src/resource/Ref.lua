--- 
--- Helper for creating Ref types.
---

local cmn = require "common"

return function(T, Mgr)
  local buf = cmn.buffer.new()

  local vars = 
  {
    T = T,
    R = T.."Ref",
    M = "Moved"..T.."Ref",
    Mgr = Mgr,
  }

  buf:put(
  [[
struct $(Mgr);
struct $(R);

struct $(M)
{
  constexpr $(M)() {}

  b8 isValid() const { return ptr != nullptr; }

private:
  
  friend $(Mgr);
  friend $(R);

  constexpr $(M)($(T)* ptr) : ptr(ptr) {}

  $(T)* ptr = nullptr;
};

DefineNilValue($(M), {}, { return !x.isValid(); });

struct $(R)
{
  constexpr $(R)() {}
  constexpr $(R)(Nil) {}

  // Typesafe movement between resource handles.
  constexpr $(R)($(M)& ref)
  {
    ptr = ref.ptr;
    ref.ptr = nullptr;
  }

  $(R)($(M)&& ref) : $(R)(ref) {}

  constexpr ~$(R)() { assert(ptr == nullptr); }

  $(R)& operator=(Nil)
  {
    release();
    return *this;
  }

  bool operator ==(const $(R)& rhs) const
  {
    return rhs.ptr == ptr;
  }

  // Disallow implicit copying.
  $(R)(const $(R)& rhs) = delete;

  // Define C++ moving so we can initialize normally and whatever.
  $(R)($(R)&& rhs)
  {
    ptr = rhs.ptr;
    rhs.ptr = nullptr;
  }

  $(R) copy() const
  {
    assert(ptr != nullptr && "attempt to copy a nil $(R)");
    ptr->rc.addRef();
    return $(R){ptr};
  }

  $(M) move()
  {
    assert(ptr != nullptr && "attempt to move a nil $(R)");
    auto out = $(M){ptr};
    ptr = nullptr;
    return out;
  }

  void release();

  void track(MovedResourceRef&& ref)
  {
    assert(ptr == nullptr);
    ptr = ref.ptr;
    ref.ptr = nullptr;
  }

  $(T)* get$(T)() const
  {
    return ptr;
  }

  b8 isValid() const { return ptr != nullptr; }

private:
  $(T)* ptr = nullptr;

  friend $(Mgr);

  $(R)($(T)* ptr) : ptr(ptr) {}
};

DefineNilValue($(R), {}, { return !x.isValid(); });
  ]])

  return buf:get():gsub("$%(([%w_]+)%)", vars)
end
