#ifndef _ecs_event_broadcasteventbus_h
#define _ecs_event_broadcasteventbus_h

#include "iro/common.h"
#include "iro/containers/list.h"
#include "iro/containers/pool.h"

struct BroadcastEventBus
{
  struct BroadcastEventBusData* data;
  
  template<typename T>
  void subscribeTo(void* subscriber, void (*callback)(void*, T&));

  template<typename T>
  void raise(T&& event) const;

  b8 init();
};

#endif
