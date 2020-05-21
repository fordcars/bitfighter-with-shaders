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

#ifndef SHADER_HPP
#define SHADER_HPP

// Don't include glad here!!
// This is because we will include Shader.hpp in files already including ogl.h.

#include "tnlTypes.h"

#include <map>
#include <string>

using namespace TNL;

class Shader
{
private:
	using U32Map = std::map<std::string, U32>;
	using U32MapPair = std::pair<std::string, U32>;

	std::string mName; // Useful for error messages, don't change this stupidly

	U32 mID; // the ID of the shader, give this to OpenGL stuff. Could be const, but I left it non-const to make things easier.
	U32Map mUniformMap; // Uniform variables, uniforms[uniformName] = uniform location

	// Static because they do not need an instance to work
	static U32 compileShader(const std::string& shaderPath, const std::string& shaderCode, U32 type);
	static U32 linkShaderProgram(const std::string& shaderProgramName, U32 vertexShader, U32 fragmentShader);

	void registerUniforms();
	U32 registerUniform(const std::string& uniformName);

public:
	Shader(const std::string& name, const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
	~Shader();

	std::string getName() const;
	U32 getID() const;

	U32 findUniform(const std::string& uniformName) const;
};

#endif /* SHADER_HPP */
