#ifndef __LIGHTOS_OPENGL_PLATFORM__
#define __LIGHTOS_OPENGL_PLATFORM__

#define GLAPIENTRY
#define APIENTRY GLAPIENTRY

#ifndef GLAPI
#define GLAPI extern
#endif

#ifndef APIENTRYP
#define APIENTRYP APIENTRY*
#endif

// See: https://www.khronos.org/opengl/wiki/OpenGL_Type
typedef char GLchar;
typedef signed char GLbyte;
typedef unsigned char GLuchar;
typedef unsigned char GLubyte;
typedef unsigned char GLboolean;
typedef short GLshort;
typedef unsigned short GLushort;
typedef int GLint;
typedef long GLint64;
typedef long GLintptr;
typedef unsigned int GLuint;
typedef unsigned long GLuint64;
typedef int GLfixed;
typedef int GLsizei;
typedef long GLsizeiptr;
typedef void GLvoid;
typedef float GLfloat;
typedef double GLclampd;
typedef float GLclampf;
typedef double GLdouble;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;

#endif // !__LIGHTOS_OPENGL_PLATFORM__
