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



#ifndef CULLINGSYSTEM_H__
#define CULLINGSYSTEM_H__

#include <cstddef>
#include <cstdint>
#include <nvgl/extensions_gl.hpp>


class CullingSystem {
public:
  struct Programs {
    GLuint  object_frustum;
    GLuint  object_hiz;
    GLuint  object_raster;

    GLuint  bit_temporallast;
    GLuint  bit_temporalnew;
    GLuint  bit_regular;
    GLuint  depth_mips;
  };

  enum MethodType {
    METHOD_FRUSTUM,
    METHOD_HIZ,
    METHOD_RASTER,
    NUM_METHODS,
  };

  enum BitType {
    BITS_CURRENT,
    BITS_CURRENT_AND_LAST,
    BITS_CURRENT_AND_NOT_LAST,
    NUM_BITS,
  };

  struct Buffer {
    GLuint      buffer;
    GLsizei     stride;
    GLintptr    offset;
    GLsizeiptr  size;

    void create( size_t sizei, const void* data, GLbitfield flags )
    {
      size = sizei;
      offset = 0;
      stride = 0;
      glCreateBuffers( 1, &buffer );
      glNamedBufferStorage( buffer, size, data, flags );
    }

    Buffer( GLuint buffer, size_t sizei = 0 )
      : buffer( buffer )
      , offset( 0 )
      , stride( 0 )
    {
      if (!sizei) {
        if (sizeof( GLsizeiptr ) > 4)
          glGetNamedBufferParameteri64v( buffer, GL_BUFFER_SIZE, (GLint64*)&size );
        else
          glGetNamedBufferParameteriv( buffer, GL_BUFFER_SIZE, (GLint*)&size );
      }
      else {
        size = sizei;
      }
    }

    Buffer()
      : buffer(0)
      , stride(0)
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
    inline void ClearBufferSubData(GLenum target,GLenum internalformat,GLenum format,GLenum type,const GLvoid* data) const {
      glClearBufferSubData(target,internalformat,offset,size,format,type,data);
    }

  };
  
  class Job {
  public:
    int     m_numObjects;
      // world-space matrices {mat4 world, mat4 worldInverseTranspose}
    Buffer  m_bufferMatrices;
    Buffer  m_bufferBboxes; // only used in dualindex mode (2 x vec4)
      // 1 32-bit integer per object (index)
    Buffer  m_bufferObjectMatrix;
      // object-space bounding box (2 x vec4)
      // or 1 32-bit integer per object (dualindex mode)
    Buffer  m_bufferObjectBbox;
    
      // 1 32-bit integer per object
    Buffer  m_bufferVisOutput;
    
      // 1 32-bit integer per 32 objects (1 bit per object)
    Buffer  m_bufferVisBitsCurrent;
    Buffer  m_bufferVisBitsLast;
    
      // for HiZ
    GLuint  m_textureDepthWithMipmaps;

    // derive from this class and implement this function how you want to
    // deal with the results that are provided in the buffer
    virtual void resultFromBits( const Buffer& bufferVisBitsCurrent ) = 0;
    // for readback methods we need to wait for a result
    virtual void resultClient() {};

  };

  class JobReadback : public Job {
  public:
    // 1 32-bit integer per 32 objects (1 bit per object)
    Buffer      m_bufferVisBitsReadback;
    uint32_t*   m_hostVisBits;

    // Do not use this Job class unless you have to. Persistent 
    // mapped buffers are preferred.

    // Copies result into readback buffer
    void resultFromBits( const Buffer& bufferVisBitsCurrent );

    // getBufferData into hostVisBits (blocking!)
    void resultClient();
  };

  class JobReadbackPersistent : public Job {
  public:
    // 1 32-bit integer per 32 objects (1 bit per object)
    Buffer      m_bufferVisBitsReadback;
    void*       m_bufferVisBitsMapping;
    uint32_t*   m_hostVisBits;
    GLsync      m_fence;

    // Copies result into readback buffer and records
    // a fence.
    void resultFromBits(const Buffer& bufferVisBitsCurrent);

    // waits on fence and copies mapping into hostVisBits
    void resultClient();
  };

  // multidrawindirect based
  class JobIndirectUnordered : public Job {
  public:
    GLuint  m_program_indirect_compact;
    // 1 indirectSize per object, 
    Buffer  m_bufferObjectIndirects;
    Buffer  m_bufferIndirectResult;
    // 1 integer
    Buffer  m_bufferIndirectCounter;

    void resultFromBits( const Buffer& bufferVisBitsCurrent );
  };
  
  struct View {
    const float*  viewProjMatrix;
    const float*  viewDir;
    const float*  viewPos;
  };
  
  void init( const Programs &programs, bool dualindex );
  void deinit();
  void update( const Programs &programs, bool dualindex );
  
  // helper function for HiZ method, leaves fbo bound to 0
  void buildDepthMipmaps(GLuint textureDepth, int width, int height);
  
  // assumes relevant fbo bound for raster method
  void buildOutput( MethodType  method, Job &job, const View& view );

  void bitsFromOutput ( Job &job, BitType type );
  void resultFromBits ( Job &job );
  void resultClient   ( Job &job );

  // swaps the Current/Last bit array (for temporal coherent techniques)
  void swapBits       ( Job &job );

private:

  struct Uniforms {
    GLint   depth_lod;
    GLint   depth_even;
    GLint   frustum_viewProj;
    GLint   hiz_viewProj;
    GLint   raster_viewProj;
    GLint   raster_viewDir;
    GLint   raster_viewPos;
  };

  void testBboxes( Job &job, bool raster);
  
  Programs  m_programs;
  Uniforms  m_uniforms;
  GLuint    m_fbo;
  GLuint    m_tbo[2];
  bool      m_dualindex;
  bool      m_useSSBO;
  bool      m_useRepesentativeTest;
};

#endif
