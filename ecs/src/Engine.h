#ifndef _ecs_engine_h
#define _ecs_engine_h

#include "event/BroadcastEventBus.h"

struct Engine
{
  struct
  {
    BroadcastEventBus broadcast;
  } eventbus;

  b8 init()
  {
    if (!eventbus.broadcast.init())
      return false;

    return true;
  }
};

#endif
