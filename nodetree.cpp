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

#include "nodetree.hpp"
#include <assert.h>

//////////////////////////////////////////////////////////////////////////


static inline void clearNode(NodeTree::Node &node)
{
  node.level      = -1;
  node.leafidx    = NodeTree::INVALID;
  node.levelidx   = NodeTree::INVALID;
  node.parentidx  = NodeTree::INVALID;
  node.childidx   = NodeTree::INVALID;
  node.siblingidx = NodeTree::INVALID;
}

NodeTree::NodeTree()
{
  m_levelsUsed = 0;
  m_treeCompactChangeID = 0;
  m_nodesActive = 0;

  clearNode(m_root);
  m_root.levelidx =  0;
  m_root.level    = -1;
}

const NodeTree::Level* NodeTree::getUsedLevel( int level ) const
{
  if (0 <= level && level < m_levelsUsed){
    return &m_levels[level];
  }
  return nullptr;
}

unsigned int NodeTree::getTreeParentChangeID() const
{
  return m_treeCompactChangeID;
}

const std::vector<NodeTree::compactID>& NodeTree::getTreeCompactNodes() const
{
  return m_treeCompactNodes;
}

NodeTree::nodeID NodeTree::createNode()
{
  nodeID id;

  if (!m_unusedNodes.empty()){
    id = m_unusedNodes[m_unusedNodes.size()-1];
    m_unusedNodes.pop_back();
  }
  else{
    Node node;
    m_nodes.push_back(node);
    m_treeCompactNodes.push_back(compactID());
    id = (nodeID)(m_nodes.size()-1);
  }

  Node&  node = getNode(id);
  clearNode(node);

  return id;
}

void NodeTree::deleteNode( nodeID nodeidx )
{
  assert (isValid(nodeidx) && nodeidx != ROOT);

  const Node &node = getNode(nodeidx);

  // make children unlinked
  while (isValid(node.childidx)){
    setNodeParent(node.childidx,INVALID);
  }

  // remove self from parent list
  setNodeParent(nodeidx,INVALID);

  m_unusedNodes.push_back(nodeidx);
}

void NodeTree::setNodeParent( nodeID nodeidx, nodeID parentidx )
{
  assert (isValid(nodeidx) && nodeidx != ROOT);

  Node &node = getNode(nodeidx);
  if (node.parentidx == parentidx)
    return;

  if (isValid(node.parentidx)){
    // unlink from old
    Node& parent = getNode(node.parentidx);
    bool found = false;
    
    if (parent.childidx == nodeidx){
      parent.childidx = node.siblingidx;
      found = true;
    }
    else if (isValid(parent.childidx)){
      nodeID child = parent.childidx;
      while(isValid(getNode(child).siblingidx)){
        if (getNode(child).siblingidx == nodeidx){
          getNode(child).siblingidx = node.siblingidx;
          found = true;
          break;
        }
        child = getNode(child).siblingidx;
      }
    }

    assert(found && "node was not a child of parent");
    node.siblingidx = INVALID;
    updateLeafNode(node.parentidx);
  }

  if (isValid(parentidx)){
    // link to new
    Node& parent = getNode(parentidx);
    node.siblingidx = parent.childidx;
    parent.childidx = nodeidx;
    updateLeafNode(node.parentidx);
  }

  if (isNodeInTree(nodeidx)){
    updateLevelNode(nodeidx, isNodeInTree(parentidx) ? parentidx : INVALID);
  }

  node.parentidx = parentidx;
}

void NodeTree::addToTree( nodeID nodeidx )
{
  assert (isValid(nodeidx) && nodeidx != ROOT);

  const Node &node = getNode(nodeidx);
  assert (!isNodeInTree(nodeidx)        && "must not be already added to tree");
  assert ( isNodeInTree(node.parentidx) && "parent must be already added to tree");

  updateLevelNode(nodeidx,node.parentidx);
}

void NodeTree::removeFromTree( nodeID nodeidx )
{
  assert (isValid(nodeidx) && nodeidx != ROOT);
  const Node &node = getNode(nodeidx);
  assert (isNodeInTree(nodeidx) && "must be already added to tree");

  updateLevelNode(nodeidx,INVALID);
}

void NodeTree::addToLevel( nodeID nodeidx, nodeID parentidx )
{
  Node&   node        = getNode(nodeidx);
  const Node& parent  = getNode(parentidx);
  Level&  level       = getLevel(parent.level+1);

  level.changeID++;

  node.levelidx = (lvlID)level.nodes.size();
  node.level    = parent.level+1;
  level.nodes.push_back(nodeidx);

  if (!isValid(node.childidx)){
    addLeafNode(nodeidx);
  }

  m_levelsUsed = node.level+1 > m_levelsUsed ? node.level+1 : m_levelsUsed;

  m_nodesActive++;
}

void NodeTree::removeFromLevel( nodeID nodeidx )
{
  Node&   node  = getNode(nodeidx);
  Level&  level = getLevel(node.level);

  level.changeID++;

  level.nodes[node.levelidx] = level.nodes[level.nodes.size()-1];
  getNode(level.nodes[node.levelidx]).levelidx = node.levelidx;
  level.nodes.pop_back();

  if (isValid(node.leafidx)){
    removeLeafNode(nodeidx);
  }

  if (node.level+1 == m_levelsUsed && level.nodes.empty()){
    m_levelsUsed--;
  }

  node.level    = -1;
  node.levelidx = INVALID;
  node.leafidx  = INVALID;

  m_nodesActive--;
}

void NodeTree::removeLeafNode( nodeID nodeidx )
{
  assert(isNodeInTree(nodeidx));
  Node& node    = getNode(nodeidx);
  Level& level  = getLevel(node.level);
  // remove
  level.leaves[node.leafidx] = level.leaves[level.leaves.size()-1];
  getNode(level.leaves[node.leafidx]).leafidx = node.leafidx;
  level.leaves.pop_back();
}

void NodeTree::addLeafNode( nodeID nodeidx )
{
  assert(isNodeInTree(nodeidx));
  Node& node    = getNode(nodeidx);
  Level& level  = getLevel(node.level);
  // add
  node.leafidx = (lvlID)level.leaves.size();
  level.leaves.push_back(nodeidx);
}

void NodeTree::updateLeafNode( nodeID nodeidx )
{
  if (!isNodeInTree(nodeidx))
    return;

  Node& node    = getNode(nodeidx);
  if (!isValid(node.childidx) && isValid(node.leafidx)){
    removeLeafNode(nodeidx);
  }
  else if (isValid(node.childidx) && !isValid(node.leafidx)){
    addLeafNode(nodeidx);
  }
}

void NodeTree::updateLevelNode( nodeID nodeidx, nodeID parentidx )
{
  // at this point node.parentidx is still the old value
  Node &node = getNode(nodeidx);

  // update level parent buffer to reflect last state always
  m_treeCompactNodes[nodeidx].parent = parentidx;
  m_treeCompactChangeID++;

  if (isValid(node.levelidx)){
    // already active
    if (isValid(parentidx)){
      const Node& parent = getNode(parentidx);
      int oldlevel = node.level;
      int newlevel = parent.level + 1;

      // we remain in the same level and only our parent has changed
      if (oldlevel == newlevel){
        return;
      }

      removeFromLevel(nodeidx);
      addToLevel(nodeidx,parentidx);
    }
    else{
      removeFromLevel(nodeidx);
    }
  }
  else if (isValid(parentidx)){
    // was inactive 
    // add to level
    addToLevel(nodeidx,parentidx);
  }

  m_treeCompactNodes[nodeidx].level  = node.level;

  nodeID child = node.childidx;
  while (isValid(child)){
    updateLevelNode(child, isValid(parentidx) ? nodeidx : INVALID );
    child = getNode(child).siblingidx;
  }
}

void NodeTree::reserve( int numNodes )
{
  m_nodes.reserve( numNodes );
  m_treeCompactNodes.reserve( numNodes );
}

void NodeTree::create( int numNodes )
{
  Node node;
  clearNode(node);

  m_nodes.resize( numNodes, node );
  m_treeCompactNodes.resize( numNodes, compactID() );
}

void NodeTree::clear()
{
  m_nodesActive = 0;
  m_levelsUsed  = 0;
  m_treeCompactChangeID = 0;
  m_levels.clear();
  m_nodes.clear();
  m_treeCompactNodes.clear();
}

