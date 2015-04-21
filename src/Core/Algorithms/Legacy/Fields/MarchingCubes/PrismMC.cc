/*
   For more information, please see: http://software.sci.utah.edu

   The MIT License

   Copyright (c) 2015 Scientific Computing and Imaging Institute,
   University of Utah.

   
   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
*/

//    File   : PrismMC.cc
//    Author : Allen Sanderson
//    Date   : Thu July 24 21:33:04 2003

#include <Core/Algorithms/Legacy/Fields/MarchingCubes/PrismMC.h>
#include <Core/Algorithms/Legacy/Fields/MarchingCubes/mcube2.h>

#include <Core/Datatypes/Legacy/Field/FieldInformation.h>
//#include <sci_hash_map.h>

void PrismMC::reset( int /*n*/, bool build_field, bool build_geom, bool transparency )
{
  build_field_ = build_field;
  build_geom_ = build_geom;
  basis_order_ = field_->basis_order();

  edge_map_.clear();
  VMesh::Node::size_type nsize;
  mesh_->size(nsize);
  nnodes_ = nsize;

  cell_map_.clear();
  VMesh::Elem::size_type csize;
  mesh_->size(csize);
  ncells_ = csize;
 #ifdef SCIRUN4_CODE_TO_BE_ENABLED_LATER
  if (basis_order_ == 0)
  {
    mesh_->synchronize(Mesh::FACES_E|Mesh::ELEM_NEIGHBORS_E);
    if (build_field_)
    {
      node_map_ = std::vector<index_type>(nnodes_, -1);
    }
  }
  triangles_ = 0;
  if (build_geom_)
  {
    if( transparency )
      triangles_ = new GeomTranspTriangles;
    else
      triangles_ = new GeomFastTriangles;
  }
  geomHandle_ = triangles_;
 #endif
 
  trisurf_ = 0;
  if (build_field_)
  {
    FieldInformation fi("TriSurfMesh",basis_order_,"double");
    trisurf_handle_ = CreateField(fi);
    trisurf_ = trisurf_handle_->vmesh(); 
  }
}

void PrismMC::extract( VMesh::Elem::index_type cell, double v )
{
  if (basis_order_ == 0)
    extract_c(cell, v);
  else
    extract_n(cell, v);
}

void PrismMC::find_or_add_parent(index_type u0, index_type u1, double d0, index_type face) 
{
  if (d0 < 0.0) { u1 = -1; }
  if (d0 > 1.0) { u0 = -1; }
  edgepair_t np;
  
  if (u0 < u1)  { np.first = u0; np.second = u1; np.dfirst = d0; }
  else { np.first = u1; np.second = u0; np.dfirst = 1.0 - d0; }
  const edge_hash_type::iterator loc = edge_map_.find(np);
  
  if (loc == edge_map_.end())
  {
    edge_map_[np] = face;
  }
  else
  {
    // This should never happen but does for prisms. This is because
    // the quads are represented as two triangles so the cell is used
    // twice.
  }
}
VMesh::Node::index_type PrismMC::find_or_add_nodepoint(VMesh::Node::index_type &tet_node_idx) 
{
  VMesh::Node::index_type surf_node_idx;
  index_type i = node_map_[tet_node_idx];
  if (i != -1) surf_node_idx = VMesh::Node::index_type(i);
  else 
  {
    Point p;
    mesh_->get_point(p, tet_node_idx);
    surf_node_idx = trisurf_->add_point(p);
    node_map_[tet_node_idx] = surf_node_idx;
  }
  return (surf_node_idx);
}

void PrismMC::extract_c( VMesh::Elem::index_type cell, double iso )
{
  double selfvalue, nbrvalue;
  field_->get_value( selfvalue, cell );
  VMesh::DElem::array_type faces;
  mesh_->get_delems(faces, cell);

  VMesh::Elem::index_type nbr_cell;
  Point p[4];
  VMesh::Node::array_type face_nodes;
  VMesh::Node::array_type vertices(4);
  VMesh::Node::array_type nodes(3);

  for (size_t i=0; i<faces.size(); i++)
  {
    if (mesh_->get_neighbor(nbr_cell, cell, faces[i]) &&
        field_->value(nbrvalue, nbr_cell) &&
        selfvalue <= iso && iso < nbrvalue)
    {
      mesh_->get_nodes(face_nodes, faces[i]);
      mesh_->get_centers(p,face_nodes);
     #ifdef SCIRUN4_CODE_TO_BE_ENABLED_LATER
      if (build_geom_)
      {
        triangles_->add(p[0], p[1], p[2]);
            
        if( face_nodes.size() == 4 )
          triangles_->add(p[0], p[2], p[3]);
      }
     #endif
      if (build_field_)
      {
        for (size_t j=0; j<face_nodes.size(); j++)
        {
          vertices[j] = find_or_add_nodepoint(face_nodes[j]);
        }

        nodes[0] = vertices[0]; 
        nodes[1] = vertices[1]; 
        nodes[2] = vertices[2]; 

        VMesh::Elem::index_type tface = trisurf_->add_elem(nodes);
        
        const double d = (selfvalue - iso) / (selfvalue - nbrvalue);
        find_or_add_parent(cell, nbr_cell, d, tface);

        if( face_nodes.size() == 4 )
        {
          nodes[1] = vertices[2]; 
          nodes[2] = vertices[3]; 
          tface = trisurf_->add_elem(nodes);
          const double d = (selfvalue - iso) / (selfvalue - nbrvalue);

          find_or_add_parent(cell, nbr_cell, d, tface);
        }
      }
    }
  }
}

void PrismMC::extract_n( VMesh::Elem::index_type cell, double iso )
{
  VMesh::Node::array_type node(6);
  Point p[8];
  double value[8];
  int code = 0;

  mesh_->get_nodes( node, cell );

  // Fake 8 nodes and use the hex algorithm. This will result in
  // degenerate triangles but they are removed below.
  unsigned int lu[8];

  lu[0] = 0;
  lu[1] = 1;
  lu[2] = 2;
  lu[3] = 0;
  lu[4] = 3;
  lu[5] = 4;
  lu[6] = 5;
  lu[7] = 3;

  for (int i=7; i>=0; i--)
  {
    mesh_->get_point( p[i], node[lu[i]] );
    field_->value( value[i], node[lu[i]] );
    code = code*2+(value[i] < iso );
  }

  if ( code == 0 || code == 255 ) return;

  TRIANGLE_CASES *tcase=&triCases[code];
  int *vertex = tcase->edges;
  
  Point q[12];
  VMesh::Node::index_type surf_node[12];

  // interpolate and project vertices
  int v = 0;
  std::vector<bool> visited(12, false);
  while (vertex[v] != -1) 
  {
    index_type i = vertex[v++];
    if (visited[i]) continue;
    visited[i]=true;
    const index_type v1 = edge_tab[i][0];
    const index_type v2 = edge_tab[i][1];
    const double d = (value[v1]-iso)/double(value[v1]-value[v2]);
    q[i] = Interpolate(p[v1], p[v2], d);

    if (build_field_)
    {
      surf_node[i] = find_or_add_edgepoint(node[lu[v1]], node[lu[v2]], d, q[i]);
    }
  }    
  
  v = 0;
  while(vertex[v] != -1) 
  {
    index_type v0 = vertex[v++];
    index_type v1 = vertex[v++];
    index_type v2 = vertex[v++];

    // Degenerate triangle can be built since points were duplicated
    // above in order to make a hexvol for MC. 
    if( v0 != v1 && v0 != v2 && v1 != v2 ) 
    {
     #ifdef SCIRUN4_CODE_TO_BE_ENABLED_LATER
      if (build_geom_)
      {
        triangles_->add(q[v0], q[v1], q[v2]);
      }
     #endif
      if (build_field_)
      {
        if (surf_node[v0] != surf_node[v1] &&
            surf_node[v1] != surf_node[v2] &&
            surf_node[v2] != surf_node[v0])
        {
          VMesh::Node::array_type nodes(3);
          nodes[0] = surf_node[v0];
          nodes[1] = surf_node[v1];
          nodes[2] = surf_node[v2];
          trisurf_->add_elem(nodes);
          cell_map_.push_back( cell );
        }
      }
    }
  }
}

FieldHandle PrismMC::get_field(double value)
{
  trisurf_handle_->vfield()->resize_values();
  trisurf_handle_->vfield()->set_all_values(value);

  return (trisurf_handle_);  
}

VMesh::Node::index_type PrismMC::find_or_add_edgepoint(index_type u0, 
                               index_type u1,
                               double d0, const Point &p) 
{
  if (d0 < 0.0) { u1 = -1; }
  if (d0 > 1.0) { u0 = -1; }
  edgepair_t np;
  
  if (u0 < u1)  { np.first = u0; np.second = u1; np.dfirst = d0; }
  else { np.first = u1; np.second = u0; np.dfirst = 1.0 - d0; }
  const edge_hash_type::iterator loc = edge_map_.find(np);
  
  if (loc == edge_map_.end())
  {
    const VMesh::Node::index_type nodeindex = trisurf_->add_point(p);
    edge_map_[np] = nodeindex;
    return (nodeindex);
  }
  else
  {
    return ((*loc).second);
  }
}

