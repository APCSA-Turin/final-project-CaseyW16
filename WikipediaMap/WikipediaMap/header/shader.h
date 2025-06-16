#ifndef SHADER_CLASS_H
#define SHADER_CLASS_H

#include<glad/glad.h>
#include<string>
#include<fstream>
#include<sstream>
#include<iostream>
#include<cerrno>

std::string GetFileContents(const char* filename);

class Shader {
public:
	GLuint ID;
	Shader(const char* vertexFile, const char* fragmentFile);

	static GLuint CreateComputeShader(const char* computeFile);
	static GLuint CreateShaderProgram(GLuint computeShader);
	void Activate();
	void Delete();
};

#endif