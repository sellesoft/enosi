/*
 *  Experimental implementation of fibers (or coroutines) based on edubart's 
 *  minicoro library.
 *
 *  Designed specifically to run on my machine for now.
 */

#include "iro/Common.h"
#include "iro/memory/Memory.h"

#include "assert.h"

using namespace iro;

struct Registers
{
  void* rip; 
  void* rsp; 
  void* rbp; 
  void* rbx; 
  void* r12; 
  void* r13; 
  void* r14; 
  void* r15; 
};

extern "C" void _fib_wrap_main();
extern "C" int _fib_switch(Registers* from, Registers* to);

struct Coroutine;

static void _fib_main(Coroutine* co);

static u64 alignForward(u64 x, u64 align)
{
  return (x + (align - 1)) & ~(align - 1);;
}

struct Coroutine
{
  enum class State
  {
    Dead,
    Normal,
    Running,
    Suspended,
  };

  struct Context
  {
    Registers current;
    Registers back;

    b8 init(Coroutine* co, void* stack_base, u64 stack_size)
    {
      stack_size -= 128; // 128 bytes reserved for red zone.
      void** stack_high_ptr = 
        (void**)((u64)stack_base + stack_size - sizeof(u64));

      // Dummy return address.
      stack_high_ptr[0] = (void*)(0xdeaddeaddeaddead);
      current.rip = (void*)(_fib_wrap_main);
      current.rsp = (void*)(stack_high_pointer);
      current.r12 = (void*)(_fib_main);
      current.r13 = (void*)(co);

      return true;
    }
  };

  typedef void Entry(Coroutine*);

  State state;

  Entry entry;

  Context* context;

  Coroutine* prev;

  void* user_data;

  u64 size;

  u8* storage;
  u64 bytes_stored;
  u64 storage_size;

  void* stack_base;
  u64   stack_size;

  struct InitParams
  {
    u64 stack_size;
    u64 storage_size;
  };

  b8 init(InitParams& params)

};

static thread_local Coroutine* fib_current_co = nullptr;

static Coroutine* _fib_running()
{
  return fib_current_co;
}

static Coroutine* fib_running()
{
  Coroutine* (*volatile func)() = _fib_running;
  return func();
}

static void _fib_prepare_jumpin(Coroutine* co)
{
  Coroutine* running = fib_running();
  assert(co->prev == nullptr);

  co->prev = running;

  if (running)
  {
    assert(running->state == Coroutine::State::Running);
    running->state = Coroutine::State::Normal;
  }

  fib_current_co = co;
}

static void _fib_prepare_jumpout(Coroutine* co)
{
  Coroutine* prev = co->prev;
  co->prev = nullptr;

  if (prev != nullptr)
    prev->state = Coroutine::State::Running;

  fib_current_co = prev;
}

static void _fib_jumpin(Coroutine* co)
{
  _fib_prepare_jumpin(co);
  _fib_switch(&co->context->back, &co->context->current);
}

static void _fib_jumpout(Coroutine* co)
{
  _fib_prepare_jumpout(co);
  _fib_switch(&co->context->current, &co->context->back);
}

void _fib_main(Coroutine* co)
{
  co->entry(co);
  co->state = Coroutine::State::Dead;
  _mco_jumpout(co);
}

b8 Coroutine::init(InitParams& params)
{
  u64 addr = (u64)this;
  u64 context_addr = alignForward(addr + sizeof(Coroutine), 16);
  u64 storage_addr = alignForward(context_addr + sizeof(Context), 16);
  u64 stack_addr = alignForward(storage_addr + params.storage_size, 16);

  context = (Context*)context_addr;
  mem::zero(context, sizeof(Context));

  storage = (u8*)storage_addr;
  storage_size = params.storage_size;
  
  stack_base = (void*)stack_addr;
  stack_size = params.stack_size;

  if (!context->init(this, stack_base, stack_size))
    return false;

  return true;
}

static b8 createCoroutine(Coroutine** co, Coroutine::Entry entry)
{
  
}

int main()
{
  Registers a, b;

  _fib_switch(&a, &b);
}
