/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
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

