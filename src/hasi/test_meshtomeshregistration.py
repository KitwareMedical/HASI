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
DIFFEO_METRIC_MAXIMUM_THRESHOLD = 0.0001
MAX_ITERATIONS = 200

FIXED_MESH_FILE = 'test/Input/901-L-mesh.vtk'
MOVING_MESH_FILE = 'test/Input/901-R-mesh.vtk'

os.makedirs('test', exist_ok=True)
os.makedirs('test/Input', exist_ok=True)
os.makedirs('test/Output', exist_ok=True)

if not os.path.exists(FIXED_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/5f9daaae50a41e3d1924dae1/download'
    urlretrieve(url, FIXED_MESH_FILE)
    
if not os.path.exists(MOVING_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/5f9daaae50a41e3d1924dae1/download'
    urlretrieve(url, MOVING_MESH_FILE)

os.makedirs('test/Output', exist_ok=True)

mesh1 = itk.meshread(FIXED_MESH_FILE)
mesh2 = itk.meshread(MOVING_MESH_FILE)

# Test base class import and functions
def test_mesh_to_image():
    # Class is imported
    from hasi.meshtomeshregistrar import MeshToMeshRegistrar

    # Class is instantiable
    registrar = MeshToMeshRegistrar()

    # Initialize runs without error
    registrar.initialize()

    # Mesh-to-image executes without error
    img1 = registrar.mesh_to_image(mesh1)

    # Image was generated
    assert(type(img1) == itk.Image[itk.F, 3])

    # Mesh-to-image can run with reference image
    img2 = registrar.mesh_to_image(mesh2, img1)

    # Image was generated
    assert(type(img2) == itk.Image[itk.F, 3])

    # Images occupy the same physical space
    assert(img2.GetOrigin() == img1.GetOrigin())
    assert(img2.GetSpacing() == img1.GetSpacing())

# Test registration with mean squares image metric
def test_meansquares_registration():
    MESH_OUTPUT = 'test/Output/testMeanSquaresRegisterOutput.vtk'

    # Class is imported
    from hasi.meansquaresregistrar import MeanSquaresRegistrar

    # Class is instantiable
    registrar = MeanSquaresRegistrar()

    # Registration executes
    mesh3 = registrar.register(mesh1,mesh2,filepath=MESH_OUTPUT)

    # Mesh was generated
    assert(type(mesh3) == type(mesh1))

    # Optimization converged
    assert(registrar.optimizer.GetCurrentMetricValue() <
            MEANSQUARES_METRIC_MAXIMUM_THRESHOLD)
  
    # Optimization did not exceed allowable iterations
    assert(registrar.optimizer.GetCurrentIteration() <= MAX_ITERATIONS)
  
    # Mesh was output
    assert(os.path.isfile(MESH_OUTPUT))

    # File output matches mesh
    fileMesh3 = itk.meshread(MESH_OUTPUT)
    assert(fileMesh3.GetNumberOfPoints() == mesh3.GetNumberOfPoints())
    assert(all(mesh3.GetPoint(i) == fileMesh3.GetPoint(i)
                for i in range(0,mesh3.GetNumberOfPoints())))

    # Clean up
    os.remove(MESH_OUTPUT)

# Test registration with diffeomorphic demons image registration
def test_diffeo_registration():
    MESH_OUTPUT = 'test/Output/testDiffeoRegisterOutput.vtk'

    # Class is imported
    from hasi.diffeoregistrar import DiffeoRegistrar

    # Class is instantiable
    registrar = DiffeoRegistrar()

    # Registration executes without error
    mesh4 = registrar.register(mesh1,mesh2,filepath=MESH_OUTPUT)

    # Mesh was generated
    assert(type(mesh4) == type(mesh1))

    # Mesh was output
    assert(os.path.isfile(MESH_OUTPUT))

    # Optimization converged
    assert(registrar.filter.GetMetric() <
            DIFFEO_METRIC_MAXIMUM_THRESHOLD)

    # Optimization did not exceed allowable iterations
    assert(registrar.filter.GetElapsedIterations() <= MAX_ITERATIONS)

    # Clean up
    os.remove(MESH_OUTPUT)
