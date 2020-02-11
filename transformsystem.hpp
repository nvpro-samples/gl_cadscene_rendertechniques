/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#ifndef TRANSFORMSYSTEM_H__
#define TRANSFORMSYSTEM_H__


#include <nvgl/extensions_gl.hpp>
#include <cstddef>

#include "nodetree.hpp"

class TransformSystem {
public:

  struct Programs {
    GLuint  transform_level;
    GLuint  transform_leaves;
  };

  struct Buffer {
    GLuint      buffer;
    GLintptr    offset;
    GLsizeiptr  size;

    Buffer(GLuint buffer, size_t sizei=0)
      : buffer(buffer)
      , offset(0)
    {
      glBindBuffer(GL_COPY_READ_BUFFER, buffer);
      if (!sizei){
        if (sizeof(GLsizeiptr) > 4)
          glGetBufferParameteri64v(GL_COPY_READ_BUFFER,GL_BUFFER_SIZE, (GLint64*)&size);
        else
          glGetBufferParameteriv(GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, (GLint*)&size);
        glBindBuffer(GL_COPY_READ_BUFFER, 0);
      }
      else{
        size = sizei;
      }
    }

    Buffer()
      : buffer(0)
      , offset(0)
      , size(0)
    {

    }

    inline void BindBufferRange(GLenum target, GLuint index) const {
      glBindBufferRange(target, index, buffer, offset, size);
    }
    inline void TexBuffer(GLenum target, GLenum internalformat) const {
      glTexBufferRange(target, internalformat, buffer, offset, size);
    }
   
  };
  
  void init( const Programs &programs );
  void deinit();
  void update( const Programs &programs );
  
  void process(const NodeTree&, Buffer& ids, Buffer& matricesObject, Buffer& matricesWorld );
  
private:

  enum Textures {
    TEXTURE_IDS,
    TEXTURE_WORLD,
    TEXTURE_OBJECT,
    TEXTURES,
  };

  GLuint    m_leavesGroup;
  GLuint    m_levelsGroup;

  Programs  m_programs;
  GLuint    m_scratchGL;
  GLuint    m_texsGL[TEXTURES];
};

#endif

