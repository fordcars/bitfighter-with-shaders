//// Copyright 2016 Carl Hewett
////
//// This file is part of SDL3D.
////
//// SDL3D is free software: you can redistribute it and/or modify
//// it under the terms of the GNU General Public License as published by
//// the Free Software Foundation, either version 3 of the License, or
//// (at your option) any later version.
////
//// SDL3D is distributed in the hope that it will be useful,
//// but WITHOUT ANY WARRANTY; without even the implied warranty of
//// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//// GNU General Public License for more details.
////
//// You should have received a copy of the GNU General Public License
//// along with SDL3D. If not, see <http://www.gnu.org/licenses/>.
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

#include <Shader.hpp>
#include <Utils.hpp>
#include <limits>

#include "glad/glad.h" // Don't include this in a header file!

// Couldn't put this in the header, sorry!
std::string getGLShaderDebugLog(U32 object, PFNGLGETSHADERIVPROC glGet_iv, PFNGLGETSHADERINFOLOGPROC glGet__InfoLog)
{
	GLint logLength; // Amount of characters
	std::string log;
	
	glGet_iv(static_cast<GLuint>(object), GL_INFO_LOG_LENGTH, &logLength); // Get size
	log.resize(logLength); // Resize string

	if(logLength)
		glGet__InfoLog(object, logLength, NULL, &log[0]);

	log.pop_back(); // Remove null terminator (\0) that OpenGL added
	return "\n-----------GL LOG-----------\n" + log; // For looks
}

// Takes the shader paths for better error logs
Shader::Shader(const std::string& name,
			   const std::string& vertexShaderPath,
			   const std::string& fragmentShaderPath)
{
	mName = name;

	std::string vertexShaderCode = Utils::getFileContents(vertexShaderPath);
	std::string fragmentShaderCode = Utils::getFileContents(fragmentShaderPath);

	U32 vertexShader = compileShader(vertexShaderPath, vertexShaderCode, GL_VERTEX_SHADER); // Is this length stuff right?
	U32 fragmentShader = compileShader(fragmentShaderPath, fragmentShaderCode, GL_FRAGMENT_SHADER);

	if(vertexShader!=0 && fragmentShader!=0) // Valid shaders
		mID = linkShaderProgram(mName, static_cast<GLuint>(vertexShader), static_cast<GLuint>(fragmentShader));
	else
		mID = 0; // Make sure it doesn't blow up. Error messages should have already been sent.

	registerUniforms(); // Will find all uniforms in the shader and register them
}

Shader::~Shader()
{
	glDeleteShader(static_cast<GLuint>(mID)); // Free memory
}

// PRIVATE

// Static
U32 Shader::compileShader(const std::string& shaderPath, const std::string& shaderCode, U32 type) // fileName for debugging
{
	GLuint shader = glCreateShader(static_cast<GLuint>(type));
	GLint shaderOk;

	std::size_t length = shaderCode.length();

	if(length>std::numeric_limits<int>::max())
	{
		Utils::CRASH("Overflow! Shader at '" + shaderPath  + "' too long! How is this possible?!");
		return 0;
	}

	int shaderLength = static_cast<int>(length);

	const char *shaderFiles[] = {shaderCode.c_str()}; // Pointer to array of pointers
	const int shaderFilesLength[] = {shaderLength}; // Array

	if(shaderLength==0) // If there is no source
	{
		Utils::CRASH("No shader source found!");
		return 0;
	}
	
	glShaderSource(shader, 1, shaderFiles, shaderFilesLength);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderOk);

	if(!shaderOk)
	{
		std::string error = "Failed to compile shader at '" + shaderPath + "'.";
		
		std::string shaderLog = getGLShaderDebugLog(shader, glGetShaderiv, glGetShaderInfoLog); // Give it the right functions
		glDeleteProgram(shader);

		Utils::LOGPRINT(shaderLog);
		Utils::CRASH(error);
		return 0;
	}

	return static_cast<U32>(shader);
}

// Static
U32 Shader::linkShaderProgram(const std::string& shaderProgramName, U32 vertexShader, U32 fragmentShader)
{
	GLint programOk;

	GLuint program = glCreateProgram();
	glAttachShader(program, static_cast<GLuint>(vertexShader));
	glAttachShader(program, static_cast<GLuint>(fragmentShader));
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &programOk);

	if(!programOk)
	{
		std::string error = "Failed to link shader program '" + shaderProgramName + "'.";
		
		std::string shaderLog = getGLShaderDebugLog(program, glGetProgramiv, glGetProgramInfoLog); // Give it the right functions
		glDeleteProgram(program);

		Utils::LOGPRINT(shaderLog);
		Utils::CRASH(error);
		return 0;
	}

	return program;
}

void Shader::registerUniforms()
{
	GLint numberOfUniforms; // Number of uniforms in the shader (linked program)
	glGetProgramiv(static_cast<GLuint>(mID), GL_ACTIVE_UNIFORMS, &numberOfUniforms);
	
	const GLsizei bufferSize = 256;
	GLchar uniformNameBuffer[bufferSize]; // Each uniform name will be read to this buffer
	GLsizei numberOfCharsReceived;

	GLint size;
	GLuint type;

	for(int i=0; i<numberOfUniforms; i++)
	{
		glGetActiveUniform(static_cast<GLuint>(mID), i, bufferSize, &numberOfCharsReceived, &size, &type, uniformNameBuffer);

		// Buffer is converted to an std::string using the null terminator placed by gl
		registerUniform(uniformNameBuffer);
	}
}

// A uniform is attached to a shader, but can be modified whenever
U32 Shader::registerUniform(const std::string& uniformName) // Uniform name is the name as in the shader
{
	GLuint uniformLocation = glGetUniformLocation(static_cast<GLuint>(mID), uniformName.c_str()); // Returns the "index" of the variable in the shader.
	
	if(uniformLocation != -1) // Valid uniform
	{
		U32MapPair uniformPair(uniformName, static_cast<U32>(uniformLocation));

		std::pair<U32Map::iterator, bool> newlyAddedPair = mUniformMap.insert(uniformPair); // Insert in map
	
		if(newlyAddedPair.second == false) // Already exists!
		{
			std::string error = "Uniform '" + uniformName + "' in shader '" + mName + "' already exists and cannot be added again!";
			Utils::CRASH(error);
			return newlyAddedPair.first->second; // Returns the uniform that was there before
		}
	} else // Invalid uniform
	{
			std::string error = "Uniform '" + uniformName + "' does not exist or is invalid in shader '" + mName + "'! Are you sure it is active (contributing to the output)?";
			Utils::CRASH(error);
	}

	return static_cast<U32>(uniformLocation); // Return the newly added uniform location
}

// PUBLIC

std::string Shader::getName() const
{
	return mName;
}

U32 Shader::getID() const
{
	return mID;
}

U32 Shader::findUniform(const std::string& uniformName) const // Returns a read only int
{
	U32Map::const_iterator got = mUniformMap.find(uniformName); // Const iterator, we should not need to change this U32

	if(got==mUniformMap.end())
	{
		std::string error = uniformName;
		error = "Uniform '" + error + "' was not registered for shader '" + mName + "'! Are you creating the right object type for your shader?";
		Utils::CRASH(error);
		return 0;
	}

	return got->second;
}
