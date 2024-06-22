// hi
struct Apple
{
  struct Leaf
  {
    float size;
  };

  enum class Kind : short
  {
    Red,
    Green,
  };

  Kind kind;
  int leaves;
  float nutrients;
};

namespace thing
{

struct Thing
{
  struct Subthing
  {
    int a = 1;
    
    struct Node
    {
      void* data;
    };
  };

  const int a = 0;
  int b = 1;
  Apple apple;
};

}

using MyThing = thing::Thing::Subthing::Node;

using namespace thing;

Thing hi(Apple apple0, const Apple apple1, Apple::Leaf leaf, Apple::Kind kind)
{ 
  return {};
}

thing::Thing hey()
{
  return {};
}

MyThing hello()
{
  return {};
}

void umm()
{
  
}
