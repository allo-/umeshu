//
//  Copyright (c) 2011-2013 Vladimir Chalupecky
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to
//  deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//  sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
//  IN THE SOFTWARE.

#ifndef UMESHU_TRIANGULATION_H
#define UMESHU_TRIANGULATION_H

#include "HDS/HDS.h"
#include "io/Postscript_ostream.h"
#include "Bounding_box.h"
#include "Exact_adaptive_kernel.h"
#include "Exceptions.h"
#include "Orientation.h"

#include <boost/assert.hpp>
#include <boost/pool/pool_alloc.hpp>

namespace umeshu
{

enum Point_location {IN_FACE, ON_EDGE, ON_NODE, OUTSIDE_MESH};

template <typename Triangulation_items, typename Kernel_ = Exact_adaptive_kernel, typename Alloc = boost::fast_pool_allocator<int> >
class Triangulation : public hds::HDS<Triangulation_items, Kernel_, Alloc>
{

  typedef hds::HDS<Triangulation_items, Kernel_, Alloc> Base;

public:

  typedef Triangulation_items Items;
  typedef Kernel_ Kernel;

  typedef typename Base::Node     Node;
  typedef typename Base::Halfedge Halfedge;
  typedef typename Base::Edge     Edge;
  typedef typename Base::Face     Face;

  typedef typename Base::Node_iterator       Node_iterator;
  typedef typename Base::Edge_iterator       Edge_iterator;
  typedef typename Base::Face_iterator       Face_iterator;

  typedef typename Base::Node_const_iterator Node_const_iterator;
  typedef typename Base::Edge_const_iterator Edge_const_iterator;
  typedef typename Base::Face_const_iterator Face_const_iterator;

  typedef typename Base::Node_handle         Node_handle;
  typedef typename Base::Halfedge_handle     Halfedge_handle;
  typedef typename Base::Edge_handle         Edge_handle;
  typedef typename Base::Face_handle         Face_handle;

  Node_handle add_node( Point2 const& p )
  {
    Node_handle n = this->get_new_node();
    n->set_position( p );
    return n;
  }

  void remove_node( Node_handle n )
  {
    Halfedge_handle next = n->halfedge();

    while ( !n->is_isolated() )
    {
      Halfedge_handle cur = next;
      next = cur->pair()->next();
      remove_edge( cur->edge() );
    }

    this->delete_node( n );
  }

  Halfedge_handle add_edge( Node_handle n1, Node_handle n2 )
  {
    BOOST_ASSERT( n1 != n2 );

    Edge_handle e = this->get_new_edge();

    Halfedge_handle he1 = e->he1();
    Halfedge_handle he2 = e->he2();

    try
    {
      attach_halfedge_to_node( he1, n1 );
      attach_halfedge_to_node( he2, n2 );
    }
    catch ( umeshu_error& exc )
    {
      exc << errinfo_desc("Trying to attach an edge to a complete mesh");
      throw;
    }

    return he1;
  }

  void remove_edge( Edge_handle e )
  {
    if ( !e->he1()->is_boundary() )
    {
      remove_face( e->he1()->face() );
    }

    if ( !e->he2()->is_boundary() )
    {
      remove_face( e->he2()->face() );
    }

    detach_edge( e->he1() );
    detach_edge( e->he2() );

    this->delete_edge( e );
  }


  Face_handle add_face( Halfedge_handle he1, Halfedge_handle he2, Halfedge_handle he3 )
  {
    if (!( he1->is_boundary() &&
              he2->is_boundary() &&
              he3->is_boundary() ) )
    {
      BOOST_THROW_EXCEPTION( add_face_error() << errinfo_desc("half-edges are not free, cannot add face") );
    }

    if (!( he1->pair()->origin() == he2->origin() &&
              he2->pair()->origin() == he3->origin() &&
              he3->pair()->origin() == he1->origin() ) )
    {
      BOOST_THROW_EXCEPTION( add_face_error() << errinfo_desc("half-edges do not form a chain, cannot add face") );
    }

    if (!( make_adjacent( he1, he2 ) &&
              make_adjacent( he2, he3 ) &&
              make_adjacent( he3, he1 ) ) )
    {
      BOOST_THROW_EXCEPTION( add_face_error() << errinfo_desc("add_face: attempting to create non-manifold mesh, cannot add face\n") );
    }

    Face_handle f = this->get_new_face();

    f->set_halfedge( he1 );

    he1->set_face( f );
    he2->set_face( f );
    he3->set_face( f );

    return f;
  }

  void remove_face( Face_handle f )
  {
    f->halfedge()->set_face( Face_handle() );
    f->halfedge()->next()->set_face( Face_handle() );
    f->halfedge()->prev()->set_face( Face_handle() );
    this->delete_face( f );
  }

  Node_handle split_edge( Edge_handle e, Point2 const& p )
  {
    Halfedge_handle h1, h2, h3, h4, h5, h6, h7, h8;
    Node_handle n1, n2, n3, n4;
    h1 = e->he1();
    h2 = e->he2();
    n1 = h1->origin();
    n2 = h2->origin();
    bool is_there_f1 = !h1->is_boundary();
    bool is_there_f2 = !h2->is_boundary();

    if ( is_there_f1 )
    {
      h5 = h1->next();
      h6 = h1->prev();
      n3 = h6->origin();
    }

    if ( is_there_f2 )
    {
      h7 = h2->next();
      h8 = h2->prev();
      n4 = h8->origin();
    }

    remove_edge( e );
    Node_handle n_new = add_node( p );
    h1 = add_edge( n_new, n1 );
    h2 = add_edge( n_new, n2 );

    if ( is_there_f1 )
    {
      h3 = add_edge( n_new, n3 );
      add_face( h2, h5, h3->pair() );
      add_face( h3, h6, h1->pair() );
    }

    if ( is_there_f2 )
    {
      h4 = add_edge( n_new, n4 );
      add_face( h1, h7, h4->pair() );
      add_face( h4, h8, h2->pair() );
    }

    return n_new;
  }

  Node_handle split_face( Face_handle f, Point2 const& p )
  {
    Halfedge_handle h1 = f->halfedge();
    Halfedge_handle h2 = h1->next();
    Halfedge_handle h3 = h1->prev();
    remove_face( f );
    Node_handle n_new = add_node( p );
    Halfedge_handle h4 = add_edge( n_new, h1->origin() );
    Halfedge_handle h5 = add_edge( n_new, h2->origin() );
    Halfedge_handle h6 = add_edge( n_new, h3->origin() );
    add_face( h4, h1, h5->pair() );
    add_face( h5, h2, h6->pair() );
    add_face( h6, h3, h4->pair() );
    return n_new;
  }

  Bounding_box bounding_box() const
  {
    Bounding_box bbox = boost::geometry::make_inverse<Bounding_box>();

    for ( Node_const_iterator iter = this->nodes_begin(); iter != this->nodes_end(); ++iter )
    {
      boost::geometry::expand( bbox, iter->position() );
    }

    return bbox;
  }

  Halfedge_handle boundary_halfedge()
  {
    for ( Edge_iterator iter = this->edges_begin(); iter != this->edges_end(); ++iter )
    {
      if ( iter->he1()->is_boundary() )
      {
        return iter->he1();
      }

      if ( iter->he2()->is_boundary() )
      {
        return iter->he2();
      }
    }

    return Halfedge_handle();
  }

  Face_handle locate( Point2 const& p, Point_location& loc, Node_handle& on_node, Edge_handle& on_edge, Face_handle start_face = Face_handle() )
  {
    Halfedge_handle he_start;

    if ( start_face == Face_handle() )
    {
      he_start = this->faces_begin()->halfedge();
    }
    else
    {
      he_start = start_face->halfedge();
    }

    Halfedge_handle he_iter = he_start;

    while ( true )
    {
      Point2 p1 = he_iter->origin()->position();
      Point2 p2 = he_iter->pair()->origin()->position();
      Oriented_side os = Kernel::oriented_side( p1, p2, p );

      switch ( os )
      {
      case ON_POSITIVE_SIDE:
        he_iter = he_iter->next();

        if ( he_iter == he_start )
        {
          loc = IN_FACE;
          return he_iter->face();
        }

        break;

      case ON_ORIENTED_BOUNDARY:
      {
        if ( ( std::min( p1.x(), p2.x() ) < p.x() && p.x() < std::max( p1.x(), p2.x() ) ) ||
             ( std::min( p1.y(), p2.y() ) < p.y() && p.y() < std::max( p1.y(), p2.y() ) ) )
        {
          loc = ON_EDGE;
          on_edge = he_iter->edge();
          return Face_handle();
        }
        else if ( p == p1 )
        {
          loc = ON_NODE;
          on_node = he_iter->origin();
          return Face_handle();
        }
        else if ( p == p2 )
        {
          loc = ON_NODE;
          on_node = he_iter->pair()->origin();
          return Face_handle();
        }
      }

      case ON_NEGATIVE_SIDE:
        if ( he_iter->pair()->is_boundary() )
        {
          loc = OUTSIDE_MESH;
          on_edge = he_iter->edge();
          return Face_handle();
        }

        he_iter = he_iter->pair();
        he_start = he_iter;
        he_iter = he_iter->next();
      }
    }
  }

private:

  void attach_halfedge_to_node( Halfedge_handle he, Node_handle n )
  {
    he->set_origin( n );

    if ( n->is_isolated() )
    {
      n->set_halfedge( he );
      he->set_prev( he->pair() );
      he->pair()->set_next( he );
    }
    else
    {
      Halfedge_handle free_in_he = find_free_incident_halfedge( n ); 
      Halfedge_handle free_out_he = free_in_he->next();

      free_in_he->set_next( he );
      he->set_prev( free_in_he );
      he->pair()->set_next( free_out_he );
      free_out_he->set_prev( he->pair() );
    }
  }

  bool make_adjacent( Halfedge_handle in, Halfedge_handle out )
  {
    // halfedges are already adjacent
    if ( in->next() == out )
    {
      return true;
    }

    Halfedge_handle b = in->next();
    Halfedge_handle d = out->prev();

    Node_handle n = out->origin();
    Halfedge_handle g = find_free_incident_halfedge( out->pair(), in );

    Halfedge_handle h = g->next();
    in->set_next( out );
    out->set_prev( in );
    g->set_next( b );
    b->set_prev( g );
    d->set_next( h );
    h->set_prev( d );

    return true;
  }

  Halfedge_handle find_free_incident_halfedge( Node_handle n )
  {
    BOOST_ASSERT( !n->is_isolated() );

    Halfedge_handle he_start = n->halfedge()->pair();
    Halfedge_handle he_iter = he_start;

    do
    {
      if ( he_iter->is_boundary() )
      {
        return he_iter;
      }

      he_iter = he_iter->next()->pair();
    }
    while ( he_iter != he_start );

    BOOST_THROW_EXCEPTION( bad_topology_error() );
    return Halfedge_handle();
  }

  Halfedge_handle find_free_incident_halfedge( Halfedge_handle he1, Halfedge_handle he2 )
  {
    BOOST_ASSERT( he1->pair()->origin() == he2->pair()->origin() );

    do
    {
      if ( he1->is_boundary() )
      {
        return he1;
      }

      he1 = he1->next()->pair();
    }
    while ( he1 != he2 );

    BOOST_THROW_EXCEPTION( bad_topology_error() );
    return Halfedge_handle();
  }


  void detach_edge( Halfedge_handle he )
  {
    Node_handle n = he->origin();

    if ( n->halfedge() == he )
    {
      if ( he->pair()->next() != he )
      {
        n->set_halfedge( he->pair()->next() );
      }
      else
      {
        n->set_halfedge( Halfedge_handle() );
      }
    }

    he->prev()->set_next( he->pair()->next() );
    he->pair()->next()->set_prev( he->prev() );
  }
};


} // namespace umeshu

#endif // UMESHU_TRIANGULATION_H
