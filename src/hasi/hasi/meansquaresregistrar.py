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

class MeanSquaresRegistrar(MeshToMeshRegistrar):
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

    # Definitions for default registration
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
    

    def __init__(self):
        super(self.__class__,self).__init__()

    def initialize(self):
        # Optimizer is exposed so that calling scripts can reference with custom observers
        self.optimizer = itk.LBFGSBOptimizerv4.New(CostFunctionConvergenceFactor=self.COST_FN_CONVERGENCE_FACTOR,
            GradientConvergenceTolerance=self.GRADIENT_CONVERGENCE_TOLERANCE,
            MaximumNumberOfFunctionEvaluations=self.MAX_FUNCTION_EVALUATIONS,
            MaximumNumberOfCorrections=self.MAX_CORRECTIONS)
        # TODO initialize verbose output with observer


    # Register two 3D images with an LBFGSB optimizer
    def register(self,
                 template_mesh:MeshType,
                 target_mesh:MeshType,
                 filepath:str=None,
                 verbose=False,
                 num_iterations=MAX_ITERATIONS) -> (TransformType, MeshType):

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

        # Monitor optimization via observer
        def print_iteration():
            iteration = self.optimizer.GetCurrentIteration()
            metric = self.optimizer.GetCurrentMetricValue()
            infnorm = self.optimizer.GetInfinityNormOfProjectedGradient()
            print(f'{iteration} {metric} {infnorm}')

        # FIXME adds a duplicate observer if multiple calls without re-initialization
        if(verbose):
            self.optimizer.AddObserver(itk.IterationEvent(),
                                            print_iteration)

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

        # Report results
        if(verbose):
            print('Solution = ' + str(list(transform.GetParameters())))
        
        # Update template
        transformed_mesh = itk.transform_mesh_filter(template_mesh, transform=transform)

        # Write out
        # TODO move away from monolithic design, leave write responsibility to user
        if filepath is not None:
            itk.meshwrite(transformed_mesh, filepath)
            if(verbose):
                print(f'Wrote resulting mesh to {filepath}')

        return (transform, transformed_mesh)
