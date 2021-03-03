#!/usr/bin/env python3

# Filename: DiffeoRegistrar.py
# Contributors: Tom Birdsong
# Purpose: Python implementation of Slicer SALT mesh-to-mesh registration Cxx
# CLI.
#    Ported from https://github.com/slicersalt/RegistrationBasedCorrespondence/

import logging
import os

import itk

from .meshtomeshregistrar import MeshToMeshRegistrar

class DiffeoRegistrar(MeshToMeshRegistrar):
    
    # Common definitions
    MAX_ITERATIONS = 200

    Dimension = 3
    PixelType = itk.F
    ImageType = itk.Image[PixelType, Dimension]
    TemplateImageType = ImageType
    TargetImageType = ImageType

    MaskPixelType = itk.UC
    MaskImageType = itk.Image[MaskPixelType, Dimension]
    FixedPointSetType = itk.PointSet[itk.F, Dimension]

    MeshType = itk.Mesh[itk.F, Dimension]

    # Definitions for diffeometric registration
    STANDARD_DEVIATIONS = 1.0


    def __init__(self):
        self.initialize()

    # Expose method to reset persistent objects as desired
    def initialize(self):
        VectorPixelType = itk.Vector[itk.F, self.Dimension]
        DisplacementFieldType = itk.Image[VectorPixelType, self.Dimension]
        DiffeoRegistrationFilterType = \
            itk.DiffeomorphicDemonsRegistrationFilter[self.ImageType,
                                                      self.ImageType,
                                                      DisplacementFieldType]
        self.filter = DiffeoRegistrationFilterType.New(
            StandardDeviations=self.STANDARD_DEVIATIONS)

    # Register meshes with diffeomorphic demons algorithm
    def register(self,
                 template_mesh:MeshType,
                 target_mesh:MeshType,
                 filepath:str=None,
                 verbose=False,
                 max_iterations=MAX_ITERATIONS) -> MeshType:
        template_image = self.mesh_to_image(template_mesh)
        target_image = self.mesh_to_image(target_mesh,template_image)

        self.filter.SetFixedImage(template_image)
        self.filter.SetMovingImage(target_image)

        self.filter.SetNumberOfIterations(max_iterations)
      
        def print_iteration():
            metric = self.filter.GetMetric()
            print(f'{metric}')

        if(verbose):
            self.filter.AddObserver(itk.ProgressEvent(),print_iteration)

        # Run registration
        self.filter.Update()
        
        # Update template mesh to match target
        DisplacementFieldTransformType = \
            itk.DisplacementFieldTransform[itk.F, self.Dimension]
        transform = DisplacementFieldTransformType.New()
        transform.SetDisplacementField(self.filter.GetOutput())

        # TODO pythonic style
        TransformFilterType = \
            itk.TransformMeshFilter[self.MeshType,
                                    self.MeshType,
                                    itk.Transform[itk.F,self.Dimension,self.Dimension]]
        transform_filter = TransformFilterType.New(Input=template_mesh,
            Transform=transform)
        transform_filter.Update()

        if filepath is not None:
            itk.meshwrite(transform_filter.GetOutput(),filepath)
            if(verbose):
                print(f'Wrote resulting mesh to {filepath}')

        return transform_filter.GetOutput()
