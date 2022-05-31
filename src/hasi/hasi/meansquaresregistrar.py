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

# Purpose: Python implementation of Slicer SALT mesh-to-mesh registration Cxx
# CLI.
#    Ported from https://github.com/slicersalt/RegistrationBasedCorrespondence/

import logging
import os

import itk

from .meshtomeshregistrar import MeshToMeshRegistrar

class MeanSquaresRegistrar(MeshToMeshRegistrar):
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

    # Default function values
    MAX_ITERATIONS = 200
    V_SPLINE_ORDER = 3
    GRID_NODES_IN_ONE_DIMENSION = 4

    COST_FN_CONVERGENCE_FACTOR = 1e7
    GRADIENT_CONVERGENCE_TOLERANCE = 1e-35
    MAX_FUNCTION_EVALUATIONS = 200
    MAX_CORRECTIONS = 7

    NUMBER_OF_LEVELS = 1
    SMOOTHING_SIGMAS_PER_LEVEL = [0] * NUMBER_OF_LEVELS
    SHRINK_FACTORS_PER_LEVEL = [1] * Dimension

    TransformType = itk.BSplineTransform[itk.D, Dimension, V_SPLINE_ORDER]
    
    def __init__(self, verbose:bool=False):
        super(self.__class__,self).__init__(verbose=verbose)

    def initialize(self):
        # Optimizer is exposed so that calling scripts can reference with custom observers
        self.optimizer = itk.LBFGSBOptimizerv4.New(
            MaximumNumberOfFunctionEvaluations=self.MAX_FUNCTION_EVALUATIONS,
            MaximumNumberOfCorrections=self.MAX_CORRECTIONS)

        # Monitor optimization via observer
        if(self.verbose):
            def print_iteration():
                print(f'Iteration: {self.optimizer.GetCurrentIteration()}'
                      f' Metric: {self.optimizer.GetCurrentMetricValue()}'
                      f' Infinity Norm: {self.optimizer.GetInfinityNormOfProjectedGradient()}')
            self.optimizer.AddObserver(itk.IterationEvent(),
                                            print_iteration)

    # Register two 3D images with an LBFGSB optimizer
    def register(self,
                 template_mesh:MeshType,
                 target_mesh:MeshType,
                 num_iterations:int=MAX_ITERATIONS,
                 convergence_factor:float=COST_FN_CONVERGENCE_FACTOR,
                 gradient_convergence_tolerance:float=GRADIENT_CONVERGENCE_TOLERANCE) \
                     -> (TransformType, MeshType):

        (template_image, target_image) = self.mesh_to_image([template_mesh, target_mesh])

        ImageType = type(template_image)
        Dimension = template_image.GetImageDimension()
            
        metric = itk.MeanSquaresImageToImageMetricv4[ImageType,ImageType].New()

        # Set physical dimensions for transform
        fixed_physical_dimensions = list()
        fixed_origin = list(template_image.GetOrigin())

        for i in range(0, Dimension):
            fixed_physical_dimensions.append(template_image.GetSpacing()[i] * \
                (template_image.GetLargestPossibleRegion().GetSize()[i] - 1))
            
        transform = self.TransformType.New(TransformDomainOrigin=fixed_origin,
                                            TransformDomainPhysicalDimensions=fixed_physical_dimensions,
                                            TransformDomainDirection=template_image.GetDirection())
        transform.SetTransformDomainMeshSize([self.GRID_NODES_IN_ONE_DIMENSION - self.V_SPLINE_ORDER] * Dimension)

        # Transform parameters unbounded for typical registration problem
        number_of_parameters = transform.GetNumberOfParameters()
        self.optimizer.SetBoundSelection([0] * number_of_parameters)
        self.optimizer.SetUpperBound([0] * number_of_parameters)
        self.optimizer.SetLowerBound([0] * number_of_parameters)

        self.optimizer.SetCostFunctionConvergenceFactor(convergence_factor)
        self.optimizer.SetGradientConvergenceTolerance(gradient_convergence_tolerance)
        self.optimizer.SetNumberOfIterations(num_iterations)

        # Define object to handle image registration

        # TODO use functional interface image_registration_method()
        RegistrationType = \
            itk.ImageRegistrationMethodv4[ImageType,ImageType]
        registration = RegistrationType.New(InitialTransform=transform,
            FixedImage=template_image,
            MovingImage=target_image,
            Metric=metric,
            Optimizer=self.optimizer,
            NumberOfLevels=self.NUMBER_OF_LEVELS,
            ShrinkFactorsPerLevel=self.SHRINK_FACTORS_PER_LEVEL)
        registration.SetSmoothingSigmasPerLevel(self.SMOOTHING_SIGMAS_PER_LEVEL)
        registration.InPlaceOn()

        # Run registration
        # NOTE ignore warning: "LBFGSBOptimizer does not support
        #     scaling, all scales set to one"
        #     Registration likely attempts to set scales by default,
        #     no observed impact on performance from this warning
        registration.Update()
        
        # Update template
        transformed_mesh = itk.transform_mesh_filter(template_mesh, transform=transform)
        return (transform, transformed_mesh)
