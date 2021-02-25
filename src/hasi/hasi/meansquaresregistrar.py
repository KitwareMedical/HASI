#!/usr/bin/env python3

# Filename: MeanSquaresRegistrar.py
# Contributors: Tom Birdsong
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

    # TODO investigate if this is useful
    LinearInterpolatorType = \
        itk.LinearInterpolateImageFunction[ImageType, itk.D]
    

    def __init__(self):
        super(self.__class__,self).__init__()

    def initialize(self):
        # Optimizer is exposed so that calling scripts can reference with custom observers
        self.optimizer = itk.LBFGSBOptimizerv4.New(CostFunctionConvergenceFactor=self.COST_FN_CONVERGENCE_FACTOR,
            GradientConvergenceTolerance=self.GRADIENT_CONVERGENCE_TOLERANCE,
            MaximumNumberOfFunctionEvaluations=self.MAX_FUNCTION_EVALUATIONS,
            MaximumNumberOfCorrections=self.MAX_CORRECTIONS)


    # Register two 3D images with an LBFGSB optimizer
    def register(self,
                 template_mesh:MeshType,
                 target_mesh:MeshType,
                 filepath:str=None,
                 verbose=False,
                 num_iterations=MAX_ITERATIONS) -> MeshType:

        template_image = self.mesh_to_image(template_mesh)
        target_image = self.mesh_to_image(target_mesh, template_image)

        ImageType = type(template_image)
        Dimension = template_image.GetImageDimension()
            
        metric = itk.MeanSquaresImageToImageMetricv4[ImageType,ImageType].New()

        # Set physical dimensions for transform
        fixed_physical_dimensions = list()
        fixed_origin = list(template_image.GetOrigin())

        for i in range(0, Dimension):
            fixed_physical_dimensions.append(template_image.GetSpacing()[i] * \
                (template_image.GetLargestPossibleRegion().GetSize()[i] - 1))
            
        TransformType = itk.BSplineTransform[itk.D, Dimension, self.V_SPLINE_ORDER]
        transform = TransformType.New(TransformDomainOrigin=fixed_origin,
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

        if(verbose):
            self.optimizer.AddObserver(itk.IterationEvent(),
                                            print_iteration)

        self.optimizer.SetNumberOfIterations(num_iterations)

        # Define object to handle image registration
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

        # TODO functional interface
        # image_registration_method()

        # Report results
        if(verbose):
            print('Solution = ' + str(list(transform.GetParameters())))

        # Update fixed image
        # TODO try inverting moving transform and compare results

        transformed_mesh = itk.transform_mesh_filter(template_mesh, transform=transform)

        # Write out
        if filepath is not None:
            itk.meshwrite(transformed_mesh, filepath)
            if(verbose):
                print(f'Wrote resulting mesh to {filepath}')

        return transformed_mesh
