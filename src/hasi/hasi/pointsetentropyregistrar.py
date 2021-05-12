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

# Purpose:  Python implementation of mesh-to-mesh registration
#     using point set entropy-based registration
# Related: https://github.com/InsightSoftwareConsortium/ITKTrimmedPointSetRegistration/blob/master/examples/TrimmedPointSetRegistrationExample.cxx

import logging
import os
import pytest

import itk
from .meshtomeshregistrar import MeshToMeshRegistrar

class PointSetEntropyRegistrar(MeshToMeshRegistrar):
    # Common definitions
    MAX_ITERATIONS = 200

    Dimension = 3
    PixelType = itk.F
    ImageType = itk.Image[PixelType, Dimension]
    TemplateImageType = ImageType
    TargetImageType = ImageType

    MeshType = itk.Mesh[itk.F, Dimension]

    # Class definitions
    PointSetType = itk.PointSet[itk.F,Dimension]
    TransformType = itk.AffineTransform[itk.D, Dimension]

    def __init__(self):
        super(self.__class__,self).__init__()

    def initialize(self):
        # Optimizer is exposed so that calling scripts can reference with custom observers
        self.optimizer = itk.GradientDescentOptimizerv4.New(
            MaximumStepSizeInPhysicalUnits=3,
            MinimumConvergenceValue=-1,
            ConvergenceWindowSize=1,
            DoEstimateLearningRateOnce=False,
            DoEstimateLearningRateAtEachIteration=False,
            ReturnBestParametersAndValue=True)

    def register(self,
                 template_mesh:MeshType=None,
                 template_point_set:PointSetType=None,
                 target_mesh:MeshType=None,
                 target_point_set:PointSetType=None,
                 filepath:str=None,
                 verbose=False,
                 metric=None,
                 transform=None,
                 resample_rate=1.0,
                 max_iterations=MAX_ITERATIONS,
                 learning_rate=1.0,
                 resample_from_target=False) -> (TransformType, PointSetType):

        # Verify a template and target were passed in
        if(not (template_mesh or template_point_set) or not (target_mesh or target_point_set)):
            raise Exception('Registration requires both a template and a target!')

        # Need both a mesh and a point set representing the template and the transform
        if(not template_mesh):
            template_mesh = self.MeshType.New(Points=template_point_set.GetPoints())
        else:
            template_point_set = self.PointSetType.New(Points=template_mesh.GetPoints())

        if(not target_mesh):
            target_mesh = self.MeshType.New(Points=target_point_set.GetPoints())
        else:
            target_point_set = self.PointSetType.New(Points=target_mesh.GetPoints())

        # Optionally resample to improve performance
        if(resample_rate != 1.0):
            template_point_set = self.randomly_sample_mesh_points(mesh=template_point_set, sampling_rate=resample_rate)
            target_point_set = self.randomly_sample_mesh_points(mesh=target_point_set, sampling_rate=resample_rate)

        # Define registration components
        if not transform:
            transform = self.TransformType.New()
            transform.SetIdentity()
        
        #template_image = self.mesh_to_image(template_mesh)
        # Set physical dimensions for transform
        #fixed_physical_dimensions = list()
        #fixed_origin = list(template_image.GetOrigin())

        #for i in range(0, self.Dimension):
        #    fixed_physical_dimensions.append(template_image.GetSpacing()[i] * \
        #        (template_image.GetLargestPossibleRegion().GetSize()[i] - 1))

        if not metric:
            metric = itk.JensenHavrdaCharvatTsallisPointSetToPointSetMetricv4[self.PointSetType].New(
                FixedPointSet=template_point_set,
                MovingPointSet=target_point_set,
                MovingTransform=transform,
                PointSetSigma=20.0,
                KernelSigma=3.0,
                UseAnisotropicCovariances=False,
                CovarianceKNeighborhood=5,
                EvaluationKNeighborhood=10,
                Alpha=1.1)
            #metric.SetVirtualDomainFromImage( template_image );

        metric.SetFixedPointSet(template_point_set)
        metric.SetMovingPointSet(target_point_set)
        metric.SetMovingTransform(transform)

        metric.Initialize()

        # Define scales to guide gradient descent steps
        ShiftScalesType = \
            itk.RegistrationParameterScalesFromPhysicalShift[type(metric)]
        shift_scale_estimator = ShiftScalesType.New(
            Metric=metric,
            VirtualDomainPointSet=metric.GetVirtualTransformedPointSet())

        self.optimizer.SetMetric(metric)
        self.optimizer.SetNumberOfIterations(max_iterations)
        self.optimizer.SetScalesEstimator(shift_scale_estimator)
        self.optimizer.SetLearningRate(learning_rate)

        def print_iteration():
            print(f'{self.optimizer.GetCurrentIteration()} {self.optimizer.GetCurrentMetricValue()}')

        if verbose:
            self.optimizer.AddObserver(itk.AnyEvent(), print_iteration)

        self.optimizer.StartOptimization()

        if(verbose):
            print(f'Number of iterations run: {self.optimizer.GetCurrentIteration()}')
            print(f'Final metric value: {self.optimizer.GetCurrentMetricValue()}')
            print(f'Final transform position: {list(self.optimizer.GetCurrentPosition())}')
            print(f'Optimizer scales: {list(self.optimizer.GetScales())}')
            print(f'Optimizer learning rate: {self.optimizer.GetLearningRate()}')

        moving_transform = metric.GetMovingTransform()

        registered_template_mesh = itk.transform_mesh_filter(template_mesh, transform=moving_transform)

        if(resample_from_target):
            registered_template_mesh = self.resample_template_from_target(registered_template_mesh, target_mesh)

        # Write out
        if filepath is not None:
            itk.meshwrite(registered_template_mesh,filepath)
            if(verbose):
                print(f'Wrote resulting point set to {filepath}')

        return (transform, registered_template_mesh)
