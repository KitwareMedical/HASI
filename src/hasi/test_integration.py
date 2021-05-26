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
import pytest

INPUT_DIRECTORY = 'test/Input/'
OUTPUT_DIRECTORY = 'test/Output/'
MESH_DIRECTORY = 'meshes/'
CORRESPONDENCE_DIRECTORY = 'registered/'

IMAGE_FILES = [
    '902-R-femur-label.nrrd',
    '906-R-femur-label.nrrd']
    #'907-R-femur-label.nrrd']
    #'908-R-femur-label.nrrd',
    #'F9-3wk-01-R-femur-label.nrrd',
    #'F9-3wk-02-R-femur-label.nrrd',
    #'F9-8wk-01-R-femur-label.nrrd']

IMAGE_URLS = [
    'https://data.kitware.com/api/v1/file/60abeaed2fa25629b99afce1/download',
    'https://data.kitware.com/api/v1/file/60abeaf22fa25629b99afcef/download']
    #'https://data.kitware.com/api/v1/file/60abeaf52fa25629b99afcfb/download',
    #'https://data.kitware.com/api/v1/file/60abeaf82fa25629b99afd09/download',
    #'https://data.kitware.com/api/v1/file/60abeadc2fa25629b99afcb4/download',
    #'https://data.kitware.com/api/v1/file/60abeae12fa25629b99afcc0/download',
    #'https://data.kitware.com/api/v1/file/60abeae52fa25629b99afccb/download']

os.makedirs(INPUT_DIRECTORY, exist_ok=True)
os.makedirs(OUTPUT_DIRECTORY, exist_ok=True)
os.makedirs(OUTPUT_DIRECTORY+MESH_DIRECTORY,exist_ok=True)
os.makedirs(OUTPUT_DIRECTORY+CORRESPONDENCE_DIRECTORY,exist_ok=True)

for idx in range(len(IMAGE_FILES)):
    image_file = IMAGE_FILES[idx]
    if not os.path.exists(INPUT_DIRECTORY + image_file):
        url = IMAGE_URLS[idx]
        urlretrieve(url, INPUT_DIRECTORY + image_file)


@pytest.mark.dependency()
def test_create_atlas():
    import hasi.align

    # Read in images
    images = list()
    for image_file in IMAGE_FILES:
        images.append(itk.imread(INPUT_DIRECTORY + image_file, itk.UC))

    # Paste into standard space
    images = hasi.align.paste_to_common_space(images)

    # Downsample template image
    TEMPLATE_IDX = 0
    SPARSE_DOWNSAMPLE_RATIO = 14
    template_image = \
        hasi.align.downsample_images([images[TEMPLATE_IDX]],SPARSE_DOWNSAMPLE_RATIO)[0]

    # Downsample dense images
    DENSE_DOWNSAMPLE_RATIO = 2
    images = hasi.align.downsample_images(images,DENSE_DOWNSAMPLE_RATIO)

    # Generate initial template mesh
    FEMUR_OBJECT_PIXEL_VALUE = 1
    template_mesh = hasi.align.binary_image_list_to_meshes([template_image],
                                                           object_pixel_value=FEMUR_OBJECT_PIXEL_VALUE)[0]

    # Generate meshes
    meshes = hasi.align.binary_image_list_to_meshes(images,
                                                    object_pixel_value=FEMUR_OBJECT_PIXEL_VALUE)

    # Write out sample meshes for later shape analysis
    for idx in range(len(IMAGE_FILES)):
        mesh_file = IMAGE_FILES[idx].replace('.nrrd','.vtk')
        mesh = meshes[idx]
        itk.meshwrite(mesh,OUTPUT_DIRECTORY+MESH_DIRECTORY+mesh_file)

    # Iteratively refine atlas
    NUM_ITERATIONS = 3
    for iteration in range(NUM_ITERATIONS):
        updated_mesh = hasi.align.refine_template_from_population(template_mesh=template_mesh,
                                                             target_meshes=meshes,
                                                             registration_iterations=200)
        distance = hasi.align.get_pairwise_hausdorff_distance(updated_mesh, template_mesh)
        template_mesh = updated_mesh

    # Verify final alignment distance
    assert 0.1 < distance < 0.5

    itk.meshwrite(updated_mesh,OUTPUT_DIRECTORY+MESH_DIRECTORY+'atlas.vtk')


@pytest.mark.dependency(depends=["test_create_atlas"])
def test_make_correspondence():
    import hasi.align

    # Load meshes
    atlas_mesh = itk.meshread(OUTPUT_DIRECTORY+MESH_DIRECTORY+'atlas.vtk')

    meshes = list()
    for idx in range(len(IMAGE_FILES)):
        mesh_file = IMAGE_FILES[idx].replace('.nrrd','.vtk')
        meshes.append(itk.meshread(OUTPUT_DIRECTORY+MESH_DIRECTORY+mesh_file, itk.F))

    # Get mesh correspondence
    for idx in range(len(meshes)):
        mesh = meshes[idx]
        registered_atlas = hasi.align.register_template_to_sample(
            template_mesh=atlas_mesh,
            sample_mesh=mesh,
            max_iterations=500)
        mesh = hasi.align.resample_template_from_target(
            template_mesh=registered_atlas,
            target_mesh=mesh)
        meshes[idx] = mesh

    # Write out for shape analysis
    for idx in range(len(IMAGE_FILES)):
        mesh = meshes[idx]
        mesh_file = IMAGE_FILES[idx].replace('.nrrd','.vtk')
        itk.meshwrite(mesh,OUTPUT_DIRECTORY+CORRESPONDENCE_DIRECTORY+mesh_file)


@pytest.mark.dependency(depends=["test_make_correspondence"])
def test_shape_analysis():
    import numpy as np
    import sklearn.model_selection
    from dwd.dwd import DWD
    import hasi.classify

    # Load correspondence meshes
    meshes = list()
    for idx in range(len(IMAGE_FILES)):
        mesh_file = IMAGE_FILES[idx].replace('.nrrd','.vtk')
        meshes.append(itk.meshread(OUTPUT_DIRECTORY+CORRESPONDENCE_DIRECTORY+mesh_file,itk.F))

    # Prepare data
    STEP_SIZE = 10
    TRAIN_SIZE=0.6
    features = hasi.classify.make_point_features(meshes, step=STEP_SIZE)

    # For labels we will try to distinguish a given mesh from the others
    # based only on shape features
    labels = np.array([('902' in image_file) for image_file in IMAGE_FILES])

    #X_train, X_test, y_train, y_test = \
    #    sklearn.model_selection.train_test_split(features,labels,TRAIN_SIZE=0.6)

    # Train classifier
    # (For integration test we simply train on the population)
    classifier = DWD(C='auto')
    classifier.fit(features,np.squeeze(labels))

    # Predict labels, expecting 100% correct
    predictions = classifier.predict(features)
    correct = np.squeeze(predictions) == labels
    assert sum(correct) == len(labels)

    # Predict distances
    distances = hasi.classify.get_distances(classifier, features)
    assert all(10 < abs(distance) < 20 for distance in distances)
