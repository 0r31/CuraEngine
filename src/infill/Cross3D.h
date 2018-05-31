/** Copyright (C) 2018 Ultimaker - Released under terms of the AGPLv3 License */
#ifndef INFILL_CROSS_3D_H
#define INFILL_CROSS_3D_H

#include <array>
#include <list>

#include "../utils/optional.h"

#include "../utils/intpoint.h"
#include "../utils/AABB3D.h"
#include "../utils/Range.h"
#include "../utils/LineSegment.h"
#include "../utils/SVG.h" // debug

#include "DensityProvider.h"

namespace cura
{

class Cross3DTest; // fwd decl
/*!
 * Cross3D is a class for generating the Cross 3D infill pattern with varying density accross X, Y and Z.
 * Each layer is a space filling curve and across the layers the pattern forms a space filling surface,
 * which satisfied overhang angle and has no bridges.
 * 
 * The 3D surface is oscillating / pulsating in and out across the layers.
 * This way it touches itself and creates a foam like structure which is similarly flexible in all directions.
 * 
 * An underlying subdivision structure is generated, which subdivides the 3D space into regular parts: prisms.
 * These prisms have a triangular base and rectangular sides.
 * The prism heights correspond to half the wave length of the oscillation pattern:
 * the surface patch across a prism is either expanding or contracting.
 * 
 * For a layer each prism crossing that layer is sliced into a triangle.
 * The triangle grid thus created for a layer is then used to generate a Sierpinski-like fractal, similar to \ref SierpinskiFill
 * 
 * This class effectively combines the algorithms from \ref SierpinskiFill and \ref SquareSubdivision.
 * Because the space filling curve can be seen as a way to linearize 2D space,
 * the 3D space filling surface is similar to a 2D square subdivision grid.
 * Quantization error is the error between actual density of a prism cell and required density of the user specified distribution.
 * Quantizatoin error should be distributed in two dimensions:
 * 'left' and 'right' that is: along the curve itself in the XY plane
 * up and down in the Z dimension.
 * 
 * 
 * We start with a cubic aabb and splice that vertically into 2 prisms of type I: half-cubic.
 * A half-cubic prism is subdivided vertically into 2 quarter-cubic prisms.
 * A quarter cubic prism is subdivided vertically and horizontally into 4 half-cubic prisms.
 * 
 * 
 * 
 * ALGORITHM OVERVIEW
 * ==================
 * 1) Generate the tree of all prisms and their subdivisions
 * 2) Decide at which 'height' in the tree the eventual pattern will be: decide on the recursion depth at each location
 * 3) Walk from the bottom to the top and create layers
 * 
 * 1) Tree generation
 * - Make dummy root node
 * - add first prisms by hand
 * - recursively set the sierpinski connectivity of each cell, the required volume, the actual volume etc.
 * 
 * 2) Create subdivision pattern
 * We start from the root and only decide for each node whether we subdivide it or not;
 * we never 'unsubdivide' a node.
 * All nodes which have been chosen to be subdivided are marked as such in \ref Cross3D::Cell::is_subdivided
 * As such the creation of the subdivision structure is a boundary on the subdivision tree which is only pushed downward toward the leaves of the tree.
 * 
 * The current subdivision structure is a net of linked cells.
 * The cells have the following info:
 * - the geometric prism
 * - links to neighboring cells in the current subdivision structure
 * - error values of built up and redistributed quatization error
 * 
 * There are several ways in which to decide on the recurison depth at each location.
 * 
 * 2.1) Minimal required density
 * - made easily by recursively subdiviging each cell (and neighbording restructing cells) which is lower than the required density
 * 
 * 2.2) Average density
 * First we decide on the minimal density at each location, while keeping density change cancades balanced around input density requirement changes.
 * Then we apply dithering to get the eventual subdivision pattern
 * 
 * 2.2.1) Average density lower boundary
 * This is 50% of the complexity of this class.
 * This is the most difficult algorithm.
 * Induced quantization errors are redistributed to nearest cells, so as to make the subdivision structure balanced.
 * If no errors would have been distributed the final pattern would either be too dense or too sparse
 * near regions where the input density requirement distribution has sharp edges.
 * Such errors cause problems for the next dithering phase, which would then oscillate between several subdivision levels too dense and several subdivision levels too sparse
 * in the regions just after the sharp density edges.
 * 
 * 2.2.2) Dithering
 * Walk over the subdivision struction from the left bottom to the top right
 * decide for each cell whether to subdivide once more or not,
 * without reconsidering the thus introduced children for subdivision again.
 * 
 * 
 * 3) Walking across layers
 * For each layer there is a single linear sequence of prisms to cross.
 * In order to efficiently compute the sequence,
 * we compute the bottom sequence once
 * and update it for each layer when it crosses the top of any prism in the sequence.
 * 
 * For such a sequence we look at all triangles of all prisms in the sequence.
 * From this sequence of triangles, we can generate a Sierpinski curve,
 * or the CrossFill curve.
 * When generating the curve, we make sure not to overlap with other line segments in the crossfill pattern.
 * 
 * 
 * 
 * 
 * Triangle subdivision explanation adopted from SierpinskiFill
 * ------------------------------------------------------------
 * Triangles are subdivided into two children like so:
 * |\       |\        .
 * |A \     |A \      .
 * |    \   |    \    . where S is always the 90* straight corner
 * |     S\ |S____B\  .       The direction between A and B is maintained
 * |      / |S    A/
 * |    /   |    /      Note that the polygon direction flips between clockwise and SSW each subdivision
 * |B /     |B /
 * |/       |/
 * 
 * The direction of the space filling curve along each triangle is recorded:
 * 
 * |\                           |\                                        .
 * |B \  AS_TO_BS               |B \   AS_TO_AB                           .
 * |  ↑ \                       |  ↑ \                                    .
 * |  ↑  S\  subdivides into    |S_↑__A\                                  .
 * |  ↑   /                     |S ↑  B/                                  .
 * |  ↑ /                       |  ↑ /                                    .
 * |A /                         |A /   AB_TO_BS                           .
 * |/                           |/                                        .
 *                                                                        .
 * |\                           |\                                        .
 * |B \  AS_TO_AB               |B \   AS_TO_BS                           .
 * |    \                       |↖   \                                    .
 * |↖    S\  subdivides into    |S_↖__A\                                  .
 * |  ↖   /                     |S ↑  B/                                  .
 * |    /                       |  ↑ /                                    .
 * |A /                         |A /   AB_TO_BS                           .
 * |/                           |/                                        .
 *                                                                        .
 * |\                           |\                                        .
 * |B \  AB_TO_BS               |B \   AS_TO_AB                           .
 * |  ↗ \                       |  ↑ \                                    .
 * |↗    S\  subdivides into    |S_↑__A\                                  .
 * |      /                     |S ↗  B/                                  .
 * |    /                       |↗   /                                    .
 * |A /                         |A /   AS_TO_BS                           .
 * |/                           |/                                        .
 * 
 */
class Cross3D
{
    friend class Cross3DTest;
    using idx_t = int_fast32_t;
protected:
    struct Cell; // forward decl
public:

    Cross3D(const DensityProvider& density_provider, const AABB3D aabb, const int max_depth, coord_t line_width);

    /*!
     * Simple wrapper class for the structure which walks through slices of the subdivision structure
     */
    struct SliceWalker
    {
        std::list<const Cell*> layer_sequence; //!< The sequence of cells on a given slice through the subdivision structure. These are in Sierpinski order.
    };

    /*!
     * Initialize the the tree structure from the density specification
     */ 
    void initialize();

    /*!
     * Create a pattern with the required density or more at each location.
     */
    void createMinimalDensityPattern();

    SliceWalker getBottomSequence() const;
    void advanceSequence(SliceWalker& sequence, coord_t new_z) const;

    Polygon generateSierpinski(const SliceWalker& sequence) const;
protected:
    static constexpr uint_fast8_t max_subdivision_count = 4; //!< Prisms are subdivided into 2 or 4 prisms
    static constexpr uint_fast8_t number_of_sides = 4; //!< Prisms connect above, below and before and after

    struct Triangle
    {
        /*!
         * The order in
         * Which the edges of the triangle are crossed by the Sierpinski curve.
         */
        enum class Direction
        {
            AC_TO_AB,
            AC_TO_BC,
            AB_TO_BC
        };
        Point straight_corner; //!< C, the 90* corner of the triangle
        Point a; //!< The corner closer to the start of the space filling curve
        Point b; //!< The corner closer to the end of the space filling curve
        Direction dir; //!< The (order in which) edges being crossed by the Sierpinski curve.
        bool straight_corner_is_left; //!< Whether the \ref straight_corner is left of the curve, rather than right. I.e. whether triangle ABC is counter-clockwise

        Triangle(
            Point straight_corner,
            Point a,
            Point b,
            Direction dir,
            bool straight_corner_is_left)
        : straight_corner(straight_corner)
        , a(a)
        , b(b)
        , dir(dir)
        , straight_corner_is_left(straight_corner_is_left)
        {}

        //! initialize with invalid data
        Triangle()
        {}

        std::array<Triangle, 2> subdivide() const;

        /*!
         * Get the first edge of this triangle crossed by the Sierpinski and/or Cross Fractal curve.
         * The from location is always toward the inside of the curve.
         */
        LineSegment getFromEdge() const;
        /*!
         * Get the second edge of this triangle crossed by the Sierpinski and/or Cross Fractal curve.
         * The from location is always toward the inside of the curve.
         */
        LineSegment getToEdge() const;
        //! Get the middle of the triangle
        Point getMiddle() const;
        //! convert into a polyogon with correct winding order
        Polygon toPolygon() const;
    };
    struct Prism
    {
        Triangle triangle;
        Range<coord_t> z_range;
        bool is_expanding; //!< Whether the surface is moving away from the space filling tree when going from bottom to top. (Though the eventual surface might be changed due to neighboring constraining cells)

        //! simple constructor
        Prism(
            Triangle triangle,
            coord_t z_min,
            coord_t z_max,
            bool is_expanding
        )
        : triangle(triangle)
        , z_range(z_min, z_max)
        , is_expanding(is_expanding)
        {}

        //! initialize with invalid data
        Prism()
        {}

        bool isHalfCube() const;
        bool isQuarterCube() const;
    };

    enum class Direction : int
    {
        LEFT = 0, // ordered on polarity and dimension: first X from less to more, then Y
        RIGHT = 1,
        DOWN = 2,
        UP = 3,
        COUNT = 4
    };
    Direction opposite(Direction in);
    uint_fast8_t opposite(uint_fast8_t in);

    struct Link; // fwd decl
    using LinkIterator = typename std::list<Link>::iterator;
    struct Link
    {
        idx_t to_index;
        std::optional<LinkIterator> reverse; //!< The link in the inverse direction
        float loan; //!< amount of requested_filled_area loaned from one cell to another, when subdivision of the former is prevented by the latter. This value should always be positive.
        idx_t from_index()
        {
            assert(reverse);
            return (*reverse)->to_index;
        }
        Link& getReverse() const
        {
            assert(reverse);
            return **reverse;
        }
        Link(idx_t to_index)
        : to_index(to_index)
        , loan(0.0)
        {}
    };
    struct Cell
    {
        Prism prism;
        idx_t index; //!< index into \ref Cross3D::cell_data
        char depth; //!< recursion depth
        float volume; //!< The volume of the prism in mm^3
        float filled_volume_allowance; //!< The volume to be filled corresponding to the average density requested by the volumetric density specification.
        float minimally_required_density; //!< The largest required density across this area. For when the density specification is the minimal density at each locatoin.
        bool is_subdivided;
        std::array<std::list<Link>, number_of_sides> adjacent_cells; //!< the adjacent cells for each edge/face of this cell. Ordered: before, after, below, above

        std::array<idx_t, max_subdivision_count> children; //!< children. Ordered: down-left, down-right, up-left, up-right

        Cell(const Prism& prism, const idx_t index, const int depth)
        : prism(prism)
        , index(index)
        , depth(depth)
        , volume(-1)
        , filled_volume_allowance(0)
        , minimally_required_density(-1)
        , is_subdivided(false)
        {
//             children.fill(-1); --> done in createTree(...)
        }

        uint_fast8_t getChildCount() const;
    };

    std::vector<Cell> cell_data; //!< Storage for all the cells. The data of a binary/quaternary tree in depth first order.

    AABB3D aabb;
    int max_depth;
    coord_t line_width; //!< The line width of the fill lines

    const DensityProvider& density_provider; //!< function which determines the requested infill density of a triangle defined by two consecutive edges.


    float getDensity(const Cell& cell) const;

private:
    constexpr static uint_fast8_t getNumberOfSides()
    {
        return number_of_sides;
    }

    // Tree creation:
    void createTree();
    void createTree(Cell& sub_tree_root, int max_depth);
    void setVolume(Cell& sub_tree_root);
    void setSpecificationAllowance(Cell& sub_tree_root);

    // Lower bound sequence:
    

    float getActualizedVolume(const Cell& cell) const;
    bool canSubdivide(const Cell& cell) const;
    bool isConstrained(const Cell& cell) const;
    bool isConstrainedBy(const Cell& constrainee, const Cell& constrainer) const;
    
    void subdivide(Cell& cell);
    void initialConnection(Cell& before, Cell& after, Direction dir);
    
    /*!
     * 
     * \param a_to_b The side of \p a to check for being next to cell b. Sides are ordered: before, after, below, above (See \ref Cross3D::Cell::adjacent_cells and \ref Cross3D::Direction)
     */
    bool isNextTo(const Cell& a, const Cell& b, Direction a_to_b) const;

    // output


    // debug
    void debugCheckDepths() const;
    void debugCheckVolumeStats() const;

    void debugOutputCell(const Cell& cell, SVG& svg, float drawing_line_width, bool horizontal_connections_only) const;
    void debugOutputTriangle(const Triangle& triangle, SVG& svg, float drawing_line_width) const;
    void debugOutputLink(const Link& link, SVG& svg) const;
    void debugOutput(const SliceWalker& sequence, SVG& svg, float drawing_line_width) const;
    void debugOutputTree(SVG& svg, float drawing_line_width) const;
    void debugOutputSequence(SVG& svg, float drawing_line_width) const;
    void debugOutputSequence(const Cell& cell, SVG& svg, float drawing_line_width) const;
};

} // namespace cura


#endif // INFILL_CROSS_3D_H
