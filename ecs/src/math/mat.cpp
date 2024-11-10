#include "math/mat.h"

void mat3x2::calcScreenMatrices(
    vec2f screen_size,
    mat3x2* proj,
    mat3x2* view)
{
  *proj = identity();
  proj->set(0, 0, 2.f / screen_size.x);
  proj->set(1, 1, -2.f / screen_size.y);
  proj->set(2, 0, -1.f);
  proj->set(2, 1, 1.f);

  *view = identity();
}
