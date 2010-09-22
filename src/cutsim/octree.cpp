/*  $Id$
 * 
 *  Copyright 2010 Anders Wallin (anders.e.e.wallin "at" gmail.com)
 *  
 *  This file is part of OpenCAMlib.
 *
 *  OpenCAMlib is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  OpenCAMlib is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with OpenCAMlib.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <list>
// uncomment to disable assert() calls
// #define NDEBUG
#include <cassert>
#include <boost/foreach.hpp>

#include "point.h"
#include "triangle.h"
#include "numeric.h"
#include "octree.h"
#include "octnode.h"
#include "volume.h"

namespace ocl
{

//**************** Octree ********************/

Octree::Octree(double scale, unsigned int  depth, Point& centerp) {
    root_scale = scale;
    max_depth = depth;
                    // parent, idx, scale, depth
    root = new Octnode( 0, 0, root_scale, 0 );
    root->center = &centerp;
    
    for (int m=0;m<6;++m) // the root node has all cube-surfaces
        root->surface[m]=true;
}
Octree::~Octree() {
    delete root;
    root = 0;
}
unsigned int Octree::get_max_depth() const {
    return max_depth;
}
double Octree::get_root_scale() const {
    return root_scale;
}
double Octree::leaf_scale() const {
    return (2.0*root_scale) / pow(2.0, (int)max_depth );
}
        
/// subdivide the Octree n-times
void Octree::init(const unsigned int n) {
    for (unsigned int m=0;m<n;++m) {
        std::vector<Octnode*> nodelist;
        Octree::get_leaf_nodes(root, nodelist);
        BOOST_FOREACH( Octnode* node, nodelist) {
            node->subdivide();
        }
    }
}

/// put leaf nodes into nodelist
void Octree::get_leaf_nodes(Octnode* current, std::vector<Octnode*>& nodelist) const {
    if ( current->childcount == 0 ) {
        nodelist.push_back( current );
    } else {
        for ( int n=0;n<8;++n) {
            if ( current->child[n] != 0 )
                get_leaf_nodes( current->child[n], nodelist );
        }
    }
}

/// put all nodes into nodelist
void Octree::get_all_nodes(Octnode* current, std::vector<Octnode*>& nodelist) const {
    if ( current ) {
        nodelist.push_back( current );
        for ( int n=0;n<8;++n) {
            if ( current->child[n] != 0 )
                get_all_nodes( current->child[n], nodelist );
        }
    }
}

/// run marching cubes on the whole octree, returning the surface triangles
std::vector<Triangle> Octree::mc() {
    std::vector<Octnode*> leaf_nodes;
    get_leaf_nodes(this->root, leaf_nodes);
    //std::cout << " mc() got " << leaf_nodes.size() << " leaf nodes\n";
    std::vector<Triangle> mc_triangles;
    BOOST_FOREACH(Octnode* n, leaf_nodes) {
        std::vector<Triangle> tris = n->mc_triangles();
        BOOST_FOREACH( Triangle t, tris) {
            mc_triangles.push_back(t);
        }
    }
    return mc_triangles;
}

/// generate side_triangles
std::vector<Triangle> Octree::side_triangles() {
    std::vector<Octnode*> leaf_nodes;
    get_leaf_nodes(this->root, leaf_nodes);
    //std::cout << " Octree::side_triangles() got " << leaf_nodes.size() << " leaf nodes\n";
    std::vector<Triangle> s_triangles;
    BOOST_FOREACH(Octnode* n, leaf_nodes) {
        std::vector<Triangle> tris = n->side_triangles();
        BOOST_FOREACH( Triangle t, tris) {
            s_triangles.push_back(t);
        }
    }
    return s_triangles;
}

void Octree::diff_negative_root(const OCTVolume* vol) {
    diff_negative( this->root, vol);
}

void Octree::diff_negative(Octnode* current, const OCTVolume* vol) {
    current->evaluate( vol ); // this sets the inside/outside flags
    if ( current->childcount == 0) { // process only leaf-nodes
        if ( current->inside ) { // inside nodes should be deleted
            Octnode* parent = current->parent;
            assert( parent );
            if (parent) {
                unsigned int delete_index = current->idx;
                assert( delete_index >=0 && delete_index <=7 ); 
                delete parent->child[ delete_index ];
                parent->child[ delete_index ]=0;
                --parent->childcount;
                assert( parent->childcount >=0 && parent->childcount <=8);
                
                if (parent->childcount == 0)  { // if the parent has become a leaf node
                    parent->evaluate( vol );
                    assert( parent->inside  ); // then it is itself inside
                }
            }
        } else if (current->outside) {// we do nothing to outside nodes.
        } else {// these are intermediate nodes
            if ( current->depth < (this->max_depth) ) { // subdivide, if possible
                current->subdivide();
                assert( current->childcount == 8 );
                for(int m=0;m<8;++m) {
                    assert(current->child[m]); // when we subdivide() there must be a child.
                    if ( vol->bb.overlaps( current->child[m]->bb ) )
                        diff_negative( current->child[m], vol); // build child
                }
            } else { 
                // max depth reached, can't subdivide
            }
        }
    } else {
        for(int m=0;m<8;++m) { // not a leaf, so go deeper into tree
                if ( current->child[m] ) {
                    if ( vol->bb.overlaps( current->child[m]->bb ) )
                        diff_negative( current->child[m], vol); // build child
                }
        }
    }

}

/// string repr
std::string Octree::str() const {
    std::ostringstream o;
    o << " Octree:\n";
    std::vector<Octnode*> nodelist;
    Octree::get_leaf_nodes(root, nodelist);
    std::vector<int> nodelevel(this->max_depth);
    BOOST_FOREACH( Octnode* n, nodelist) {
        ++nodelevel[n->depth];
    }
    o << "  " << nodelist.size() << " leaf-nodes:\n";
    int m=0;
    BOOST_FOREACH( int count, nodelevel) {
        o << "depth="<<m <<" has " << count << " nodes\n";
        ++m;
    }
    return o.str();
}

} // end namespace
// end of file octree.cpp