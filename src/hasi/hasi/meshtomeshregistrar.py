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
                       filepath:str=None, verbose=False) -> MeshType:
        raise NotImplementedError('Base class does not implement register functionality')

    # Convert itk.Mesh object to 3-dimensional itk.Image object
    # Defined for mesh of type itk.F
    def mesh_to_image(self, mesh:MeshType, reference_image:ImageType=None) -> ImageType :

        MeshToImageFilterType = itk.TriangleMeshToBinaryImageFilter[self.MeshType,self.ImageType]

        if not reference_image:
            bounds = mesh.GetBoundingBox().GetBounds()
            spacing = ((bounds[1] - bounds[0]) / 90,
                        (bounds[3] - bounds[2]) / 90,
                        (bounds[5] - bounds[4]) / 90)
            origin = (bounds[0] - 5 * spacing[0],
                        bounds[2] - 5 * spacing[1],
                        bounds[4] - 5 * spacing[2])
            size = (100,100,100)
        
            filter = MeshToImageFilterType.New(Input=mesh,
                                                Origin=origin,
                                                Spacing=spacing,
                                                Size=size)
        else:
            filter = MeshToImageFilterType.New(Input=mesh,
                                                Origin=reference_image.GetOrigin(),
                                                Spacing=reference_image.GetSpacing(),
                                                Size=reference_image.GetLargestPossibleRegion().GetSize())
        filter.Update()

        DistanceType = itk.SignedMaurerDistanceMapImageFilter[self.ImageType,self.ImageType]
        distance = DistanceType.New(Input=filter.GetOutput())
        distance.Update()

        image = distance.GetOutput()
        return image




