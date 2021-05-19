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

# Purpose:  Pytest definitions for mesh-to-mesh registration classes
#           defined in the src/hasi/hasi module

# Directions:
# - run `pytest .`

import os
import sys
from urllib.request import urlretrieve
module_path = os.path.abspath(os.path.join('.'))

if module_path not in sys.path:
  sys.path.append(module_path)

import itk

MEANSQUARES_METRIC_MAXIMUM_THRESHOLD = 0.015
DIFFEO_METRIC_MAXIMUM_THRESHOLD = 0.25
POINT_SET_METRIC_MAXIMUM_THRESHOLD = 0.0236
MAX_ITERATIONS = 200

TEMPLATE_MESH_FILE = 'test/Input/906-R-atlas.obj'
TARGET_MESH_FILE = 'test/Input/901-R-mesh.vtk'

os.makedirs('test', exist_ok=True)
os.makedirs('test/Input', exist_ok=True)
os.makedirs('test/Output', exist_ok=True)

if not os.path.exists(TEMPLATE_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/608b006d2fa25629b970f139/download'
    urlretrieve(url, TEMPLATE_MESH_FILE)
    
if not os.path.exists(TARGET_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/5f9daaba50a41e3d1924dae9/download'
    urlretrieve(url, TARGET_MESH_FILE)

template_mesh = itk.meshread(TEMPLATE_MESH_FILE, itk.F)
target_mesh = itk.meshread(TARGET_MESH_FILE, itk.F)

# Test registration with mean squares image metric
def test_meansquares_registration():
    # Class is imported
    from hasi.meansquaresregistrar import MeanSquaresRegistrar

    # Class is instantiable
    registrar = MeanSquaresRegistrar()

    # Registration executes
    (transform, template_output_mesh) = registrar.register(template_mesh=template_mesh,
                                            target_mesh=target_mesh)

    # Mesh was generated
    assert(type(template_output_mesh) == type(template_mesh))

    # Mesh is a resampling of the original template
    assert(template_output_mesh.GetNumberOfPoints() == \
        template_mesh.GetNumberOfPoints())

    # Optimization converged
    assert(registrar.optimizer.GetCurrentMetricValue() <
            MEANSQUARES_METRIC_MAXIMUM_THRESHOLD)
  
    # Optimization did not exceed allowable iterations
    assert(registrar.optimizer.GetCurrentIteration() <= MAX_ITERATIONS)

    # Transform was output
    assert(type(transform) == registrar.TransformType)

    # Resample template
    template_resampled = registrar.resample_template_from_target(template_output_mesh, target_mesh)

# Test registration with diffeomorphic demons image registration
def test_diffeo_registration():
    # Class is imported
    from hasi.diffeoregistrar import DiffeoRegistrar

    # Class is instantiable
    registrar = DiffeoRegistrar()

    # Registration executes without error
    (transform, template_output_mesh) = registrar.register(template_mesh=template_mesh,
                                                           target_mesh=target_mesh)
    
    # Transform was output
    assert(type(transform) == registrar.TransformType)

    # Mesh was generated
    assert(type(template_output_mesh) == type(template_mesh))
    
    # Mesh is a resampling of the original template
    assert(template_output_mesh.GetNumberOfPoints() == \
        template_mesh.GetNumberOfPoints())

    # Optimization converged
    assert(registrar.filter.GetMetric() <
            DIFFEO_METRIC_MAXIMUM_THRESHOLD)

    # Optimization did not exceed allowable iterations
    assert(registrar.filter.GetElapsedIterations() <= MAX_ITERATIONS)

def test_pointset_registration():
    # Class is imported
    from hasi.pointsetentropyregistrar import PointSetEntropyRegistrar

    # Class is instantiable
    registrar = PointSetEntropyRegistrar()

    # Registration executes without error
    metric = itk.EuclideanDistancePointSetToPointSetMetricv4[itk.PointSet[itk.F,3]].New()
    (transform, template_output_mesh) = registrar.register(template_mesh=template_mesh,
                                                           target_mesh=target_mesh,
                                                           metric=metric,
                                                           minimum_convergence_value=1e-6,
                                                           convergence_window_size=3)
    
    # Transform was output
    assert(type(transform) == registrar.TransformType)

    # Mesh was generated
    assert(type(template_output_mesh) == type(template_mesh))
    
    # Mesh is a transformation of the original template
    assert(template_output_mesh.GetNumberOfPoints() == \
        template_mesh.GetNumberOfPoints())

    # Optimization converged
    assert(registrar.optimizer.GetCurrentMetricValue() <
           POINT_SET_METRIC_MAXIMUM_THRESHOLD)

    # Optimization did not exceed allowable iterations
    assert(registrar.optimizer.GetCurrentIteration() <= registrar.MAX_ITERATIONS)

    # Resample template
    template_resampled = \
        registrar.resample_template_from_target(template_output_mesh, target_mesh)
    
    # Mesh is a transformation of the original template
    assert(template_resampled.GetNumberOfPoints() == \
        template_mesh.GetNumberOfPoints())
