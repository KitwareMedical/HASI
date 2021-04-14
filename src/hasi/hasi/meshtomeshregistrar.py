#!/usr/bin/env python3

# Filename: MeshToMeshRegistration
# Contributors: Tom Birdsong
# Purpose: Python implementation of Slicer SALT mesh-to-mesh registration Cxx
# CLI.
#    Ported from https://github.com/slicersalt/RegistrationBasedCorrespondence/
import logging
import os

import itk


class MeshToMeshRegistrar:
    # Common definitions
    MAX_ITERATIONS = 200

    Dimension = 3
    PixelType = itk.F
    ImageType = itk.Image[PixelType, Dimension]
    FixedImageType = ImageType
    MovingImageType = ImageType

    MaskPixelType = itk.UC
    MaskImageType = itk.Image[MaskPixelType, Dimension]
    FixedPointSetType = itk.PointSet[itk.F, Dimension]

    MeshType = itk.Mesh[itk.F, Dimension]

    def __init__(self):
        self.initialize()

    # Abstract method for developer to explicitly reset internal state objects
    def initialize(self):
        pass
    
    def register(self, template_mesh:MeshType, target_mesh:MeshType,
                       filepath:str=None, verbose=False):
        raise NotImplementedError('Base class does not implement register functionality')

    # Convert itk.Mesh object to 3-dimensional itk.Image object
    def mesh_to_image(self, meshes:list, reference_image:ImageType=None) -> ImageType :
        # Allow single mesh as input
        if type(meshes) == self.MeshType:
            meshes = [meshes]

        MeshToImageFilterType = itk.TriangleMeshToBinaryImageFilter[self.MeshType,self.ImageType]
        images = list()

        if not reference_image:
            # Set bounds to largest region encompassing all meshes
            # Bounds format: [x_min x_max y_min y_max z_min z_max]
            bounds = meshes[0].GetBoundingBox().GetBounds()
            for mesh in meshes[1:]:
                mesh_bounds = mesh.GetBoundingBox().GetBounds()
                for dim in range(0, self.Dimension):
                    bounds[dim*2] = min(bounds[dim*2], mesh_bounds[dim*2])
                    bounds[(dim*2)+1] = max(bounds[(dim*2)+1], mesh_bounds[(dim*2)+1])

            # Calculate spacing, origin, and size with 5 pixel buffer around mesh
            spacing = ((bounds[1] - bounds[0]) / 90,
                        (bounds[3] - bounds[2]) / 90,
                        (bounds[5] - bounds[4]) / 90)
            origin = (bounds[0] - 5 * spacing[0],
                        bounds[2] - 5 * spacing[1],
                        bounds[4] - 5 * spacing[2])
            size = (100,100,100)
            direction = None
        else:
            # Use given parameters
            origin = reference_image.GetOrigin()
            spacing = reference_image.GetSpacing()
            size = reference_image.GetLargestPossibleRegion().GetSize()
            direction = reference_image.GetDirection()

        # Generate image for each mesh
        for mesh in meshes:
            filter = MeshToImageFilterType.New(Input=mesh,
                                                Origin=origin,
                                                Spacing=spacing,
                                                Size=size)
            # Direction defaults to identity
            if direction:
                filter.SetDirection(direction)
            filter.Update()

            DistanceType = itk.SignedMaurerDistanceMapImageFilter[self.ImageType,self.ImageType]
            distance = DistanceType.New(Input=filter.GetOutput())
            distance.Update()

            images.append(distance.GetOutput())

        return images[0] if len(images) == 1 else images
