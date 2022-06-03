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

# Purpose: Python functions for mesh registration and alignment
#           in HASI pipeline

import itk


# Convert itk.Mesh object to 3-dimensional itk.Image object
def mesh_to_image(meshes:list, reference_image:itk.Image=None) -> itk.Image :
    # Allow single mesh as input
    if type(meshes) != list:
        meshes = [meshes]

    mesh_type = type(meshes[0])
    pixel_type, dimension = itk.template(mesh_type)[1]
    image_type = itk.Image[pixel_type, dimension]

    mesh_to_image_filter_type = itk.TriangleMeshToBinaryImageFilter[mesh_type,image_type]
    images = list()

    if not reference_image:
        # Set bounds to largest region encompassing all meshes
        # Bounds format: [x_min x_max y_min y_max z_min z_max]
        bounds = meshes[0].GetBoundingBox().GetBounds()
        for mesh in meshes[1:]:
            mesh_bounds = mesh.GetBoundingBox().GetBounds()
            for dim in range(0, dimension):
                bounds[dim*2] = min(bounds[dim*2], mesh_bounds[dim*2])
                bounds[(dim*2)+1] = max(bounds[(dim*2)+1], mesh_bounds[(dim*2)+1])

        # Calculate spacing, origin, and size with 5 pixel buffer around mesh
        spacing = ((bounds[1] - bounds[0]) / 90,
                    (bounds[3] - bounds[2]) / 90,
                    (bounds[5] - bounds[4]) / 90)
        origin = (bounds[0] - 5 * spacing[0],
                    bounds[2] - 5 * spacing[1],
                    bounds[4] - 5 * spacing[2])
        size = (100,100,100)
        direction = itk.Matrix[itk.D,dimension,dimension].GetIdentity()
    else:
        # Use given parameters
        origin = reference_image.GetOrigin()
        spacing = reference_image.GetSpacing()
        size = reference_image.GetLargestPossibleRegion().GetSize()
        direction = reference_image.GetDirection()

    # Generate image for each mesh
    for mesh in meshes:
        mesh_to_image_filter = mesh_to_image_filter_type.New(Input=mesh,
                                                             Origin=origin,
                                                             Spacing=spacing,
                                                             Size=size,
                                                             Direction=direction)
        mesh_to_image_filter.Update()

        distance = itk.signed_maurer_distance_map_image_filter(mesh_to_image_filter.GetOutput())
        images.append(distance)

    return images[0] if len(images) == 1 else images


# Build KdTree for operations referencing the target mesh
def build_kd_tree(target_mesh:itk.Mesh, bucket_size=16) -> itk.KdTree:
    _, Dimension = itk.template(target_mesh)[1]

    # Set samples from target mesh points
    sample = itk.ListSample[itk.Vector[itk.F, Dimension]].New()

    # Populate ListSample with mesh points
    sample.Resize(target_mesh.GetNumberOfPoints())
    for i in range(0, target_mesh.GetNumberOfPoints()):
        sample.SetMeasurementVector(i, target_mesh.GetPoint(i))

    # Build kd-tree
    tree_generator = itk.KdTreeGenerator[type(sample)].New(Sample=sample, BucketSize=bucket_size)
    tree_generator.Update()
    tree = tree_generator.GetOutput()

    # Iteratively update template points to nearest neighbor in target point set
    return tree, sample


# Deep copy itk.Mesh vertices to avoid updating template in place
def deep_copy_mesh_vertices(mesh:itk.Mesh) -> itk.Mesh:
    mesh_copy = type(mesh).New()

    # Deep copy vertices
    for i in range(mesh.GetNumberOfPoints()):
        mesh_copy.SetPoint(i, mesh.GetPoint(i))

    # Shallow copy cells
    mesh_copy.SetCells(mesh.GetCells())

    return mesh_copy


# Align each template vertex to the closest target vertex
def resample_template_from_target(template_mesh:itk.Mesh, target_mesh:itk.Mesh) -> itk.Mesh:
    # Reference the KdTree to set each template vertex to its nearest neighbor in the target mesh
    _, Dimension = itk.template(template_mesh)[1]
    measurement = itk.Vector[itk.F,Dimension]()

    # Use available wrapping for itk.VectorContainer
    if (itk.ULL, itk.ULL) in itk.VectorContainer.keys():
        neighbors = itk.VectorContainer[itk.ULL, itk.ULL].New()
    else:
        neighbors = itk.VectorContainer[itk.UL, itk.UL].New()

    # Create a KdTree from the target
    tree, _ = build_kd_tree(target_mesh=target_mesh, bucket_size=16)

    template_copy = deep_copy_mesh_vertices(template_mesh)
    for i in range(0, template_mesh.GetNumberOfPoints()):
        tree.Search(template_mesh.GetPoint(i), 1, neighbors)
        template_copy.SetPoint(i, target_mesh.GetPoint(neighbors.GetElement(0)))

    return template_copy


# Resize binary images of equal spacing to occupy the largest common region
def paste_to_common_space(images:list) -> list:
    image_type = type(images[0])
    pixel_type, dimension = itk.template(images[0])[1]

    resized_images = list()

    # Verify spacing is equivalent
    SPACING_TOLERANCE = 1e-7
    assert(all([itk.spacing(images[idx])[dim] - itk.spacing(images[0])[dim] < SPACING_TOLERANCE
                for dim in range(dimension)
                for idx in range(1,len(images))]))

    # Get largest common region
    max_size = itk.size(images[0])
    for image in images:
        max_size = [max(max_size[i], itk.size(image)[i]) for i in range(len(max_size))]

    # Define paste region
    for image in images:
        region = itk.ImageRegion[dimension]()
        region.SetSize(max_size)
        region.SetIndex([0] * dimension)
        new_image = type(images[0]).New(regions=region, spacing=image.GetSpacing())
        new_image.Allocate()

        resized_image = itk.paste_image_filter(source_image=image,
                                               source_region=image.GetLargestPossibleRegion(),
                                               destination_image=new_image,
                                               destination_index=[0] * dimension,
                                               ttype=type(image))
        resized_images.append(resized_image)
    return resized_images


# Downsample images by a given ratio
def downsample_images(images:list, ratio:float) -> list:
    assert(ratio > 1.0)

    downsampled_image_list = list()
    for image in images:
        new_spacing = [spacing * ratio for spacing in itk.spacing(image)]
        new_size = [int(size / ratio) for size in itk.size(image)]
        downsampled_image = itk.resample_image_filter(image,
                                                      size=new_size,
                                                      output_origin=itk.origin(image),
                                                      output_spacing=new_spacing)
        downsampled_image_list.append(downsampled_image)
    return downsampled_image_list


# Generate meshes from binary images
def binary_image_list_to_meshes(images:list, mesh_pixel_type:type=itk.F, object_pixel_value=1) -> list:
    _, dimension = itk.template(images[0])[1]
    mesh_type = itk.Mesh[mesh_pixel_type, dimension]

    mesh_list = list()
    for image in images:
        mesh = itk.binary_mask3_d_mesh_source(input=image,
                                              object_value=object_pixel_value,
                                              ttype=[type(image), mesh_type])
        mesh_list.append(mesh)
    return mesh_list


# Register template mesh to sample mesh
def register_template_to_sample(template_mesh:itk.Mesh,
                                sample_mesh:itk.Mesh,
                                transform=None,
                                learning_rate:float=1.0,
                                max_iterations:int=2000,
                                minimum_convergence_value:float=-1.0,
                                convergence_window_size:int=1,
                                verbose=False) -> itk.Mesh:
    from .pointsetentropyregistrar import PointSetEntropyRegistrar

    registrar = PointSetEntropyRegistrar(verbose=verbose)
    metric = itk.EuclideanDistancePointSetToPointSetMetricv4[itk.PointSet[itk.F,3]].New()

    # Make a deep copy of the template point set to resample from the target
    template_copy = deep_copy_mesh_vertices(template_mesh)

    # Run registration and resample from target
    (transform, deformed_mesh) = registrar.register(template_mesh=template_copy,
                                                    target_mesh=sample_mesh,
                                                    metric=metric,
                                                    transform=transform,
                                                    learning_rate=learning_rate,
                                                    minimum_convergence_value=minimum_convergence_value,
                                                    convergence_window_size=convergence_window_size,
                                                    max_iterations=max_iterations)
    return deformed_mesh


# Align correspondence meshes with Procrustes
def get_mean_correspondence_mesh(template_meshes:list(),
                                 convergence_threshold:float=1.0,
                                 verbose=False) -> itk.Mesh:
    # Verify ITKShape dependency
    assert(hasattr(itk, 'MeshProcrustesAlignFilter'))

    # Verify templates have correspondence points
    assert(all(mesh.GetNumberOfPoints() == template_meshes[0].GetNumberOfPoints()
               for mesh in template_meshes))

    mesh_type = type(template_meshes[0])

    alignment_filter = itk.MeshProcrustesAlignFilter[mesh_type, mesh_type].New(
        convergence=convergence_threshold)
    alignment_filter.SetUseInitialAverageOff()
    alignment_filter.SetUseNormalizationOff()
    alignment_filter.SetUseScalingOff()
    alignment_filter.SetNumberOfInputs(len(template_meshes))
    for idx in range(len(template_meshes)):
        alignment_filter.SetInput(idx, template_meshes[idx])
    alignment_filter.Update()

    if(verbose):
        print(f'Alignment converged at {alignment_filter.GetMeanPointsDifference()}')

    # Mean output contains only vertices so add cells back before returning
    mean_mesh_result = alignment_filter.GetMean()
    mean_mesh_result.SetCells(template_meshes[0].GetCells())
    return mean_mesh_result


# Compute largest correspondence distance between two meshes
def get_pairwise_hausdorff_distance(first_mesh:itk.Mesh, second_mesh:itk.Mesh) -> float:
    def euclidean_distance(pt1:itk.Point, pt2:itk.Point) -> float:
        _, dimension = itk.template(pt1)[1]
        return sum([(pt1[dim] - pt2[dim]) ** 2 for dim in range(dimension)]) ** 0.5

    max_distance = euclidean_distance(first_mesh.GetPoint(0), second_mesh.GetPoint(0))
    for idx in range(1, first_mesh.GetNumberOfPoints()):
        max_distance = max(max_distance,
                           euclidean_distance(first_mesh.GetPoint(idx), second_mesh.GetPoint(idx)))
    return max_distance


# Refine a template by registering it to a target population,
# setting template points to nearest neighbors, and then running Procrustes
# alignment to get the mean of the registered meshes
def refine_template_from_population(template_mesh:itk.Mesh,
                                    target_meshes:list,
                                    registration_iterations=500,
                                    alignment_threshold=0.1,
                                    verbose:bool=False) -> itk.Mesh:
    # Verify ITKShape dependency
    assert(hasattr(itk, 'MeshProcrustesAlignFilter'))

    # Register template to each target mesh in the population
    registered_templates = list()
    for target_mesh in target_meshes:
        registered_template = register_template_to_sample(template_mesh,
                                                          target_mesh,
                                                          max_iterations=registration_iterations,
                                                          verbose=verbose)
        registered_templates.append(registered_template)

    # Deform each registered template to correspond with its respective target
    deformed_templates = list()
    for idx in range(len(target_meshes)):
        deformed_templates.append(resample_template_from_target(registered_templates[idx], target_meshes[idx]))

    # Align templates
    mesh_result = get_mean_correspondence_mesh(deformed_templates,
                                               convergence_threshold=alignment_threshold,
                                               verbose=verbose)

    # Quantify difference from original template
    if(verbose):
        distance = get_pairwise_hausdorff_distance(mesh_result, template_mesh)
        print(f'Got Hausdorff distance of {distance} between refined template and original.')

    return mesh_result
