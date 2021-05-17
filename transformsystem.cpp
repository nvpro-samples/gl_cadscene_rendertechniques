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

#include <assert.h>

#include "transformsystem.hpp"
#include <nvgl/base_gl.hpp>

void TransformSystem::process(const NodeTree& nodeTree, Buffer& ids, Buffer& matricesObject, Buffer& matricesWorld )
{
  glUseProgram(m_programs.transform_leaves);

  glBindBuffer    (GL_SHADER_STORAGE_BUFFER,  m_scratchGL);
  glBufferData    (GL_SHADER_STORAGE_BUFFER,  sizeof(GLuint)*nodeTree.getNumActiveNodes(),NULL,GL_STREAM_DRAW);

#if 0
  // APIC hack
  glTextureBufferEXT(m_texsGL[TEXTURE_IDS],   GL_TEXTURE_BUFFER, GL_R32I,    ids.buffer);
  glTextureBufferEXT(m_texsGL[TEXTURE_OBJECT],GL_TEXTURE_BUFFER, GL_RGBA32F, matricesObject.buffer);
  glTextureBufferEXT(m_texsGL[TEXTURE_WORLD], GL_TEXTURE_BUFFER, GL_RGBA32F, matricesWorld.buffer);
#else
  glTextureBufferRange(m_texsGL[TEXTURE_IDS],     GL_R32I, ids.buffer, ids.offset, ids.size);
  glTextureBufferRange(m_texsGL[TEXTURE_OBJECT],  GL_RGBA32F, matricesObject.buffer, matricesObject.offset, matricesObject.size);
  glTextureBufferRange(m_texsGL[TEXTURE_WORLD],   GL_RGBA32F, matricesWorld.buffer, matricesWorld.offset, matricesWorld.size);
#endif

  for (int i = 0; i < TEXTURES; i++){
    nvgl::bindMultiTexture(GL_TEXTURE0 + i, GL_TEXTURE_BUFFER, m_texsGL[i]);
  }

  matricesWorld.BindBufferRange(GL_SHADER_STORAGE_BUFFER,0);
  matricesObject.BindBufferRange(GL_SHADER_STORAGE_BUFFER,1);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,m_scratchGL);

  const int maxshaderlevels = 10;
  int maxlevels = maxshaderlevels;
  int totalNodes = 0;
  bool useLeaves = true;

  int currentDepth = 1;
  const NodeTree::Level* level = nodeTree.getUsedLevel(currentDepth);

  // TODO:
  //
  // This code lacks a proper heuristic for switching between level and leaves based processing.
  // One should prefer level if there is enough nodes per level, otherwise descend and gather 
  // many leaves from multiple levels.
  //
  while (level){
    // dispatch on last level, or if we have reached maxlevels
    bool willdispatch = currentDepth && (!nodeTree.getUsedLevel(currentDepth+1) || currentDepth+1 % maxlevels == 0);

    // the last level in leaf mode, must use all level nodes, and not just the leaves of this level
    // as subsequent leaves operate in level mode
    const std::vector<NodeTree::nodeID>& nodes = useLeaves && !willdispatch ? level->leaves : level->nodes;

    if (!nodes.empty()){
      glBufferSubData(GL_SHADER_STORAGE_BUFFER,totalNodes*sizeof(GLuint),sizeof(GLuint)*nodes.size(),&nodes[0]);
      totalNodes += (int)nodes.size();
    }

    currentDepth++;
    level = nodeTree.getUsedLevel(currentDepth);
    if (willdispatch){
      int groupsize = useLeaves ? m_leavesGroup : m_levelsGroup;
      if (useLeaves){
        glUniform1i(0,totalNodes);
        glUniform1i(1,1);
      }
      else{
        glUniform1i(0,totalNodes);
      }
      
      glDispatchCompute((totalNodes+groupsize-1)/groupsize,1,1);
      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

      if (useLeaves){
        // switch to per-level mode after first batch of leaves is over (tip of hierarchy)
        glUseProgram(m_programs.transform_level);
        useLeaves = false;
        maxlevels = 1; // assure we dispatch every level
      }

      totalNodes = 0;
    }
  }

  glUseProgram(0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,1,0);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER,2,0);

  for (int i = 0; i < TEXTURES; i++){
    nvgl::bindMultiTexture(GL_TEXTURE0 + i, GL_TEXTURE_BUFFER, 0);
  }
  
}

void TransformSystem::init( const Programs &programs )
{
  m_programs = programs;
  glCreateBuffers(1,&m_scratchGL);
  glCreateTextures(GL_TEXTURE_BUFFER, TEXTURES, m_texsGL);
}

void TransformSystem::deinit()
{
  glDeleteBuffers(1,&m_scratchGL);
  glDeleteTextures(TEXTURES,m_texsGL);
}

void TransformSystem::update( const Programs &programs )
{
  m_programs = programs;

  GLuint groupsizes[3];
  glGetProgramiv(programs.transform_leaves, GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)groupsizes);
  m_leavesGroup = groupsizes[0];

  glGetProgramiv(programs.transform_level, GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)groupsizes);
  m_levelsGroup = groupsizes[0];
}


