#include "../header/shader.h"

// Boilerplate methods sourced from LearnOpenGL.com

// Reads the contents of a file to a string
std::string GetFileContents(const char* filename) {
	std::ifstream in(filename, std::ios::binary);
	if (in) {
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

// Creates a shader object, which references both the vertex and fragment shader files
Shader::Shader(const char* vertexFile, const char* fragmentFile) {
	std::string vertexCode = GetFileContents(vertexFile);
	std::string fragmentCode = GetFileContents(fragmentFile);

	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

// Creates a compute shader object from the file
GLuint Shader::CreateComputeShader(const char* computeFile) {
	std::string shaderString = GetFileContents(computeFile);
	const char* shaderSource = shaderString.c_str();

	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &shaderSource, nullptr);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLint logLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		char* log = new char[logLength];
		glGetShaderInfoLog(shader, logLength, &logLength, log);
		std::cerr << "Compute shader compilation failed: " << log << std::endl;
		delete[] log;
		exit(-1);
	}

	return shader;
}

// Creates a shader program from the loaded compute shader
GLuint Shader::CreateShaderProgram(GLuint computeShader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, computeShader);
	glLinkProgram(program);

	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		GLint logLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		char* log = new char[logLength];
		glGetProgramInfoLog(program, logLength, &logLength, log);
		std::cerr << "Program linking failed: " << log << std::endl;
		delete[] log;
		exit(-1);
	}

	return program;
}

// Tells OpenGL to use this shader object
void Shader::Activate() {
	glUseProgram(ID);
}

// Tells OpenGL to remove this shader object
void Shader::Delete() {
	glDeleteProgram(ID);
}