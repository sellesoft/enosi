#include "Renderer.h"

#include "iro/logger.h"

#include "../window/Window.h"
#include "../window/Window_linux.h"

#define Window X11Window
#define Font X11Font
#define Time X11Time
#define KeyCode X11KeyCode
#define GLAD_GL_IMPLEMENTATION
#define GLAD_GLX_IMPLEMENTATION
#include "glad/glx.h"
#undef Window
#undef Font
#undef Time
#undef KeyCode

static Logger logger = 
  Logger::create("ecs.renderer.linux"_str, Logger::Verbosity::Info);

/* ----------------------------------------------------------------------------
 */
static void glxDebugCallback(
    void* ret, 
    const char* name,
    GLADapiproc apiproc, 
    s32 len_args,
    ...)
{
  u32 error_code = 0;
  const char* error_flag = nullptr;
  const char* error_msg = nullptr;

  switch(error_code)
  {
  default:
    error_flag = "?";
    error_msg = "?";
    break;
  }

  ERROR("glx error ", error_code, " in ", name, "(): ", error_msg, "\n");
}

/* ----------------------------------------------------------------------------
 */
b8 rendererPlatformInit(Window* window)
{
  int backend_version = gladLoaderLoadGLX(x11.display, x11.screen);

#if ECS_DEBUG
  gladInstallGLXDebug();
  // gladSetGLXPostCallback(glxDebugCallback);
#endif
  
  INFO("loaded GLX ", 
      GLAD_VERSION_MAJOR(backend_version), 
      ".", GLAD_VERSION_MINOR(backend_version), "\n");

  int attributes_config[] = 
  {
    GLX_X_RENDERABLE, GL_TRUE,
	  GLX_DOUBLEBUFFER, GL_TRUE,
	  //GLX_TRANSPARENT_TYPE, GLX_TRANSPARENT_RGB,
	  GLX_RENDER_TYPE, GLX_RGBA_BIT,
	  GLX_RED_SIZE, 8,
	  GLX_GREEN_SIZE, 8,
	  GLX_BLUE_SIZE, 8,
	  GLX_ALPHA_SIZE, 8,
	  GLX_DEPTH_SIZE, 24,
	  GLX_STENCIL_SIZE, 8,
	  GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
	  0, //terminate  
  };

  int framebuffer_configs_count;
  GLXFBConfig* framebuffer_configs = 
    glXChooseFBConfig(
      x11.display, 
      x11.screen, 
      attributes_config, 
      &framebuffer_configs_count);

  if (!framebuffer_configs || framebuffer_configs_count <= 0)
    return ERROR(
        "cannot find an appropriate framebuffer configuration with "
        "glXChooseFBConfig\n");

  int best_fbcfg_idx = 0;
  int best_fbcfg_samples = -1;
  int samples;
  int sample_buffers;
  XVisualInfo* best_visual = nullptr;
  for (int i = 1; i < framebuffer_configs_count; ++i)
  {
    if (XVisualInfo* visual = 
          glXGetVisualFromFBConfig(x11.display, framebuffer_configs[i]))
    {
      glXGetFBConfigAttrib(
        x11.display, 
        framebuffer_configs[i],
        GLX_SAMPLES,
        &samples);
      glXGetFBConfigAttrib(
        x11.display,
        framebuffer_configs[i],
        GLX_SAMPLE_BUFFERS,
        &sample_buffers);

      if (sample_buffers > 0 && samples > best_fbcfg_samples)
      {
        best_fbcfg_idx = i;
        best_fbcfg_samples = samples;
        if (best_visual)
          XFree(best_visual);
        best_visual = visual;
      }
      else
        XFree(visual);
    }
  }

  GLXFBConfig best_fbcfg = framebuffer_configs[best_fbcfg_idx];
  XFree(framebuffer_configs);

  if (!best_visual)
    return ERROR("cannot find an appropriate visual for the given attributes");

  // Create the colormap.
  Colormap cm = 
    XCreateColormap(
      x11.display, 
      (X11Window)window->handle,
      best_visual->visual,
      AllocNone);

  // Create the context.
  GLXContext context = 
    glXCreateContext(
      x11.display,
      best_visual,
      NULL,
      True);

  if (!context)
    return ERROR("failed to make GLX context\n");

  XFree(best_visual);

  if (!glXMakeCurrent(
        x11.display, 
        (X11Window)window->handle,
        context))
    return ERROR("failed to create glx context\n");

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 rendererPlatformSwapBuffers(Window* window)
{
  glXSwapBuffers(x11.display, (X11Window)window->handle);
  return true;
}
