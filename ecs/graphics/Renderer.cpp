#include "Renderer.h"

#include "../window/Window.h"

#include "iro/logger.h"
#include "iro/fs/file.h"

#include "glad/gl.h"

// platform specific initialization defined in Renderer_<platform>.cpp
extern b8 rendererPlatformInit(Window* window);
extern b8 rendererPlatformSwapBuffers(Window* window);

static Logger logger = 
  Logger::create("ecs.renderer"_str, Logger::Verbosity::Info);

static void gladDebugCallback(
    void* ret, 
    const char* name, 
    GLADapiproc apiproc, 
    s32 len_args, 
    ...)
{
	int opengl_error = glad_glGetError();
	if(opengl_error != GL_NO_ERROR)
  {
		const char* error_flag = 0;
		const char* error_msg  = 0;
		switch(opengl_error){
    case GL_INVALID_ENUM:
      error_flag = "GL_INVALID_ENUM";
      error_msg  = "Set when an enumeration parameter is not legal.";
      break;
    case GL_INVALID_VALUE:
      error_flag = "GL_INVALID_VALUE";
      error_msg  = "Set when a value parameter is not legal.";
      break;
    case GL_INVALID_OPERATION:
      error_flag = "GL_INVALID_OPERATION";
      error_msg  = "Set when the state for a command is not legal for its "
                    "given parameters.";
      break;
    case 1283:
      error_flag = "GL_STACK_OVERFLOW";
      error_msg  = "Set when a stack pushing operation causes a stack "
                   "overflow.";
      break;
    case 1284:
      error_flag = "GL_STACK_UNDERFLOW";
      error_msg  = "Set when a stack popping operation occurs while the stack "
                   "is at its lowest point.";
      break;
    case GL_OUT_OF_MEMORY:
      error_flag = "GL_OUT_OF_MEMORY";
      error_msg = "Set when a memory allocation operation cannot allocate "
                  "(enough) memory.";
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      error_flag = "GL_INVALID_FRAMEBUFFER_OPERATION";
      error_msg = "Set when reading or writing to a framebuffer that is not "
                  "complete.";
      break;
		}
		ERROR(
        "opengl error ",opengl_error," '",error_flag,"' in ",name,
        "(): ",error_msg, "\n");
	}
}

/* ----------------------------------------------------------------------------
 */
b8 Renderer::init(Window* window)
{
  INFO("initializing\n");
  
  if (!rendererPlatformInit(window))
    return false;
    
  int opengl_version = gladLoaderLoadGL();
  if (!opengl_version)
    return ERROR("failed to load OpenGL\n");

#if ECS_DEBUG
  gladInstallGLDebug();
  gladSetGLPostCallback(gladDebugCallback);
#endif

  INFO("loaded OpenGL ", 
    GLAD_VERSION_MAJOR(opengl_version), '.', 
    GLAD_VERSION_MINOR(opengl_version), "\n");

  if (!compileShaders())
    return false;

  
  f32 vertices[] = 
  { 
    -0.5f, -0.5f, 0.f,
     0.5f, -0.5f, 0.f,
     0.0f,  0.5f, 0.f,
  };

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  INFO("done!\n");
  
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Renderer::update(Window* window)
{

  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(shader_program);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  rendererPlatformSwapBuffers(window);

  return true;
};

/* ----------------------------------------------------------------------------
 */
static b8 compileShader(str path, int kind, u32* out_shader_id)
{
  using namespace fs;
  auto file = File::from(path, OpenFlag::Read);
  if (isnil(file))
    return ERROR("failed to open shader at path '", path, "'\n");

  u64 filesize = file.getInfo().byte_size;
    
  io::Memory buffer;
  buffer.open();
  defer { buffer.close(); };

  Bytes reserved = buffer.reserve(filesize);
  file.read(reserved);
  buffer.commit(filesize);

  u32 id = *out_shader_id = glCreateShader(kind);

  glShaderSource(id, 1, &(char*&)buffer.buffer, nullptr);
  glCompileShader(id);

  int success;
  glGetShaderiv(id, GL_COMPILE_STATUS, &success);

  if (!success)
  {
    u8 buffer[512];
    glGetShaderInfoLog(id, 512, nullptr, (char*)buffer);
    return ERROR("failed to compile shader ", path, ":\n", buffer, "\n");
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Renderer::compileShaders()
{
  INFO("compiling shaders\n");

  if (!compileShader(
        "graphics/shaders/frag.glsl"_str, 
        GL_FRAGMENT_SHADER,
        &frag_shader))
    return false;
  
  if (!compileShader(
        "graphics/shaders/vertex.glsl"_str, 
        GL_VERTEX_SHADER,
        &vertex_shader))
    return false;


  shader_program = glCreateProgram();

  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, frag_shader);
  glLinkProgram(shader_program);

  int success;
  glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
  
  if (!success)
  {
    u8 buffer[512];
    glGetShaderInfoLog(shader_program, 512, nullptr, (char*)buffer);
    return ERROR("failed to link shaders\n");
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(frag_shader);

  return true;
}
