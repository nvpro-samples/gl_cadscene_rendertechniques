/*-----------------------------------------------------------------------
  Copyright (c) 2014, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include "statesystem.hpp"


//////////////////////////////////////////////////////////////////////////

void StateSystem::ClipDistanceState::applyGL() const
{
  for (GLuint i = 0; i < MAX_CLIPPLANES; i++){
    if (isBitSet(enabled,i))  glEnable  (GL_CLIP_DISTANCE0 + i);
    else                      glDisable (GL_CLIP_DISTANCE0 + i);
  }
}

void StateSystem::ClipDistanceState::getGL()
{
  enabled = 0;
  for (GLuint i = 0; i < MAX_CLIPPLANES; i++){
    setBitState(enabled,i,glIsEnabled(GL_CLIP_DISTANCE0 + i));
  }
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::AlphaStateDepr::applyGL() const
{
  glAlphaFunc(mode,refvalue);
}

void StateSystem::AlphaStateDepr::getGL()
{
  glGetIntegerv(GL_ALPHA_TEST_FUNC,(GLint*)&mode);
  glGetFloatv(GL_ALPHA_TEST_REF, &refvalue);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::StencilState::applyGL() const
{
  glStencilFuncSeparate(GL_FRONT, funcs[FACE_FRONT].func, funcs[FACE_FRONT].refvalue, funcs[FACE_FRONT].mask);
  glStencilFuncSeparate(GL_BACK,  funcs[FACE_BACK ].func, funcs[FACE_BACK ].refvalue, funcs[FACE_BACK ].mask);
  glStencilOpSeparate(GL_FRONT,   ops[FACE_FRONT].fail,   ops[FACE_FRONT].zfail,      ops[FACE_FRONT].zpass);
  glStencilOpSeparate(GL_BACK,    ops[FACE_BACK ].fail,   ops[FACE_BACK ].zfail,      ops[FACE_BACK ].zpass);
}

void StateSystem::StencilState::getGL()
{
  glGetIntegerv(GL_STENCIL_FUNC,        (GLint*)&funcs[FACE_FRONT].func);
  glGetIntegerv(GL_STENCIL_REF,         (GLint*)&funcs[FACE_FRONT].refvalue);
  glGetIntegerv(GL_STENCIL_VALUE_MASK,  (GLint*)&funcs[FACE_FRONT].mask);

  glGetIntegerv(GL_STENCIL_BACK_FUNC,         (GLint*)&funcs[FACE_BACK].func);
  glGetIntegerv(GL_STENCIL_BACK_REF,          (GLint*)&funcs[FACE_BACK].refvalue);
  glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK,   (GLint*)&funcs[FACE_BACK].mask);

  glGetIntegerv(GL_STENCIL_FAIL,              (GLint*)&ops[FACE_FRONT].fail);
  glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL,   (GLint*)&ops[FACE_FRONT].zfail);
  glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS,   (GLint*)&ops[FACE_FRONT].zpass);

  glGetIntegerv(GL_STENCIL_BACK_FAIL,             (GLint*)&ops[FACE_BACK].fail);
  glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL,  (GLint*)&ops[FACE_BACK].zfail);
  glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS,  (GLint*)&ops[FACE_BACK].zpass);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::BlendState::applyGL() const
{
  if (separateEnable){
    for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
      if (isBitSet(separateEnable,i)) glEnablei(GL_BLEND,i);
      else                            glDisablei(GL_BLEND,i);
    }
  }

  if (useSeparate){
    for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
      glBlendFuncSeparatei(i,blends[i].rgb.srcw,blends[i].rgb.dstw,blends[i].alpha.srcw,blends[i].alpha.dstw);
      glBlendEquationSeparatei(i,blends[i].rgb.equ,blends[i].alpha.equ);
    }
  }
  else{
    glBlendFuncSeparate(blends[0].rgb.srcw,blends[0].rgb.dstw,blends[0].alpha.srcw,blends[0].alpha.dstw);
    glBlendEquationSeparate(blends[0].rgb.equ,blends[0].alpha.equ);
  }

  //glBlendColor(color[0],color[1],color[2],color[3]);
}

void StateSystem::BlendState::getGL()
{
  GLuint stateSet = 0;
  separateEnable = 0;
  for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
    if (setBitState(separateEnable,i, glIsEnabledi( GL_BLEND, i))) stateSet++;
  }
  if (stateSet == MAX_DRAWBUFFERS){
    separateEnable = 0;
  }

  GLuint numEqual = 1;
  for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
    glGetIntegeri_v(GL_BLEND_SRC_RGB,i,(GLint*)&blends[i].rgb.srcw);
    glGetIntegeri_v(GL_BLEND_DST_RGB,i,(GLint*)&blends[i].rgb.dstw);
    glGetIntegeri_v(GL_BLEND_EQUATION_RGB,i,(GLint*)&blends[i].rgb.equ);

    glGetIntegeri_v(GL_BLEND_SRC_ALPHA,i,(GLint*)&blends[i].alpha.srcw);
    glGetIntegeri_v(GL_BLEND_DST_ALPHA,i,(GLint*)&blends[i].alpha.dstw);
    glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA,i,(GLint*)&blends[i].alpha.equ);

    if (i > 1 && memcmp(&blends[i].rgb,&blends[i-1].rgb,sizeof(blends[i].rgb))==0 && memcmp(&blends[i].alpha,&blends[i-1].alpha,sizeof(blends[i].alpha))==0){
      numEqual++;
    }
  }

  useSeparate = numEqual != MAX_DRAWBUFFERS;

  //glGetFloatv(GL_BLEND_COLOR,color);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::DepthState::applyGL() const
{
  glDepthFunc(func);
}

void StateSystem::DepthState::getGL()
{
  glGetIntegerv(GL_DEPTH_FUNC,(GLint*)&func);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::LogicState::applyGL() const
{
  glLogicOp(op);
}

void StateSystem::LogicState::getGL()
{
  glGetIntegerv(GL_LOGIC_OP_MODE,(GLint*)&op);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::RasterState::applyGL() const
{
  //glFrontFace(frontFace);
  glCullFace(cullFace);
  //glPolygonOffset(polyOffsetFactor,polyOffsetUnits);
  glPolygonMode(GL_FRONT_AND_BACK,polyMode);
  //glLineWidth(lineWidth);
  glPointSize(pointSize);
  glPointParameterf(GL_POINT_FADE_THRESHOLD_SIZE,pointFade);
  glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN,pointSpriteOrigin);
}

void StateSystem::RasterState::getGL()
{
  //glGetIntegerv(GL_FRONT_FACE, (GLint*)&frontFace);
  glGetIntegerv(GL_CULL_FACE_MODE, (GLint*)&cullFace);
  //glGetFloatv(GL_POLYGON_OFFSET_FACTOR,&polyOffsetFactor);
  //glGetFloatv(GL_POLYGON_OFFSET_UNITS,&polyOffsetUnits);
  //glGetFloatv(GL_LINE_WIDTH,&lineWidth);
  glGetFloatv(GL_POINT_SIZE,&pointSize);
  glGetFloatv(GL_POINT_FADE_THRESHOLD_SIZE,&pointFade);
  glGetIntegerv(GL_POINT_SPRITE_COORD_ORIGIN,(GLint*)&pointSpriteOrigin);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::RasterStateDepr::applyGL() const
{
  glLineStipple(lineStippleFactor,lineStipplePattern);
  glShadeModel(shadeModel);
}

void StateSystem::RasterStateDepr::getGL()
{
  GLint pattern;
  glGetIntegerv(GL_LINE_STIPPLE_PATTERN,&pattern);
  lineStipplePattern = pattern;
  glGetIntegerv(GL_LINE_STIPPLE_REPEAT,(GLint*)&lineStippleFactor);
  glGetIntegerv(GL_SHADE_MODEL,(GLint*)&shadeModel);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::PrimitiveState::applyGL() const
{
  glPrimitiveRestartIndex(restartIndex);
  glProvokingVertex(provokingVertex);
  glPatchParameteri(GL_PATCH_VERTICES,patchVertices);
}

void StateSystem::PrimitiveState::getGL()
{
  glGetIntegerv(GL_PRIMITIVE_RESTART_INDEX, (GLint*)&restartIndex);
  glGetIntegerv(GL_PROVOKING_VERTEX, (GLint*)&provokingVertex);
  glGetIntegerv(GL_PATCH_VERTICES, (GLint*)&patchVertices);
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::SampleState::applyGL() const
{
  glSampleCoverage(coverage,invert);
  glSampleMaski(0,mask);
}

void StateSystem::SampleState::getGL()
{
  glGetIntegerv(GL_SAMPLE_COVERAGE_VALUE,(GLint*)&coverage);
  glGetIntegerv(GL_SAMPLE_COVERAGE_INVERT,(GLint*)&invert);
  glGetIntegeri_v(GL_SAMPLE_MASK_VALUE,0,(GLint*)&mask);
}

//////////////////////////////////////////////////////////////////////////
/*
void StateSystem::ViewportState::applyGL() const
{
  if (useSeparate){
    glViewportArrayv(0,MAX_VIEWPORTS, &viewports[0].x);
  }
  else{
    glViewport(GLint(viewports[0].x),GLint(viewports[0].y),GLsizei(viewports[0].width),GLsizei(viewports[0].height));
  }
}

void StateSystem::ViewportState::getGL()
{
  int numEqual = 1;
  for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
    glGetFloati_v(GL_VIEWPORT,i,&viewports[i].x);
    if (i > 0 && memcmp(&viewports[i],&viewports[i-1],sizeof(viewports[i]))==0){
      numEqual++;
    }
  }
  
  useSeparate = (numEqual != MAX_VIEWPORTS);
}
*/
//////////////////////////////////////////////////////////////////////////

void StateSystem::DepthRangeState::applyGL() const
{
  if (useSeparate){
    glDepthRangeArrayv(0,MAX_VIEWPORTS, &depths[0].nearPlane);
  }
  else{
    glDepthRange(depths[0].nearPlane,depths[0].farPlane);
  }
}

void StateSystem::DepthRangeState::getGL()
{
  GLuint numEqual = 1;
  for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
    glGetDoublei_v(GL_DEPTH_RANGE,i,&depths[i].nearPlane);
    if (i > 0 && memcmp(&depths[i],&depths[i-1],sizeof(depths[i]))==0){
      numEqual++;
    }
  }

  useSeparate = (numEqual != MAX_VIEWPORTS);
}

//////////////////////////////////////////////////////////////////////////
/*
void StateSystem::ScissorState::applyGL() const
{
  if (useSeparate){
    glScissorArrayv(0,MAX_VIEWPORTS, &scissor[0].x);
  }
  else{
    glScissor(scissor[0].x,scissor[0].y,scissor[0].width,scissor[0].height);
  }
}

void StateSystem::ScissorState::getGL()
{
  GLuint numEqual = 1;
  for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
    glGetIntegeri_v(GL_SCISSOR_BOX,i,&scissor[i].x);
    if (i > 0 && memcmp(&scissor[i],&scissor[i-1],sizeof(scissor[i]))==0){
      numEqual++;
    }
  }

  useSeparate = (numEqual != MAX_VIEWPORTS);
}
*/
//////////////////////////////////////////////////////////////////////////

void StateSystem::ScissorEnableState::applyGL() const
{
  if (separateEnable){
    for (GLuint i = 0; i < MAX_VIEWPORTS; i++){
      if (isBitSet(separateEnable,i))  glEnablei (GL_SCISSOR_TEST,i);
      else                                    glDisablei(GL_SCISSOR_TEST,i);
    }
  }

}

void StateSystem::ScissorEnableState::getGL()
{
  GLuint stateSet = 0;
  separateEnable = 0;
  for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
    if (setBitState(separateEnable,i, glIsEnabledi( GL_BLEND, i))) stateSet++;
  }
  if (stateSet == MAX_DRAWBUFFERS){
    separateEnable = 0;
  }
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::MaskState::applyGL() const
{
  if (colormaskUseSeparate){
    for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
      glColorMaski(i, colormask[i][0],colormask[i][1],colormask[i][2],colormask[i][3]);
    }
  }
  else{
    glColorMask( colormask[0][0],colormask[0][1],colormask[0][2],colormask[0][3] );
  }
  glDepthMask(depth);
  glStencilMaskSeparate(GL_FRONT, stencil[FACE_FRONT]);
  glStencilMaskSeparate(GL_BACK,  stencil[FACE_BACK]);
}

void StateSystem::MaskState::getGL()
{
  glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);
  glGetIntegerv(GL_STENCIL_WRITEMASK, (GLint*)&stencil[FACE_FRONT]);
  glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, (GLint*)&stencil[FACE_BACK]);

  int numEqual = 1;
  for (GLuint i = 0; i < MAX_DRAWBUFFERS; i++){
    glGetBooleani_v(GL_COLOR_WRITEMASK, i, colormask[i]);

    if ( i > 0 && memcmp(colormask[i],colormask[i-1],sizeof(colormask[i]))==0){
      numEqual++;
    }
  }

  colormaskUseSeparate = numEqual != MAX_DRAWBUFFERS;
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::FBOState::applyGL(bool skipFboBinding) const
{
  if (!skipFboBinding){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fboDraw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,fboRead);
  }
  glDrawBuffers(numBuffers,drawBuffers);
  glReadBuffer(readBuffer);
}

void StateSystem::FBOState::getGL()
{
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,(GLint*)&fboDraw);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,(GLint*)&fboRead);

  glGetIntegerv(GL_READ_BUFFER,(GLint*)&readBuffer);

  for (int i = 0; i < MAX_DRAWBUFFERS; i++){
    glGetIntegerv(GL_DRAW_BUFFER0 + i,(GLint*)&drawBuffers[i]);
    if (drawBuffers[i] != GL_NONE){
      numBuffers = i+1;
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::VertexEnableState::applyGL(GLbitfield changed) const
{
  for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
    if (isBitSet(changed,i)){
      if (isBitSet(enabled,i))  glEnableVertexAttribArray(i);
      else                      glDisableVertexAttribArray(i);
    }
  }
}

void StateSystem::VertexEnableState::getGL()
{
  enabled = 0;
  for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
    GLint status;
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED, (GLint*)&status);
    setBitState(enabled,i, status);
  }
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::VertexFormatState::applyGL(GLbitfield changedFormat, GLbitfield changedBinding) const
{
  for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
    if (!isBitSet(changedFormat,i)) continue;

    switch(formats[i].mode){
    case VERTEXMODE_FLOAT:
      glVertexAttribFormat(i, formats[i].size, formats[i].type, formats[i].normalized, formats[i].relativeoffset);
      break;
    case VERTEXMODE_INT:
    case VERTEXMODE_UINT:
      glVertexAttribIFormat(i, formats[i].size, formats[i].type, formats[i].relativeoffset);
      break;
    }
    glVertexAttribBinding(i,formats[i].binding);
  }

  for (GLuint i = 0; i < MAX_VERTEXBINDINGS; i++){
    if (!isBitSet(changedBinding,i)) continue;

    glVertexBindingDivisor(i,bindings[i].divisor);
    glBindVertexBuffer(i,0,0,bindings[i].stride);
  }
}

void StateSystem::VertexFormatState::getGL()
{
  for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
    GLint status = 0;
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_RELATIVE_OFFSET, (GLint*)&formats[i].relativeoffset);
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_SIZE, (GLint*)&formats[i].size);
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint*)&formats[i].type);
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint*)&status);
    formats[i].normalized = status;
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_ARRAY_INTEGER, (GLint*)&status);
    if (status){
      formats[i].mode = VERTEXMODE_INT;
    }
    else{
      formats[i].mode = VERTEXMODE_FLOAT;
    }
    glGetVertexAttribiv(i,GL_VERTEX_ATTRIB_BINDING, (GLint*)&formats[i].binding);
  }

  for (GLuint i = 0; i < MAX_VERTEXBINDINGS; i++){
    glGetIntegeri_v(GL_VERTEX_BINDING_DIVISOR,i,(GLint*)&bindings[i].divisor);
    glGetIntegeri_v(GL_VERTEX_BINDING_STRIDE, i,(GLint*)&bindings[i].stride);
  }
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::VertexImmediateState::applyGL(GLbitfield changed) const
{
  for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
    if (!isBitSet(changed,i)) continue;

    switch(data[i].mode){
    case VERTEXMODE_FLOAT:
      glVertexAttrib4fv(i,data[i].floats);
      break;
    case VERTEXMODE_INT:
      glVertexAttribI4iv(i,data[i].ints);
      break;
    case VERTEXMODE_UINT:
      glVertexAttribI4uiv(i,data[i].uints);
      break;
    }
  }
}

void StateSystem::VertexImmediateState::getGL()
{
  for (GLuint i = 0; i < MAX_VERTEXATTRIBS; i++){
    switch(data[i].mode){
    case VERTEXMODE_FLOAT:
      glGetVertexAttribfv(i,GL_CURRENT_VERTEX_ATTRIB,data[i].floats);
      break;
    case VERTEXMODE_INT:
      glGetVertexAttribIiv(i,GL_CURRENT_VERTEX_ATTRIB,data[i].ints);
      break;
    case VERTEXMODE_UINT:
      glGetVertexAttribIuiv(i,GL_CURRENT_VERTEX_ATTRIB,data[i].uints);
      break;
    }
  }
}

//////////////////////////////////////////////////////////////////////////

void StateSystem::ProgramState::applyGL() const
{
  glUseProgram(program);
}

void StateSystem::ProgramState::getGL()
{
  glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&program);
}

//////////////////////////////////////////////////////////////////////////

// keep in sync!
static GLenum s_stateEnums[StateSystem::NUM_STATEBITS] = {
  GL_BLEND,
  GL_COLOR_LOGIC_OP,
  GL_CULL_FACE,
  GL_DEPTH_CLAMP,
  GL_DEPTH_TEST,
  GL_DITHER,
  GL_FRAMEBUFFER_SRGB,
  GL_LINE_SMOOTH,
  GL_MULTISAMPLE,
  GL_POLYGON_OFFSET_FILL,
  GL_POLYGON_OFFSET_LINE,
  GL_POLYGON_OFFSET_POINT,
  GL_POLYGON_SMOOTH,
  GL_PRIMITIVE_RESTART,
  GL_PRIMITIVE_RESTART_FIXED_INDEX,
  GL_RASTERIZER_DISCARD,
  GL_SAMPLE_ALPHA_TO_COVERAGE,
  GL_SAMPLE_ALPHA_TO_ONE,
  GL_SAMPLE_COVERAGE,
  GL_SAMPLE_SHADING,
  GL_SAMPLE_MASK,
  GL_STENCIL_TEST,
  GL_SCISSOR_TEST,
  GL_TEXTURE_CUBE_MAP_SEAMLESS,
  GL_PROGRAM_POINT_SIZE,
};

void StateSystem::EnableState::applyGL(GLbitfield changedBits) const
{
  for (GLuint i = 0; i < NUM_STATEBITS; i++){
    if (isBitSet(changedBits,i)){
      if (isBitSet(stateBits,i))  glEnable  (s_stateEnums[i]);
      else                        glDisable (s_stateEnums[i]);
    }
  }
}

void StateSystem::EnableState::getGL()
{
  for (GLuint i = 0; i < NUM_STATEBITS; i++){
    setBitState(stateBits,i, glIsEnabled(s_stateEnums[i]));
  }
}

//////////////////////////////////////////////////////////////////////////

static GLenum s_stateEnumsDepr[StateSystem::NUM_STATEBITSDEPR] = {
  GL_ALPHA_TEST,
  GL_LINE_STIPPLE,
  GL_POINT_SMOOTH,
  GL_POINT_SPRITE,
  GL_POLYGON_STIPPLE,
};

void StateSystem::EnableStateDepr::applyGL(GLbitfield changedBits) const
{
  for (GLuint i = 0; i < NUM_STATEBITSDEPR; i++){
    if (isBitSet(changedBits,i)){
      if (isBitSet(stateBitsDepr,i))  glEnable  (s_stateEnumsDepr[i]);
      else                            glDisable (s_stateEnumsDepr[i]);
    }
  }
}

void StateSystem::EnableStateDepr::getGL()
{
  for (GLuint i = 0; i < NUM_STATEBITSDEPR; i++){
    setBitState(stateBitsDepr,i, glIsEnabled(s_stateEnumsDepr[i]));
  }
}


//////////////////////////////////////////////////////////////////////////

void StateSystem::State::applyGL(bool coreonly, bool skipFboBinding) const
{
  enable.applyGL();
  if (!coreonly) enableDepr.applyGL();
  program.applyGL();
  clip.applyGL();
  if (!coreonly) alpha.applyGL();
  blend.applyGL();
  depth.applyGL();
  stencil.applyGL();
  logic.applyGL();
  primitive.applyGL();
  sample.applyGL();
  raster.applyGL();
  if (!coreonly) rasterDepr.applyGL();
  /*if (!isBitSet(dynamicState,DYNAMIC_VIEWPORT)){
    viewport.applyGL();
  }*/
  depthrange.applyGL();
  /*if (!isBitSet(dynamicState,DYNAMIC_SCISSOR)){
    scissor.applyGL();
  }*/
  scissorenable.applyGL();
  mask.applyGL();
  fbo.applyGL(skipFboBinding);
  vertexenable.applyGL();
  vertexformat.applyGL();
  verteximm.applyGL();
}

void StateSystem::State::getGL(bool coreonly)
{
  enable.getGL();
  if (!coreonly) enableDepr.applyGL();
  program.getGL();
  clip.getGL();
  if (!coreonly) alpha.applyGL();
  blend.getGL();
  depth.getGL();
  stencil.getGL();
  logic.getGL();
  primitive.getGL();
  sample.getGL();
  raster.getGL();
  if (!coreonly) rasterDepr.applyGL();
  //viewport.getGL();
  depthrange.getGL();
  //scissor.getGL();
  scissorenable.getGL();
  mask.getGL();
  fbo.getGL();
  vertexenable.getGL();
  vertexformat.getGL();
  verteximm.getGL();
}


//////////////////////////////////////////////////////////////////////////

void StateSystem::init(bool coreonly)
{
  m_coreonly = coreonly;
}

void StateSystem::deinit()
{
  m_states.resize(0);
  m_freeIDs.resize(0);
}

void StateSystem::generate( GLuint num, StateID* objects )
{

  GLuint i;
  for ( i = 0; i < num && !m_freeIDs.empty(); i++){
    objects[i] = m_freeIDs.back();
    m_freeIDs.pop_back();
  }

  GLuint begin = GLuint(m_states.size());

  if ( i < num){
    m_states.resize( begin + num - i);
  }

  for ( i = i; i < num; i++){
    objects[i] = begin + i;
  }
}

void StateSystem::destroy( GLuint num, const StateID* objects )
{
  for (GLuint i = 0; i < num; i++){
    m_freeIDs.push_back(objects[i]);
  }
}

void StateSystem::set( StateID id, const State& state, GLenum basePrimitiveMode )
{
  StateInternal& intstate   = m_states[id];
  intstate.incarnation++;
  intstate.state = state;
  intstate.state.basePrimitiveMode = basePrimitiveMode;

  intstate.usedDiff = 0;
  for (int i = 0; i < MAX_DIFFS; i++){
    intstate.others[i].state = INVALID_ID;
  }
}

const StateSystem::State& StateSystem::get( StateID id ) const
{
  return m_states[id].state;
}

__forceinline int StateSystem::prepareTransitionCache(StateID prev, StateInternal& to )
{
  StateInternal& from = m_states[prev];

  int index = -1;

  for (int i = 0; i < MAX_DIFFS; i++){
    if ( to.others[i].state == prev && to.others[i].incarnation == from.incarnation) {
      index = i;
      break;
    }
  }

  if (index < 0){
    index = to.usedDiff;
    to.usedDiff = (to.usedDiff + 1) % MAX_DIFFS;

    to.others[index].state = prev;
    to.others[index].incarnation = from.incarnation;

    makeDiff(to.diffs[index], from, to);
  }

  return index;
}

void StateSystem::applyGL( StateID id, bool skipFboBinding ) const
{
  m_states[id].state.applyGL( m_coreonly, skipFboBinding );
}

void StateSystem::applyGL( StateID id, StateID prev, bool skipFboBinding )
{
  StateInternal& to   = m_states[id];

  if (prev == INVALID_ID){
    applyGL(id, skipFboBinding);
    return;
  }

  int index = prepareTransitionCache(prev, to);
  applyDiffGL( to.diffs[index], to.state, skipFboBinding );

}

void StateSystem::applyDiffGL( const StateDiff& diff, const State &state, bool skipFboBinding )
{
  if (isBitSet(diff.changedContentBits,StateDiff::ENABLE))
    state.enable.applyGL(diff.changedStateBits);
  if (!m_coreonly && isBitSet(diff.changedContentBits,StateDiff::ENABLE_DEPR))
    state.enableDepr.applyGL(diff.changedStateDeprBits);
  if (isBitSet(diff.changedContentBits,StateDiff::PROGRAM))
    state.program.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::CLIP))
    state.clip.applyGL();
  if (!m_coreonly && isBitSet(diff.changedContentBits,StateDiff::ALPHA_DEPR))
    state.alpha.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::BLEND))
    state.blend.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::DEPTH))
    state.depth.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::STENCIL))
    state.stencil.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::LOGIC))
    state.logic.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::PRIMITIVE))
    state.primitive.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::RASTER))
    state.raster.applyGL();
  if (!m_coreonly && isBitSet(diff.changedContentBits,StateDiff::RASTER_DEPR))
    state.rasterDepr.applyGL();
  /*if (isBitSet(diff.changedContentBits,StateDiff::VIEWPORT))
    state.viewport.applyGL();*/
  if (isBitSet(diff.changedContentBits,StateDiff::DEPTHRANGE))
    state.depthrange.applyGL();
  /*if (isBitSet(diff.changedContentBits,StateDiff::SCISSOR))
    state.scissor.applyGL();*/
  if (isBitSet(diff.changedContentBits,StateDiff::SCISSORENABLE))
    state.scissorenable.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::MASK))
    state.mask.applyGL();
  if (isBitSet(diff.changedContentBits,StateDiff::FBO))
    state.fbo.applyGL(skipFboBinding);
  if (isBitSet(diff.changedContentBits,StateDiff::VERTEXENABLE))
    state.vertexenable.applyGL(diff.changedVertexEnable);
  if (isBitSet(diff.changedContentBits,StateDiff::VERTEXFORMAT))
    state.vertexformat.applyGL(diff.changedVertexFormat, diff.changedVertexBinding);
  if (isBitSet(diff.changedContentBits,StateDiff::VERTEXIMMEDIATE))
    state.verteximm.applyGL(diff.changedVertexImm);
}


void StateSystem::makeDiff( StateDiff& diff, const StateInternal &fromInternal, const StateInternal &toInternal )
{
  const State &from = fromInternal.state;
  const State &to   = toInternal.state;

  diff.changedStateBits     = from.enable.stateBits ^ to.enable.stateBits;
  diff.changedStateDeprBits = from.enableDepr.stateBitsDepr ^ to.enableDepr.stateBitsDepr;
  diff.changedContentBits   = 0;
  
  if (memcmp(&from.enable         ,&to.enable         ,sizeof(from.enable         )) != 0) setBit(diff.changedContentBits,StateDiff::ENABLE);
  if (memcmp(&from.enableDepr     ,&to.enableDepr     ,sizeof(from.enableDepr     )) != 0) setBit(diff.changedContentBits,StateDiff::ENABLE_DEPR);
  if (memcmp(&from.program        ,&to.program        ,sizeof(from.program        )) != 0) setBit(diff.changedContentBits,StateDiff::PROGRAM);
  if (memcmp(&from.clip           ,&to.clip           ,sizeof(from.clip           )) != 0) setBit(diff.changedContentBits,StateDiff::CLIP);
  if (memcmp(&from.alpha          ,&to.alpha          ,sizeof(from.alpha          )) != 0) setBit(diff.changedContentBits,StateDiff::ALPHA_DEPR);
  if (memcmp(&from.blend          ,&to.blend          ,sizeof(from.blend          )) != 0) setBit(diff.changedContentBits,StateDiff::BLEND);
  if (memcmp(&from.depth          ,&to.depth          ,sizeof(from.depth          )) != 0) setBit(diff.changedContentBits,StateDiff::DEPTH);
  if (memcmp(&from.stencil        ,&to.stencil        ,sizeof(from.stencil        )) != 0) setBit(diff.changedContentBits,StateDiff::STENCIL);
  if (memcmp(&from.logic          ,&to.logic          ,sizeof(from.logic          )) != 0) setBit(diff.changedContentBits,StateDiff::LOGIC);
  if (memcmp(&from.primitive      ,&to.primitive      ,sizeof(from.primitive      )) != 0) setBit(diff.changedContentBits,StateDiff::PRIMITIVE);
  if (memcmp(&from.raster         ,&to.raster         ,sizeof(from.raster         )) != 0) setBit(diff.changedContentBits,StateDiff::RASTER);
  if (memcmp(&from.rasterDepr     ,&to.rasterDepr     ,sizeof(from.rasterDepr     )) != 0) setBit(diff.changedContentBits,StateDiff::RASTER_DEPR);
  //if (memcmp(&from.viewport       ,&to.viewport       ,sizeof(from.viewport       )) != 0) setBit(diff.changedContentBits,StateDiff::VIEWPORT);
  if (memcmp(&from.depth          ,&to.depth          ,sizeof(from.depth          )) != 0) setBit(diff.changedContentBits,StateDiff::DEPTHRANGE);
  //if (memcmp(&from.scissor        ,&to.scissor        ,sizeof(from.scissor        )) != 0) setBit(diff.changedContentBits,StateDiff::SCISSOR);
  if (memcmp(&from.scissorenable  ,&to.scissorenable  ,sizeof(from.scissorenable  )) != 0) setBit(diff.changedContentBits,StateDiff::SCISSORENABLE);
  if (memcmp(&from.mask           ,&to.mask           ,sizeof(from.mask           )) != 0) setBit(diff.changedContentBits,StateDiff::MASK);
  if (memcmp(&from.fbo            ,&to.fbo            ,sizeof(from.fbo            )) != 0) setBit(diff.changedContentBits,StateDiff::FBO);

  // special case vertex stuff, more likely to change then rest

  diff.changedVertexEnable  = from.vertexenable.enabled ^ to.vertexenable.enabled;

  diff.changedVertexImm = 0;
  diff.changedVertexFormat = 0;
  
  for (GLint i = 0; i < MAX_VERTEXATTRIBS; i++){
    if (memcmp(&from.vertexformat.formats[i], &to.vertexformat.formats[i], sizeof(to.vertexformat.formats[i])) != 0)  setBit(diff.changedVertexFormat,i);
    if (memcmp(&from.verteximm.data[i], &to.verteximm.data[i], sizeof(to.verteximm.data[i])) != 0)                    setBit(diff.changedVertexImm,i);
  }

  diff.changedVertexBinding = 0;
  for (GLint i = 0; i < MAX_VERTEXBINDINGS; i++){
    if (memcmp(&from.vertexformat.bindings[i], &to.vertexformat.bindings[i], sizeof(to.vertexformat.bindings[i])) != 0)  setBit(diff.changedVertexBinding,i);
  }

  if (diff.changedVertexEnable)                               setBit(diff.changedContentBits,StateDiff::VERTEXENABLE);
  if (diff.changedVertexBinding || diff.changedVertexFormat)  setBit(diff.changedContentBits,StateDiff::VERTEXFORMAT);
  if (diff.changedVertexImm)                                  setBit(diff.changedContentBits,StateDiff::VERTEXIMMEDIATE);
}

void StateSystem::prepareTransition( StateID id, StateID prev )
{
  StateInternal& to   = m_states[id];

  prepareTransitionCache(prev,to);
}


