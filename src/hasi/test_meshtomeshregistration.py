#!/usr/bin/env python3

# Filename:     test_meshtomeshregistration.py
# Contributors: Tom Birdsong
# Purpose:  Pytest definitions for mesh-to-mesh registration classes
#           defined in the src/hasi/hasi module

# Directions:
# - cd into the src/hasi directory
# - run `pytest .`

import os
import sys
from urllib.request import urlretrieve
module_path = os.path.abspath(os.path.join('.'))

if module_path not in sys.path:
  sys.path.append(module_path)

import itk

MEANSQUARES_METRIC_MAXIMUM_THRESHOLD = 0.002
DIFFEO_METRIC_MAXIMUM_THRESHOLD = 0.0002
POINT_SET_METRIC_MAXIMUM_THRESHOLD = 0.0001
MAX_ITERATIONS = 200

TEMPLATE_MESH_FILE = 'test/Input/901-L-mesh.vtk'
TARGET_MESH_FILE = 'test/Input/901-R-mesh.vtk'

os.makedirs('test', exist_ok=True)
os.makedirs('test/Input', exist_ok=True)
os.makedirs('test/Output', exist_ok=True)

if not os.path.exists(TEMPLATE_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/5f9daaae50a41e3d1924dae1/download'
    urlretrieve(url, TEMPLATE_MESH_FILE)
    
if not os.path.exists(TARGET_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/5f9daaba50a41e3d1924dae9/download'
    urlretrieve(url, TARGET_MESH_FILE)

os.makedirs('test/Output', exist_ok=True)

template_mesh = itk.meshread(TEMPLATE_MESH_FILE)
target_mesh = itk.meshread(TARGET_MESH_FILE)

# Test base class import and functions
def test_mesh_to_image():
    # Class is imported
    from hasi.meshtomeshregistrar import MeshToMeshRegistrar

    # Class is instantiable
    registrar = MeshToMeshRegistrar()

    # Initialize runs without error
    registrar.initialize()

    # Mesh-to-image executes without error
    img1 = registrar.mesh_to_image(template_mesh)

    # Image was generated
    assert(type(img1) == itk.Image[itk.F, 3])

    # Mesh-to-image can run with reference image
    img2 = registrar.mesh_to_image(target_mesh, img1)
    assert(type(img2) == itk.Image[itk.F, 3])

    # Images occupy the same physical space
    assert(img2.GetOrigin() == img1.GetOrigin())
    assert(img2.GetSpacing() == img1.GetSpacing())

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
                               resample_rate=0.001,
                               resample_from_target=True)
    
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
    assert(registrar.optimizer.GetCurrentMetricValue() <
           POINT_SET_METRIC_MAXIMUM_THRESHOLD)

    # Optimization did not exceed allowable iterations
    assert(registrar.optimizer.GetCurrentIteration() <= registrar.MAX_ITERATIONS)

    # Resample template
    template_resampled = \
        registrar.resample_template_from_target(template_output_mesh, target_mesh)

    # Clean up
    os.remove(MESH_OUTPUT_FILE)
