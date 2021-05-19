#!/usr/bin/env python3

# Copyright NumFOCUS
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Purpose: Python implementation of Slicer SALT mesh-to-mesh registration Cxx
# CLI.
#    Ported from https://github.com/slicersalt/RegistrationBasedCorrespondence/

import logging
import os

import itk

from .meshtomeshregistrar import MeshToMeshRegistrar

class DiffeoRegistrar(MeshToMeshRegistrar):
    
    # Type definitions for function annotations
    Dimension = 3
    PixelType = itk.F
    ImageType = itk.Image[PixelType, Dimension]
    TemplateImageType = ImageType
    TargetImageType = ImageType

    MaskPixelType = itk.UC
    MaskImageType = itk.Image[MaskPixelType, Dimension]
    FixedPointSetType = itk.PointSet[itk.F, Dimension]

    MeshType = itk.Mesh[itk.F, Dimension]
    TransformType = itk.DisplacementFieldTransform[itk.F, Dimension]
    
    VectorPixelType = itk.Vector[itk.F, Dimension]
    DisplacementFieldType = itk.Image[VectorPixelType, Dimension]
    DiffeoRegistrationFilterType = \
        itk.DiffeomorphicDemonsRegistrationFilter[ImageType,
                                                    ImageType,
                                                    DisplacementFieldType]

    # Default function values
    MAX_ITERATIONS = 200
    STANDARD_DEVIATIONS = 1.0
    MAX_RMS_ERROR = 0.0

    def __init__(self, verbose:bool=False):
        super(self.__class__,self).__init__(verbose=verbose)

    # Expose method to reset persistent objects as desired
    def initialize(self):
        self.filter = self.DiffeoRegistrationFilterType.New(
            StandardDeviations=self.STANDARD_DEVIATIONS)

        # Print iteration updates
        if(self.verbose):
            def print_iteration():
                print(f'Iteration: {self.filter.GetElapsedIterations()}'
                      f' Metric: {self.filter.GetMetric()}'
                      f' RMS Change: {self.filter.GetRMSChange()}')
            self.filter.AddObserver(itk.ProgressEvent(),print_iteration)

    # Register meshes with diffeomorphic demons algorithm
    def register(self,
                 template_mesh:MeshType,
                 target_mesh:MeshType,
                 max_iterations:int=MAX_ITERATIONS,
                 max_rms_error:float=MAX_RMS_ERROR,
                 verbose=False) -> (TransformType, MeshType):

        (template_image, target_image) = self.mesh_to_image([template_mesh, target_mesh])

        self.filter.SetFixedImage(template_image)
        self.filter.SetMovingImage(target_image)
        self.filter.SetNumberOfIterations(max_iterations)
        self.filter.SetMaximumRMSError(max_rms_error)

        # Run registration
        self.filter.Update()
        
        # Update template mesh to match target
        transform = self.TransformType.New()
        transform.SetDisplacementField(self.filter.GetOutput())

        # TODO pythonic style
        TransformFilterType = \
            itk.TransformMeshFilter[self.MeshType,
                                    self.MeshType,
                                    itk.Transform[itk.F,self.Dimension,self.Dimension]]
        transform_filter = TransformFilterType.New(Input=template_mesh,
            Transform=transform)
        transform_filter.Update()

        return (transform, transform_filter.GetOutput())
