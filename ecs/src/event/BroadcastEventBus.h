#ifndef _ecs_event_BroadcastEventBus_h
#define _ecs_event_BroadcastEventBus_h

#include "iro/Common.h"
#include "iro/containers/List.h"
#include "iro/containers/Pool.h"

using namespace iro;

struct BroadcastEventBus
{
  struct BroadcastEventBusData* data;
  
  template<typename T>
  void subscribeTo(void* subscriber, void (*callback)(void*, T&));

  template<typename T>
  void raise(T&& event) const { raise<T>(event); }

  template<typename T>
  void raise(T& event) const;

  b8 init();
};

#endif
