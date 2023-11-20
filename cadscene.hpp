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


#ifndef CADSCENE_H__
#define CADSCENE_H__

#include <cstring> // memset
#include <nvgl/extensions_gl.hpp>
#include <glm/glm.hpp>
#include <vector>
#include "nodetree.hpp"

class CadScene {

public:

  struct BBox {
    glm::vec4    min;
    glm::vec4    max;

    BBox() : min(FLT_MAX), max(-FLT_MAX) {}

    inline void merge( const glm::vec4& point )
    {
      min = glm::min(min, point);
      max = glm::max(max, point);
    }

    inline void merge( const BBox& bbox )
    {
      min = glm::min(min, bbox.min);
      max = glm::max(max, bbox.max);
    }

    inline BBox transformed ( const glm::mat4 &matrix, int dim=3)
    {
      int i;
      glm::vec4 box[16];
      // create box corners
      box[0] = glm::vec4(min.x,min.y,min.z,min.w);
      box[1] = glm::vec4(max.x,min.y,min.z,min.w);
      box[2] = glm::vec4(min.x,max.y,min.z,min.w);
      box[3] = glm::vec4(max.x,max.y,min.z,min.w);
      box[4] = glm::vec4(min.x,min.y,max.z,min.w);
      box[5] = glm::vec4(max.x,min.y,max.z,min.w);
      box[6] = glm::vec4(min.x,max.y,max.z,min.w);
      box[7] = glm::vec4(max.x,max.y,max.z,min.w);

      box[8] = glm::vec4(min.x,min.y,min.z,max.w);
      box[9] = glm::vec4(max.x,min.y,min.z,max.w);
      box[10] = glm::vec4(min.x,max.y,min.z,max.w);
      box[11] = glm::vec4(max.x,max.y,min.z,max.w);
      box[12] = glm::vec4(min.x,min.y,max.z,max.w);
      box[13] = glm::vec4(max.x,min.y,max.z,max.w);
      box[14] = glm::vec4(min.x,max.y,max.z,max.w);
      box[15] = glm::vec4(max.x,max.y,max.z,max.w);

      // transform box corners
      // and find new mins,maxs
      BBox bbox;

      for (i = 0; i < (1<<dim) ; i++){
        glm::vec4 point = matrix * box[i];
        bbox.merge(point);
      }

      return bbox;
    }
  };

  struct MaterialSide {
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    glm::vec4 emissive;
  };

  // need to keep this 256 byte aligned (UBO range)
  struct Material {
    MaterialSide  sides[2];
    GLuint64      texturesADDR[4];
    GLuint        textures[4];
    GLuint        _pad[4+16];

    Material() {
      memset(this,0,sizeof(Material));
    }
  };

  // need to keep this 256 byte aligned (UBO range)
  struct MatrixNode {
    glm::mat4  worldMatrix;
    glm::mat4  worldMatrixIT;
    glm::mat4  objectMatrix;
    glm::mat4  objectMatrixIT;
  };

  struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
  };

  struct DrawRange {
    size_t        offset;
    int           count;

    DrawRange() : offset(0) , count(0) {}
  };

  struct DrawStateInfo {
    int           materialIndex;
    int           matrixIndex;

    friend bool operator != ( const DrawStateInfo &lhs,  const DrawStateInfo &rhs){
      return lhs.materialIndex != rhs.materialIndex || lhs.matrixIndex != rhs.matrixIndex;
    }

    friend bool operator == ( const DrawStateInfo &lhs,  const DrawStateInfo &rhs){
      return lhs.materialIndex == rhs.materialIndex && lhs.matrixIndex == rhs.matrixIndex;
    }
  };

  struct DrawRangeCache {
    std::vector<DrawStateInfo>    state;
    std::vector<int>          stateCount;

    std::vector<size_t>       offsets;
    std::vector<int>          counts;
  };

  struct GeometryPart {
    DrawRange     indexSolid;
    DrawRange     indexWire;
  };

  struct Geometry {
    GLuint    vboGL;
    GLuint    iboGL;
    GLuint64  vboADDR;
    GLuint64  iboADDR;
    size_t    vboSize;
    size_t    iboSize;

    std::vector<GeometryPart> parts;

    int       numVertices;
    int       numIndexSolid;
    int       numIndexWire;
    
    int       cloneIdx;
  };

  struct ObjectPart {
    int   active;
    int   materialIndex;
    int   matrixIndex;
  };

  struct Object {
    int             matrixIndex;
    int             geometryIndex;

    std::vector<ObjectPart> parts;

    DrawRangeCache  cacheSolid;
    DrawRangeCache  cacheWire;
  };

  std::vector<Material>       m_materials;
  std::vector<BBox>           m_geometryBboxes;
  std::vector<Geometry>       m_geometry;
  std::vector<MatrixNode>     m_matrices;
  std::vector<Object>         m_objects;
  std::vector<glm::ivec2>  m_objectAssigns;


  BBox      m_bbox;

  GLuint    m_materialsGL;
  GLuint64  m_materialsADDR;
  GLuint    m_matricesGL;
  GLuint64  m_matricesADDR;
  GLuint    m_matricesTexGL;
  GLuint64  m_matricesTexGLADDR;
  GLuint    m_geometryBboxesGL;
  GLuint    m_geometryBboxesTexGL;
  GLuint    m_objectAssignsGL;

  GLuint    m_parentIDsGL;

  GLuint    m_matricesOrigGL;
  GLuint    m_matricesOrigTexGL;

  NodeTree  m_nodeTree;

  void  updateObjectDrawCache(Object& object);
  
  bool  loadCSF(const char* filename, int clones = 0, int cloneaxis=3);
  void  unload();

  static void enableVertexFormat(int attrPos, int attrNormal);
  static void disableVertexFormat(int attrPos, int attrNormal);
  void resetMatrices();
};


#endif

