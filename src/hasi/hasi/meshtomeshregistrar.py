#!/usr/bin/env python3

# Copyright NumFOCUS
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#        https://www.apache.org/licenses/LICENSE-2.0.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Purpose: Base Python class for implementations of mesh-to-mesh registration algorithms.
#   Initially ported from https://github.com/slicersalt/RegistrationBasedCorrespondence/
import logging
import os
import random

import itk
import numpy as np

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
    PointSetType = itk.PointSet[itk.F, Dimension]

    def __init__(self,verbose:bool=False):
        self.verbose = verbose
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

    # Randomly sample mesh to get a reduced point set
    # This may not be suitable for low-density meshes
    def randomly_sample_mesh_points(self, mesh:MeshType, sampling_rate:float=1.0) -> PointSetType:
        if(sampling_rate > 1.0):
            raise ValueError('Cannot resample with a rate greater than 1.0!')
        
        selected_points = set()
        num_source_points = mesh.GetNumberOfPoints()
        resampled_set = self.PointSetType.New()

        while resampled_set.GetNumberOfPoints() < sampling_rate * num_source_points:
            point_index = random.randrange(0, num_source_points)
            while(point_index in selected_points):
                point_index = random.randrange(0, num_source_points)
            selected_points.add(point_index)

            resampled_set.SetPoint(
                resampled_set.GetNumberOfPoints(),
                mesh.GetPoint(point_index))

        return resampled_set
    
    @staticmethod
    def resample_template_from_target(template_mesh:MeshType, target_mesh:MeshType) -> MeshType:
        # Set samples from target mesh points
        measurement = itk.Vector[itk.F,3]()
        sample = itk.ListSample[type(measurement)].New()

        # Use available wrapping for itk.VectorContainer
        if (itk.ULL, itk.ULL) in itk.VectorContainer.keys():
            neighbors = itk.VectorContainer[itk.ULL, itk.ULL].New()
        else:
            neighbors = itk.VectorContainer[itk.UL, itk.UL].New()

        sample.Resize(target_mesh.GetNumberOfPoints())
        for i in range(0, target_mesh.GetNumberOfPoints()):
            measurement.SetVnlVector(target_mesh.GetPoint(i).GetVnlVector())
            sample.SetMeasurementVector(i, measurement)

        # Build kd-tree
        tree_generator = itk.KdTreeGenerator[type(sample)].New(Sample=sample, BucketSize=16)
        tree_generator.Update()

        # Iteratively update template points to nearest neighbor in target point set
        tree = tree_generator.GetOutput()

        for i in range(0, template_mesh.GetNumberOfPoints()):
            measurement.SetVnlVector(template_mesh.GetPoint(i).GetVnlVector())
            tree.Search(measurement, 1, neighbors)
            template_mesh.SetPoint(i, target_mesh.GetPoint(neighbors.GetElement(0)))

        return template_mesh
