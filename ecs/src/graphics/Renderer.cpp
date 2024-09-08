#include "Renderer.h"

#include "../window/Window.h"

#include "iro/logger.h"
#include "iro/fs/file.h"

#include "glad/gl.h"

namespace gfx
{

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

  if (!buffers.init())
    return ERROR("failed to initialize buffer pool\n");
  
  INFO("done!\n");
  
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Renderer::update(
    Window* window,
    f64 time)
{
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(shader_program);

  u32 scale_loc = glGetUniformLocation(shader_program, "scale");
  u32 translation = glGetUniformLocation(shader_program, "translation");
  u32 rotation = glGetUniformLocation(shader_program, "rotation");

  glUniform2f(scale_loc, 1.f,  1.f);
  glUniform2f(translation, 0.f, 0.f);
  glUniform1f(rotation, 0.f);

  for (Buffer& buffer : buffers)
  {
    glBindVertexArray(buffer.vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.ibo);
    glDrawElements(GL_TRIANGLES, buffer.num_indexes, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  }

  rendererPlatformSwapBuffers(window);

  frame += 1;

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
    return ERROR("failed to compile shader ", path, ":\n", 
        (char*)buffer, "\n");
  }

  return true;
}

/*----------------------------------------------------------------------------
 */
b8 Renderer::compileShaders()
{
  INFO("compiling shaders\n");

  if (!compileShader(
        "src/graphics/shaders/frag.glsl"_str, 
        GL_FRAGMENT_SHADER,
        &frag_shader))
    return false;
  
  if (!compileShader(
        "src/graphics/shaders/vertex.glsl"_str, 
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

/* ----------------------------------------------------------------------------
 */
Buffer* Renderer::createBuffer()
{
  return buffers.push()->data;
}

/* ----------------------------------------------------------------------------
 */
b8 Buffer::init(u64 vsize, u64 isize)
{
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ibo);
  glGenVertexArrays(1, &vao);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  vertex_size = vsize;
  index_size = isize;

  glNamedBufferStorage(vbo, vertex_size, nullptr, GL_MAP_WRITE_BIT);
  glNamedBufferStorage(ibo, index_size, nullptr, GL_MAP_WRITE_BIT);

  return true;
}

/* ----------------------------------------------------------------------------
 */
static void setAttrib(
    u32 bufferidx, 
    u32 idx, 
    u32 len, 
    u32 type, 
    b8 normalized,
    u64 stride,
    u64 offset)
{
  glBindBuffer(GL_ARRAY_BUFFER, bufferidx);
  u8* baseOffset = 0;
  glVertexAttribPointer(
    idx, 
    len, 
    type, 
    normalized, 
    stride, 
    (void*)(baseOffset+offset));
  glEnableVertexAttribArray(idx);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/* ----------------------------------------------------------------------------
 */
void Buffer::setF32AttribF32(
    u32 idx, 
    u32 len, 
    b8 normalized, 
    u64 stride,
    u64 offset)
{
  glBindVertexArray(vao);
  setAttrib(vbo, idx, len, GL_FLOAT, normalized, stride, offset);
}

/* ----------------------------------------------------------------------------
 */
void Buffer::setF32AttribU8(
    u32 idx, 
    u32 len, 
    b8 normalized, 
    u64 stride,
    u64 offset)
{
  glBindVertexArray(vao);
  setAttrib(vbo, idx, len, GL_UNSIGNED_BYTE, normalized, stride, offset);
}

/* ----------------------------------------------------------------------------
 */
void Buffer::map()
{
  assert(!mapped.vp && !mapped.ip && "buffer is already mapped");
  
  glBindVertexArray(vao);
  mapped.vp = glMapNamedBuffer(vbo, GL_WRITE_ONLY);
  mapped.ip = glMapNamedBuffer(ibo, GL_WRITE_ONLY);
}

/* ----------------------------------------------------------------------------
 */
void Buffer::unmap()
{
  assert(mapped.vp && mapped.ip && "buffer is not mapped");

  assert(glUnmapNamedBuffer(vbo));
  assert(glUnmapNamedBuffer(ibo));

  mapped.vp = mapped.ip = nullptr;
}


}
