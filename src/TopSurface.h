//Copyright (c) 2017 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef TOPSURFACE_H
#define TOPSURFACE_H

#include "utils/polygon.h" //For the polygon areas.
#include "sliceDataStorage.h" //For the input mesh.

namespace cura
{

class SliceMeshStorage;

class TopSurface
{
public:
    /*!
     * Create an empty top surface area.
     */
    TopSurface();

    /*!
     * \brief Generate new top surface for a specific layer.
     *
     * The surface will be generated by subtracting the layer above from the
     * current layer. Anything that is leftover is then part of the top surface
     * (since there is nothing above it).
     *
     * \param mesh The mesh to generate the top surface area for.
     * \param layer_number The layer to generate the top surface area for.
     */
    TopSurface(SliceMeshStorage& mesh, size_t layer_number);

    /*!
     * \brief The areas of top surface, for each layer.
     */
    Polygons areas;

    /*!
     * \brief Generate paths for sanding over the top surface.
     *
     * This generates an infill pattern over the top surface that is supposed to
     * strike the surface smooth by melting it with the hot nozzle, without
     * extruding anything.
     *
     * \param mesh The settings base to get our sanding settings and skin angles
     * from.
     * \param line_config The configuration of the sanding lines to use. Note
     * that the flow might still get adjusted by the sanding settings.
     * \param[out] layer The output g-code layer to put the resulting lines in.
     */
    void sand(const SliceMeshStorage& mesh, const GCodePathConfig& line_config, LayerPlan& layer);

    /*!
     * \brief Generate paths for sanding between a lower layer and this one.
     *
     * This draws a lot of lines diagonally moving from a lower layer to this
     * layer. These lines start on points along the perimeter of the lower layer
     * and take the shortest route towards this layer. This technique should
     * cover most of the area between the two layers. It may also miss some
     * areas that are not directly between the two layers and it may go outside
     * of the polygon of the lower layer. TODO: Prevent this.
     *
     * \param mesh The settings base to get our sanding settings from.
     * \param line_config The configuration of the sanding lines to use. Note
     * that the flow might still get adjusted by the sanding settings.
     * \param top_surface_below The top surface of the layer below this layer.
     * \param[out] layer The output g-code layer to put the resulting lines in.
     */
    bool sandBelow(const SliceMeshStorage& mesh, const GCodePathConfig& line_config, const TopSurface& top_surface_below, LayerPlan& layer);
};

}

#endif /* TOPSURFACE_H */

