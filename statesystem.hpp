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


#ifndef STATESYSTEM_H__
#define STATESYSTEM_H__


#include <nvgl/extensions_gl.hpp>
#include <vector>

class StateSystem {
public:

  static inline bool isBitSet(GLbitfield bits, GLuint key)
  {
    return  (bits & (1<<key)) ? true : false;
  }

  static inline void setBit(GLbitfield& bits, GLuint key)
  {
    bits |= (1<<key);
  }

  static GLbitfield getBit(GLuint key)
  {
    return (1<<key);
  }

  static inline GLboolean setBitState(GLbitfield& bits, GLuint key, GLboolean state)
  {
    if (state)  bits |=  (1<<key);
    else        bits &= ~(1<<key);
    return state;
  }
  
  static const GLuint MAX_DRAWBUFFERS    = 8;
  static const GLuint MAX_CLIPPLANES     = 8;
  static const GLuint MAX_VIEWPORTS      = 16;
  static const GLuint MAX_VERTEXATTRIBS  = 16;
  static const GLuint MAX_VERTEXBINDINGS = 16;
  static const GLuint MAX_COLORS         = 4;
    
  enum StateBits {
    BLEND,
    COLOR_LOGIC_OP,
    CULL_FACE,
    DEPTH_CLAMP,
    DEPTH_TEST,
    DITHER,
    FRAMEBUFFER_SRGB,
    LINE_SMOOTH,
    MULTISAMPLE,
    POLYGON_OFFSET_FILL,
    POLYGON_OFFSET_LINE,
    POLYGON_OFFSET_POINT,
    POLYGON_SMOOTH,
    PRIMITIVE_RESTART,
    PRIMITIVE_RESTART_FIXED_INDEX,
    RASTERIZER_DISCARD,
    SAMPLE_ALPHA_TO_COVERAGE,
    SAMPLE_ALPHA_TO_ONE,
    SAMPLE_COVERAGE,
    SAMPLE_SHADING,
    SAMPLE_MASK,
    STENCIL_TEST,
    SCISSOR_TEST,
    TEXTURE_CUBE_MAP_SEAMLESS,
    PROGRAM_POINT_SIZE,
    NUM_STATEBITS,
  };
#if STATESYSTEM_USE_DEPRECATED
  enum StateBitsDepr {
    DEPR_ALPHA_TEST,
    DEPR_LINE_STIPPLE,
    DEPR_POINT_SMOOTH,
    DEPR_POINT_SPRITE,
    DEPR_POLYGON_STIPPLE,
    NUM_STATEBITSDEPR,
  };
#endif
    
  enum Faces {
    FACE_FRONT,
    FACE_BACK,
    MAX_FACES,
  };

  //////////////////////////////////////////////////////////////////////////

  struct ClipDistanceState {
    GLbitfield  enabled;

    ClipDistanceState()
    {
      enabled = 0;
    }

    void applyGL() const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////
#if STATESYSTEM_USE_DEPRECATED
  struct AlphaStateDepr {
    GLenum    mode;
    GLfloat   refvalue;

    AlphaStateDepr()
    {
      mode      = GL_ALWAYS;
      refvalue  = 1.0;
    }

    void applyGL() const;
    void getGL();
  };
#endif
  //////////////////////////////////////////////////////////////////////////

  struct StencilOp
  {
    GLenum  fail;
    GLenum  zfail;
    GLenum  zpass;
  };
  struct StencilFunc
  {
    GLenum  func;
    GLuint  refvalue;
    GLuint  mask;
  };
  struct StencilState{
    StencilFunc funcs[MAX_FACES];
    StencilOp   ops[MAX_FACES];

    StencilState()
    {
      for (GLuint i = 0; i < MAX_FACES; i++){
        funcs[i].func = GL_ALWAYS;
        funcs[i].refvalue = 0;
        funcs[i].mask = ~0;
      }
    }

    void applyGL() const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////
  struct BlendMode{
    GLenum srcw;
    GLenum dstw;
    GLenum equ;
  };
  struct BlendStage{
    BlendMode rgb;
    BlendMode alpha;
  };
  struct BlendState{
    GLbitfield  separateEnable; // only set this if you want per draw enable
    //GLfloat     color[4];
    GLuint      useSeparate;    // if set uses per draw, otherwise first
    BlendStage  blends[MAX_DRAWBUFFERS];

    BlendState() {
      separateEnable = 0;
      useSeparate = GL_FALSE;
      for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
        blends[i].alpha.srcw = GL_ONE;
        blends[i].alpha.dstw = GL_ZERO;
        blends[i].alpha.equ  = GL_FUNC_ADD;
        blends[i].rgb = blends[i].alpha;
      }
    }

    void applyGL() const;
    void getGL();
  };
  //////////////////////////////////////////////////////////////////////////
  
  struct DepthState {
    GLenum  func;
    // depth bounds for NV?

    DepthState() {
      func = GL_LESS;
    }

    void applyGL() const;
    void getGL();
  };
  //////////////////////////////////////////////////////////////////////////
  
  struct LogicState {
    GLenum  op;

    LogicState() {
      op = GL_COPY;
    }

    void applyGL() const;
    void getGL();
  };
  //////////////////////////////////////////////////////////////////////////
  
  struct RasterState {
    //GLenum    frontFace;
    GLenum    cullFace;
    //GLfloat   polyOffsetFactor;
    //GLfloat   polyOffsetUnits;
    GLenum    polyMode;   // front and back, no separate support
    //GLfloat   lineWidth;
    GLfloat   pointSize;
    GLfloat   pointFade;
    GLenum    pointSpriteOrigin;

    RasterState() {
      //frontFace = GL_CCW;
      cullFace = GL_BACK;
      //polyOffsetFactor = 0;
      //polyOffsetUnits  = 0;
      polyMode = GL_FILL;
      //lineWidth = 1.0f;
      pointSize = 1.0f;
      pointFade = 1.0f;
      pointSpriteOrigin = GL_UPPER_LEFT;
    }

    void applyGL() const;
    void getGL();
  };

#if STATESYSTEM_USE_DEPRECATED
  struct RasterStateDepr {
    GLint     lineStippleFactor;
    GLushort  lineStipplePattern;
    GLenum    shadeModel;
    // ignore polygonStipple

    RasterStateDepr() {
      lineStippleFactor   = 1;
      lineStipplePattern  = ~0;
      shadeModel  = GL_SMOOTH;
    }

    void applyGL() const;
    void getGL();
  };
#endif

  //////////////////////////////////////////////////////////////////////////

  struct PrimitiveState {
    GLuint    restartIndex;
    GLint     patchVertices;
    GLenum    provokingVertex;

    PrimitiveState() {
      restartIndex = ~0;
      patchVertices = 3;
      provokingVertex = GL_LAST_VERTEX_CONVENTION;
    }

    void applyGL() const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////

  struct SampleState {
    GLfloat   coverage;
    GLboolean invert;
    GLuint    mask;

    SampleState() {
      coverage = 1.0;
      invert = GL_FALSE;
      mask = ~0;
    }

    void applyGL() const;
    void getGL();
  };
  //////////////////////////////////////////////////////////////////////////

  struct Viewport {
    float   x;
    float   y;
    float   width;
    float   height;
  };
  struct DepthRange {
    double  nearPlane;
    double  farPlane;
  };
  struct Scissor {
    GLint   x;
    GLint   y;
    GLsizei width;
    GLsizei height;
  };

  /*
  struct ViewportState {
    GLuint        useSeparate;  // if set uses per view, otherwise first
    Viewport      viewports[MAX_VIEWPORTS];

    ViewportState() {
      useSeparate = GL_FALSE;
      for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
        viewports[i].x = 0;
        viewports[i].y = 0;
        viewports[i].width = 0;
        viewports[i].height = 0;
      }
    }

    void applyGL() const;
    void getGL();
  };
  */

  struct DepthRangeState {
    GLuint        useSeparate;  // if set uses per view, otherwise first
    DepthRange    depths[MAX_VIEWPORTS];

    DepthRangeState() {
      useSeparate = GL_FALSE;
      for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
        depths[i].nearPlane = 0;
        depths[i].farPlane  = 1;
      }
    }

    void applyGL() const;
    void getGL();
  };

  /*
  struct ScissorState {
    GLuint        useSeparate;    // if set uses per draw, otherwise first
    Scissor       scissor[MAX_VIEWPORTS];

    ScissorState() {
      useSeparate = GL_FALSE;
      for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
        scissor[i].x = 0;
        scissor[i].y = 0;
        scissor[i].width = 0;
        scissor[i].height = 0;
      }
    }

    void applyGL() const;
    void getGL();
  };
  */

  struct ScissorEnableState {
    GLbitfield    separateEnable; // only set this if you want per view enable

    ScissorEnableState() {
      separateEnable = 0;
    }

    void applyGL() const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////

  struct MaskState {
    GLuint    colormaskUseSeparate;
    GLboolean colormask[MAX_DRAWBUFFERS][MAX_COLORS];
    GLboolean depth;
    GLuint    stencil[MAX_FACES];

    MaskState() {
      colormaskUseSeparate = GL_FALSE;
      depth = GL_TRUE;
      stencil[FACE_FRONT] = ~0;
      stencil[FACE_BACK] = ~0;
      for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
        for (GLuint c = 0; c < MAX_COLORS; c++){
          colormask[i][c] = GL_TRUE;
        }
      }
    }

    void applyGL() const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////
  
  struct FBOState {
    GLuint  fboDraw;
    GLuint  fboRead;
    GLenum  readBuffer;
    GLenum  drawBuffers[MAX_DRAWBUFFERS];
    GLuint  numBuffers;

    FBOState() {
      fboDraw = 0;
      fboRead = 0;
      readBuffer = GL_BACK;
      for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
        drawBuffers[i] = GL_NONE;
      }
      drawBuffers[0] = GL_BACK;
      numBuffers = 1;
    }

    void setFbo(GLuint fbo){
      fboDraw = fbo;
      fboRead = fbo;
      readBuffer = GL_COLOR_ATTACHMENT0;
      drawBuffers[0] = GL_COLOR_ATTACHMENT0;
      numBuffers = 1;
    }

    void applyGL(bool noBind=false) const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////

  struct VertexEnableState {
    GLbitfield    enabled;

    VertexEnableState() {
      enabled = 0;
    }

    void applyGL(GLbitfield changed=~0) const;
    void getGL();
  };

  enum VertexModeType {
    VERTEXMODE_FLOAT,
    VERTEXMODE_INT,
    VERTEXMODE_UINT,
    // ignore double and int64 for now
  };

  struct VertexFormat {
    VertexModeType  mode;

    GLboolean normalized;
    
    GLuint    size;
    GLenum    type;
    GLsizei   relativeoffset;

    GLuint    binding;
  };

  struct VertexBinding {
    GLsizei       divisor;
    GLsizei       stride;
  };

  struct VertexFormatState {
    VertexFormat  formats[MAX_VERTEXATTRIBS];
    VertexBinding bindings[MAX_VERTEXBINDINGS];

    VertexFormatState() {
      for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
        formats[i].mode           = VERTEXMODE_FLOAT;
        formats[i].size           = 4;
        formats[i].type           = GL_FLOAT;
        formats[i].normalized     = GL_FALSE;
        formats[i].relativeoffset = 0;
        formats[i].binding        = i;
      }

      for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
        bindings[i].divisor = 0;
        bindings[i].stride  = 0;
      }
    }

    void applyGL(GLbitfield changedFormat = ~0,GLbitfield changedBinding = ~0) const;
    void getGL();
  };

  struct VertexData {
    VertexModeType  mode;
    union {
      float         floats[4];
      int           ints[4];
      unsigned int  uints[4];
    };
  };

  struct VertexImmediateState {
    VertexData  data[MAX_VERTEXATTRIBS];

    VertexImmediateState() {
      for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
        data[i].mode = VERTEXMODE_FLOAT;
        data[i].floats[0] = 0;
        data[i].floats[1] = 0;
        data[i].floats[2] = 0;
        data[i].floats[3] = 1;
      }
    }

    void applyGL(GLbitfield changed = ~0) const;
    void getGL(); // ensure proper mode, otherwise will get garbage
  };

  //////////////////////////////////////////////////////////////////////////

  struct ProgramState {
    // for sake of simplicity this mechanism only support programs
    // and not program pipelines, nor use of subroutines
    GLuint    program;

    ProgramState() {
      program = 0;
    }

    void applyGL() const;
    void getGL();
  };

  //////////////////////////////////////////////////////////////////////////

  struct EnableState {
    GLbitfield      stateBits;

    EnableState() {
      stateBits = 0;
    }

    void applyGL(GLbitfield changed = ~0) const;
    void getGL();
  };

#if STATESYSTEM_USE_DEPRECATED
  struct EnableStateDepr {
    GLbitfield      stateBitsDepr;

    EnableStateDepr() {
      stateBitsDepr = 0;
    }

    void applyGL(GLbitfield changed = ~0) const;
    void getGL();
  };
#endif

  //////////////////////////////////////////////////////////////////////////
  
  struct State {
    EnableState           enable;
  #if STATESYSTEM_USE_DEPRECATED
    EnableStateDepr       enableDepr;
  #endif
    ProgramState          program;
    ClipDistanceState     clip;
  #if STATESYSTEM_USE_DEPRECATED
    AlphaStateDepr        alpha;
  #endif
    BlendState            blend;
    DepthState            depth;
    StencilState          stencil;
    LogicState            logic;
    PrimitiveState        primitive;
    SampleState           sample;
    RasterState           raster;
  #if STATESYSTEM_USE_DEPRECATED
    RasterStateDepr       rasterDepr;
  #endif
    //ViewportState         viewport;
    DepthRangeState       depthrange;
    //ScissorState          scissor;
    ScissorEnableState    scissorenable;
    MaskState             mask;
    FBOState              fbo;
    VertexEnableState     vertexenable;
    VertexFormatState     vertexformat;
    VertexImmediateState  verteximm;

    // This value only exists to ease compatibility with NV_command_list
    // and is unaffected by apply or get operations, its value
    // is set during StateSystem::set
    GLenum                basePrimitiveMode; 

    State() 
      : basePrimitiveMode(GL_TRIANGLES)
    {

    }

    void    applyGL(bool coreonly=false, bool skipFboBinding=false) const;
    void    getGL(bool coreonly=false);
  };
  
  typedef unsigned int StateID;
  static const StateID  INVALID_ID = ~0;

  void    init(bool coreonly=false);
  void    deinit();
  
  void    generate(GLuint num, StateID* objects);
  void    destroy( GLuint num, const StateID* objects );
  void          set(StateID id, const State& state, GLenum basePrimitiveMode);
  const State&  get(StateID id) const;
  
  void    applyGL(StateID id, bool skipFboBinding) const;         // brute force sets everything
  void    applyGL(StateID id, StateID prev,bool skipFboBinding);  // tries to avoid redundant, can pass INVALID_ID as previous

  void    prepareTransition(StateID id, StateID prev); // can speed up state apply
  
  
private:
  static const int MAX_DIFFS = 16;

  struct StateDiffKey{
    StateID   state;
    GLuint    changeID;
  };

  struct StateDiff {

    enum ContentBits {
      ENABLE,
      ENABLE_DEPR,
      PROGRAM,
      CLIP,
      ALPHA_DEPR,
      BLEND,
      DEPTH,
      STENCIL,
      LOGIC,
      PRIMITIVE,
      RASTER,
      RASTER_DEPR,
      //VIEWPORT,
      DEPTHRANGE,
      //SCISSOR,
      SCISSORENABLE,
      MASK,
      FBO,
      VERTEXENABLE,
      VERTEXFORMAT,
      VERTEXIMMEDIATE,
    };

    GLbitfield    changedContentBits;
    GLbitfield    changedStateBits;
    GLbitfield    changedStateDeprBits;
    GLbitfield    changedVertexEnable;
    GLbitfield    changedVertexImm;
    GLbitfield    changedVertexFormat;
    GLbitfield    changedVertexBinding;
    GLuint        pad;
  };

  struct StateInternal {
    State       state;
    GLuint      changeID;
    
    int           usedDiff;
    StateDiffKey  others[MAX_DIFFS];
    StateDiff     diffs[MAX_DIFFS];

    StateInternal() {
      changeID = 0;
    }
  };

  bool                          m_coreonly;
  std::vector<StateInternal>    m_states;
  std::vector<StateID>          m_freeIDs;

  void  makeDiff(StateDiff& diff, const StateInternal &fromInternal, const StateInternal &toInternal);
  void  applyDiffGL(const StateDiff& diff, const State &to, bool skipFboBinding);
  int   prepareTransitionCache(StateID prev, StateInternal& to );
};


#endif