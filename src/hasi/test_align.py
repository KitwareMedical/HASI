#!/usr/bin/env python3

# Filename:     test_align.py
# Contributors: Tom Birdsong
# Purpose:  Pytest definitions for atlas alignment classes
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
TARGET_HEALTHY_IMAGE_FILE = 'test/Input/901-R-femur-label.nrrd'
TARGET_UNHEALTHY_IMAGE_FILE = 'test/Input/901-L-femur-label.nrrd'

os.makedirs('test', exist_ok=True)
os.makedirs('test/Input', exist_ok=True)
os.makedirs('test/Output', exist_ok=True)

if not os.path.exists(TEMPLATE_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/608b006d2fa25629b970f139/download'
    urlretrieve(url, TEMPLATE_MESH_FILE)
    
if not os.path.exists(TARGET_MESH_FILE):
    url = 'https://data.kitware.com/api/v1/file/5f9daaba50a41e3d1924dae9/download'
    urlretrieve(url, TARGET_MESH_FILE)
    
if not os.path.exists(TARGET_HEALTHY_IMAGE_FILE):
    url = 'https://data.kitware.com/api/v1/file/5eea20bd9014a6d84ec75df1/download'
    urlretrieve(url, TARGET_HEALTHY_IMAGE_FILE)
    
if not os.path.exists(TARGET_UNHEALTHY_IMAGE_FILE):
    url = 'https://data.kitware.com/api/v1/file/5eea20bb9014a6d84ec75de5/download'
    urlretrieve(url, TARGET_UNHEALTHY_IMAGE_FILE)
    
template_mesh = itk.meshread(TEMPLATE_MESH_FILE, itk.F)
target_mesh = itk.meshread(TARGET_MESH_FILE, itk.F)
target_healthy_image = itk.imread(TARGET_HEALTHY_IMAGE_FILE)
target_unhealthy_image = itk.imread(TARGET_UNHEALTHY_IMAGE_FILE)

mesh_pixel_type, dimension = itk.template(template_mesh)[1]

def test_hasi_import():
    # Import succeeds
    from hasi import align


def test_mesh_to_image():
    # Import succeeds
    from hasi.align import mesh_to_image

    # Mesh-to-image executes without error
    images = mesh_to_image([template_mesh, target_mesh])

    # Images were generated
    assert(all([type(image) == itk.Image[mesh_pixel_type, dimension] for image in images]))

    # Images occupy the same physical space
    assert(all([itk.spacing(image) == itk.spacing(images[0]) for image in images]))
    assert(all([itk.size(image) == itk.size(images[0]) for image in images]))
    assert(all([itk.origin(image) == itk.origin(images[0]) for image in images]))

    # Mesh-to-image can run with reference image
    image_from_reference = mesh_to_image(target_mesh, reference_image=images[0])

    # Type and image attributes match previous output
    assert(type(image_from_reference) == type(images[0]))
    assert(itk.spacing(image_from_reference) == itk.spacing(images[0]))
    assert(itk.size(image_from_reference) == itk.size(images[0]))
    assert(itk.origin(image_from_reference) == itk.origin(images[0]))


def test_resample_from_target():
    DISTANCE_THRESHOLD = 0.7

    # Import succeeds
    from hasi.align import resample_template_from_target, get_pairwise_hausdorff_distance

    # Resample succeeds
    # Note for actual use we would want to register first
    resampled_template = resample_template_from_target(template_mesh, target_mesh)

    # Template has same number of points before and after
    assert(resampled_template.GetNumberOfPoints() == template_mesh.GetNumberOfPoints())

    # Template points moved
    assert(resampled_template.GetPoint(0) != template_mesh.GetPoint(0))

    # All template points lie on template mesh
    assert(any([resampled_template.GetPoint(1) == target_mesh.GetPoint(j)
                    for j in range(target_mesh.GetNumberOfPoints())]))

    # Movement distance was within threshold
    distance = get_pairwise_hausdorff_distance(resampled_template, template_mesh)
    assert(distance > 0.0 and distance < DISTANCE_THRESHOLD)


def test_paste_to_common_space():
    # Import succeeds
    from hasi.align import mesh_to_image, paste_to_common_space
    
    # Paste succeeds
    resized_images = paste_to_common_space([target_healthy_image, target_unhealthy_image])
    assert(len(resized_images) == 2)

    # Verify common space
    TOLERANCE = 1e-6
    assert(itk.size(resized_images[0]) == itk.size(resized_images[1]))
    assert(all([itk.spacing(resized_images[0])[dim] - itk.spacing(resized_images[1])[dim] < TOLERANCE
                for dim in range(dimension)]))


def test_downsample_images():
    DOWNSAMPLE_RATIO = 2.0

    # Import succeeds
    from hasi.align import mesh_to_image, downsample_images

    # Downsample succeeds
    images = downsample_images([target_healthy_image, target_unhealthy_image], DOWNSAMPLE_RATIO)
    assert(len(images) == 2)

    # Downsampled image dimensions adjusted by expected ratio
    assert(itk.size(images[0]) ==
           [int(itk.size(target_healthy_image)[dim] / DOWNSAMPLE_RATIO) for dim in range(dimension)])
    assert(itk.spacing(images[0]) ==
           [itk.spacing(target_healthy_image)[dim] * DOWNSAMPLE_RATIO for dim in range(dimension)])
    assert(itk.size(images[1]) ==
           [int(itk.size(target_unhealthy_image)[dim] / DOWNSAMPLE_RATIO) for dim in range(dimension)])
    assert(itk.spacing(images[1]) ==
           [itk.spacing(target_unhealthy_image)[dim] * DOWNSAMPLE_RATIO for dim in range(dimension)])


def test_generate_meshes():
    # Import succeeds
    from hasi.align import binary_image_list_to_meshes

    # Mesh generation succeeds
    meshes = binary_image_list_to_meshes([target_healthy_image, target_unhealthy_image], itk.F, 1)
    assert(len(meshes) == 2)

    # Mesh vertices match expectation
    assert(meshes[0].GetNumberOfPoints() == 900671)
    assert(meshes[1].GetNumberOfPoints() == 855276)


def test_refine_template_from_population():
    # Import succeeds
    from hasi.align import refine_template_from_population, get_pairwise_hausdorff_distance

    # Refinement succeeds
    mesh_result = refine_template_from_population(template_mesh,
                                                  [target_mesh],
                                                  registration_iterations=100)

    # Distance matches expectation
    distance = get_pairwise_hausdorff_distance(mesh_result, template_mesh)
    assert(distance > 0.7 and distance < 1.0)
