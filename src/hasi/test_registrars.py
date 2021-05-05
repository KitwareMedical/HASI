#!/usr/bin/env python3

# Filename:     test_registrars.py
# Contributors: Tom Birdsong
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
POINT_SET_METRIC_MAXIMUM_THRESHOLD = 0.0001
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
    MESH_OUTPUT_FILE = 'test/Output/testMeanSquaresRegisterOutput.vtk'

    # Class is imported
    from hasi.meansquaresregistrar import MeanSquaresRegistrar

    # Class is instantiable
    registrar = MeanSquaresRegistrar()

    # Registration executes
    (transform, template_output_mesh) = registrar.register(template_mesh=template_mesh,
                                            target_mesh=target_mesh,
                                            filepath=MESH_OUTPUT_FILE)

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

    # Mesh was output
    assert(os.path.isfile(MESH_OUTPUT_FILE))

    # File output matches mesh
    output_mesh = itk.meshread(MESH_OUTPUT_FILE)
    assert(output_mesh.GetNumberOfPoints() == \
        template_output_mesh.GetNumberOfPoints())
    assert(all(output_mesh.GetPoint(i) == template_output_mesh.GetPoint(i)
                for i in range(0,output_mesh.GetNumberOfPoints())))

    # Resample template
    template_resampled = registrar.resample_template_from_target(template_output_mesh, target_mesh)

    # Clean up
    os.remove(MESH_OUTPUT_FILE)

# Test registration with diffeomorphic demons image registration
def test_diffeo_registration():
    MESH_OUTPUT_FILE = 'test/Output/testDiffeoRegisterOutput.vtk'

    # Class is imported
    from hasi.diffeoregistrar import DiffeoRegistrar

    # Class is instantiable
    registrar = DiffeoRegistrar()

    # Registration executes without error
    (transform, template_output_mesh) = registrar.register(template_mesh=template_mesh,
                                                           target_mesh=target_mesh,
                                                           filepath=MESH_OUTPUT_FILE)
    
    # Transform was output
    assert(type(transform) == registrar.TransformType)

    # Mesh was generated
    assert(type(template_output_mesh) == type(template_mesh))
    
    # Mesh is a resampling of the original template
    assert(template_output_mesh.GetNumberOfPoints() == \
        template_mesh.GetNumberOfPoints())

    # Mesh was output
    assert(os.path.isfile(MESH_OUTPUT_FILE))

    # Optimization converged
    assert(registrar.filter.GetMetric() <
            DIFFEO_METRIC_MAXIMUM_THRESHOLD)

    # Optimization did not exceed allowable iterations
    assert(registrar.filter.GetElapsedIterations() <= MAX_ITERATIONS)

    # Clean up
    os.remove(MESH_OUTPUT_FILE)

def test_pointset_registration():
    MESH_OUTPUT_FILE = 'test/Output/testPointSetRegistrarOutput.vtk'
    RESAMPLED_OUTPUT_FILE = 'test/Output/testPointSetResampledOutput.vtk'

    # Class is imported
    from hasi.pointsetentropyregistrar import PointSetEntropyRegistrar

    # Class is instantiable
    registrar = PointSetEntropyRegistrar()

    # Registration executes without error
    (transform, template_output_mesh) = registrar.register(template_mesh=template_mesh,
                               target_mesh=target_mesh,
                               filepath=MESH_OUTPUT_FILE,
                               resample_from_target=False)
    
    # Transform was output
    assert(type(transform) == registrar.TransformType)

    # Mesh was generated
    assert(type(template_output_mesh) == type(template_mesh))
    
    # Mesh is a transformation of the original template
    assert(template_output_mesh.GetNumberOfPoints() == \
        template_mesh.GetNumberOfPoints())

    # Mesh was output
    assert(os.path.isfile(MESH_OUTPUT_FILE))

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

    # Clean up
    os.remove(MESH_OUTPUT_FILE)
