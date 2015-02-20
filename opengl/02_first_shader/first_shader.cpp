#include <math.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

GLuint rendering_program;
GLuint vertex_array_object;

GLuint compile_shaders(void)
{
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint program;

	static const GLchar *vertex_shader_source[] = {
		"#version 430 core								\n"
		"												\n"		
		"void main(void)								\n"
		"{												\n"
		"	gl_Position = vec4(0.0, 0.0, 0.5, 1.0);		\n"
		"}												\n"
	};

	static const GLchar *fragment_shader_source[] = {
		"#version 430 core							\n"
		"											\n"
		"out vec4 color;							\n"
		"											\n"
		"void main(void)							\n"
		"{											\n"
		"	color = vec4(0.0, 0.8, 1.0, 1.0);		\n"
		"}											\n"
	};

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, vertex_shader_source, nullptr);
	glCompileShader(vertex_shader);

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, fragment_shader_source, nullptr);
	glCompileShader(fragment_shader);

	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}

void startup(void)
{
	rendering_program = compile_shaders();
	glGenVertexArrays(1, &vertex_array_object);
	glBindVertexArray(vertex_array_object);
}

void render(double currentTime)
{
	const GLfloat color[] = {
		static_cast<float>(sin(currentTime)) * 0.5f + 0.5f,
		static_cast<float>(cos(currentTime)) * 0.5f + 0.5f,
		1.0f, 1.0f
	};
	glClearBufferfv(GL_COLOR, 0, color);

	glUseProgram(rendering_program);
	glPointSize(40.0f);
	glDrawArrays(GL_POINTS, 0, 1);
}

void on_resize(int width, int height)
{
}

void shutdown(void)
{
	glDeleteVertexArrays(1, &vertex_array_object);
	glDeleteProgram(rendering_program);
}