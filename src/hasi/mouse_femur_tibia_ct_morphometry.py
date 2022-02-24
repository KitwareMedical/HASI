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

# Purpose: Overall segmentation pipeline

import itk
import os
from pathlib import Path
import subprocess
import sys


def sorted_file_list(folder, extension):
    file_list = []
    for filename in os.listdir(folder):
        if filename.endswith(extension):
            filename = os.path.splitext(filename)[0]
            filename = Path(filename).stem
            file_list.append(filename)

    file_list = list(set(file_list))  # remove duplicates
    file_list.sort()
    return file_list


def read_slicer_fiducials(filename):
    file = open(filename, 'r')
    lines = file.readlines()
    lines.pop(0)  # Markups fiducial file version = 4.11

    coordinate_system = lines[0][-4:-1]
    if coordinate_system == 'RAS' or coordinate_system[-1:] == '0':
        ras = True
    elif coordinate_system == 'LPS' or coordinate_system[-1:] == '1':
        ras = False
    elif coordinate_system == 'IJK' or coordinate_system[-1:] == '2':
        raise RuntimeError('Fiducials file with IJK coordinates is not supported')
    else:
        raise RuntimeError('Unrecognized coordinate system: ' + coordinate_system)

    lines.pop(0)  # CoordinateSystem = 0
    lines.pop(0)  # columns = id,x,y,z,ow,ox,oy,oz,vis,sel,lock,label,desc,associatedNodeID

    fiducials = []
    for line in lines:
        e = line.split(',', 4)
        p = itk.Point[itk.D, 3]()
        for i in range(3):
            p[i] = float(e[i + 1])
            if ras and i < 2:
                p[i] = -p[i]
        fiducials.append(p)

    return fiducials


rigid_transform_type = itk.VersorRigid3DTransform[itk.D]

# create an atlas laterality changer transform
atlas_aa_laterality_inverter = itk.Rigid3DTransform.New()
invert_superior_inferior = atlas_aa_laterality_inverter.GetParameters()
# the canonical pose was chosen without regard for proper anatomical orientation
invert_superior_inferior[8] = -1  # so we mirror along SI axis
atlas_aa_laterality_inverter.SetParameters(invert_superior_inferior)


def register_landmarks(atlas_landmarks, input_landmarks):
    transform_type = itk.Transform[itk.D, 3, 3]
    landmark_transformer = itk.LandmarkBasedTransformInitializer[transform_type].New()
    rigid_transform = rigid_transform_type.New()
    landmark_transformer.SetFixedLandmarks(atlas_landmarks)
    landmark_transformer.SetMovingLandmarks(input_landmarks)
    landmark_transformer.SetTransform(rigid_transform)
    landmark_transformer.InitializeTransform()

    # force rotation to be around center of femur head
    rigid_transform.SetCenter(atlas_landmarks[0])
    # and make sure that the other corresponding point maps to it perfectly
    rigid_transform.SetTranslation(input_landmarks[0] - atlas_landmarks[0])

    return rigid_transform


# If label is non-zero, only the specified label participates
# in computation of the bounding box.
# Normally, all non-zero labels contribute to bounding box.
def label_bounding_box(segmentation, label=0):
    if label != 0:
        segmentation = itk.binary_threshold_image_filter(
            segmentation, lower_threshold=label, upper_threshold=label)
    image_mask_spatial_object = itk.ImageMaskSpatialObject[3].New(segmentation)
    bounding_box = image_mask_spatial_object.ComputeMyBoundingBoxInIndexSpace()
    return bounding_box


def process_case(root_dir, bone, case, bone_label, atlas):
    case_base = root_dir + bone + '/' + case + '-' + atlas  # prefix for case file names

    pose = read_slicer_fiducials(root_dir + bone + '/Pose.fcsv')

    case_landmarks = read_slicer_fiducials(root_dir + bone + '/' + case + '.fcsv')
    pose_to_case = register_landmarks(case_landmarks, pose)

    if case[-1] != atlas[-1]:  # last letter of file name is either L or R
        print(f'Changing atlas laterality from {atlas[-1]} to {case[-1]}.')
        # pose_to_case.Compose(atlas_aa_laterality_inverter, True)
        pose_to_case.Compose(atlas_aa_laterality_inverter)
    # we don't need to change laterality of atlas landmarks
    # as they all lie in a plane with K coordinate of zero

    atlas_bone_label_filename = root_dir + bone + '/' + atlas + '-AA-' + bone + '-label.nrrd'
    print(f'Reading {bone} variant of atlas labels from file: {atlas_bone_label_filename}')
    atlas_aa_segmentation = itk.imread(atlas_bone_label_filename)

    atlas_bone_image_filename = root_dir + bone + '/' + atlas + '-AA-' + bone + '.nrrd'
    print(f'Reading {bone} variant of atlas image from file: {atlas_bone_image_filename}')
    atlas_aa_image = itk.imread(atlas_bone_image_filename)

    case_image_filename = root_dir + 'Data/' + case + '.nrrd'
    print(f'Reading case image from file: {case_image_filename}')
    case_image = itk.imread(case_image_filename)

    auto_segmentation_filename = root_dir + 'AutoSegmentations/' + case + '-label.nrrd'
    print(f'Reading case bone segmentation from file: {auto_segmentation_filename}')
    case_auto_segmentation = itk.imread(auto_segmentation_filename)

    print(f'Computing {bone} bounding box')
    case_bounding_box = label_bounding_box(case_auto_segmentation, bone_label)
    case_bone_image = itk.region_of_interest_image_filter(
        case_image,
        region_of_interest=case_bounding_box)
    case_bone_image_filename = root_dir + 'Bones/' + case + '-' + bone + '.nrrd'
    print(f'Writing case bone image to file: {case_bone_image_filename}')
    itk.imwrite(case_bone_image, case_bone_image_filename)

    print('Writing atlas to case transform to file for initializing Elastix registration')
    affine_pose_to_case = itk.AffineTransform[itk.D, 3].New()
    affine_pose_to_case.SetCenter(pose_to_case.GetCenter())
    affine_pose_to_case.SetMatrix(pose_to_case.GetMatrix())
    affine_pose_to_case.SetOffset(pose_to_case.GetOffset())
    atlas_to_case_filename = case_base + '.tfm'
    itk.transformwrite([affine_pose_to_case], atlas_to_case_filename)
    out_elastix_transform = open(atlas_to_case_filename + '.txt', "w")
    out_elastix_transform.writelines(['(Transform "File")\n',
                                      '(TransformFileName "' + case + '-' + atlas + '.tfm")'])
    out_elastix_transform.close()

    # Construct elastix parameter map
    parameter_object = itk.ParameterObject.New()
    resolutions = 4
    parameter_map_rigid = parameter_object.GetDefaultParameterMap('rigid', resolutions)
    parameter_object.AddParameterMap(parameter_map_rigid)
    parameter_map_bspline = parameter_object.GetDefaultParameterMap("bspline", resolutions, 1.0)
    parameter_object.AddParameterMap(parameter_map_bspline)
    parameter_object.SetParameter("DefaultPixelValue", "-1024")
    parameter_object.SetParameter("Metric", "AdvancedMeanSquares")
    # parameter_object.SetParameter("FixedInternalImagePixelType", "short")
    # parameter_object.SetParameter("MovingInternalImagePixelType", "short")
    # we still have to use float pixels

    print('Starting atlas registration')
    registered, elastix_transform = itk.elastix_registration_method(
        case_bone_image.astype(itk.F),  # fixed image is used as primary input to the filter
        moving_image=atlas_aa_image.astype(itk.F),
        # moving_mask=atlas_aa_segmentation,
        parameter_object=parameter_object,
        initial_transform_parameter_file_name=atlas_to_case_filename + '.txt',
        # log_to_console=True,
        output_directory=root_dir + bone + '/',
        log_file_name=case + '-' + atlas + '-elx.log'
    )

    # serialize each parameter map to a file.
    for index in range(elastix_transform.GetNumberOfParameterMaps()):
        parameter_map = elastix_transform.GetParameterMap(index)
        elastix_transform.WriteParameterFile(
            parameter_map,
            case_base + f".{index}.txt")

    registered_filename = case_base + '-reg.nrrd'
    print(f'Writing registered image to file {registered_filename}')
    itk.imwrite(registered.astype(itk.SS), registered_filename)

    print('Running transformix')
    elastix_transform.SetParameter('FinalBSplineInterpolationOrder', '0')
    result_image_transformix = itk.transformix_filter(
        atlas_aa_segmentation.astype(itk.F),  # transformix only works with float pixels
        elastix_transform,
        # reference image?
    )
    result_image = result_image_transformix.astype(itk.UC)
    registered_label_file = case_base + '-label.nrrd'
    print(f'Writing deformed atlas to {registered_label_file}')
    itk.imwrite(result_image, registered_label_file, compression=True)

    print('Computing morphometry features')
    morphometry_filter = itk.BoneMorphometryFeaturesFilter[type(atlas_aa_image)].New(case_bone_image)
    morphometry_filter.SetMaskImage(result_image)
    morphometry_filter.Update()
    print('BVTV', morphometry_filter.GetBVTV())
    print('TbN', morphometry_filter.GetTbN())
    print('TbTh', morphometry_filter.GetTbTh())
    print('TbSp', morphometry_filter.GetTbSp())
    print('BSBV', morphometry_filter.GetBSBV())

    print('Generate the mesh from the segmented case')
    padded_segmentation = itk.constant_pad_image_filter(
        result_image,
        PadUpperBound=1,
        PadLowerBound=1,
        Constant=0
    )

    mesh = itk.cuberille_image_to_mesh_filter(padded_segmentation)
    mesh_filename = case_base + '.vtk'
    print(f'Writing the mesh to file {mesh_filename}')
    itk.meshwrite(mesh, mesh_filename)

    canonical_pose_mesh = itk.transform_mesh_filter(
        mesh,
        transform=pose_to_case  # TODO: we should use the result of Elastix registration here
    )
    canonical_pose_filename = case_base + '.obj'
    print(f'Writing canonical pose mesh to {canonical_pose_filename}')
    itk.meshwrite(canonical_pose_mesh, canonical_pose_filename)


def main_processing(root_dir, bone, atlas, bone_label):
    root_dir = os.path.abspath(root_dir) + '/'
    data_list = sorted_file_list(root_dir + 'Data', '.nrrd')
    if atlas not in data_list:
        raise RuntimeError('Missing data file for the atlas')
    data_list.remove(atlas)

    landmarks_list = sorted_file_list(root_dir + bone, '.fcsv')
    if atlas not in landmarks_list:
        print('Missing landmarks file for the atlas')
    else:
        landmarks_list.remove(atlas)
    if 'Pose' not in landmarks_list:
        raise RuntimeError('Missing Pose.fcsv file')
    landmarks_list.remove('Pose')

    # check if there are any discrepancies
    if data_list != landmarks_list:
        print('There is a discrepancy between data_list and landmarks_list')
        print('data_list:', data_list)
        print('landmarks_list:', landmarks_list)
        sys.exit(1)
    print(f'List of cases to process: {data_list}')

    atlas_image_filename = root_dir + bone + '/' + atlas + '-AA.nrrd'
    print(f'Reading atlas image from file: {atlas_image_filename}')
    atlas_aa_image = itk.imread(atlas_image_filename)
    # atlas_aa_segmentation = itk.imread(root_dir + bone + '/' + atlas + '-AA.seg.nrrd',
    #                                    pixel_type=itk.VariableLengthVector[itk.UC])
    atlas_labels_filename = root_dir + bone + '/' + atlas + '-AA-label.nrrd'
    print(f'Reading atlas labels from file: {atlas_labels_filename}')
    atlas_aa_segmentation = itk.imread(atlas_labels_filename, pixel_type=itk.UC)

    print('Computing bounding box of the labels')
    # reduce the image to a bounding box around the segmented bone
    # the other content makes the registration more difficult
    # because the knees will be bent to different degree etc
    atlas_bounding_box = label_bounding_box(atlas_aa_segmentation)

    atlas_aa_segmentation = itk.region_of_interest_image_filter(
        atlas_aa_segmentation,
        region_of_interest=atlas_bounding_box)
    atlas_bone_label_filename = root_dir + bone + '/' + atlas + '-AA-' + bone + '-label.nrrd'
    print(f'Writing {bone} variant of atlas labels to file: {atlas_bone_label_filename}')
    itk.imwrite(atlas_aa_segmentation, atlas_bone_label_filename, compression=True)

    atlas_aa_image = itk.region_of_interest_image_filter(
        atlas_aa_image,
        region_of_interest=atlas_bounding_box)
    atlas_bone_image_filename = root_dir + bone + '/' + atlas + '-AA-' + bone + '.nrrd'
    print(f'Writing {bone} variant of atlas image to file: {atlas_bone_image_filename}')
    itk.imwrite(atlas_aa_image.astype(itk.SS), atlas_bone_image_filename)

    # now go through all the cases, doing main processing
    for case in data_list:
        print(u'\u2500' * 80)
        print(f'Processing case {case}')

        # process_case(root_dir, bone, case, bone_label, atlas)

        # Elastix crashes on second iteration of this for loop,
        # so we use subprocess as a work-around
        status = subprocess.run(['python', __file__, root_dir, bone, case, str(bone_label), atlas])
        if status.returncode != 0:
            print(f'Case {case} failed with error {status.returncode}')
        else:
            print(f'Success processing case {case}')

    print(f'Processed {len(data_list)} cases for bone {bone} using {atlas} as atlas.\n\n\n')


if __name__ == '__main__':
    if len(sys.argv) == 6:  # this is the subprocess call
        process_case(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), sys.argv[5])
    elif len(sys.argv) == 1:  # direct invocation
        main_processing('../../', 'Femur', '907-L', 1)
        main_processing('../../', 'Femur', '907-R', 1)
        main_processing('../../', 'Tibia', '901-L', 2)
        main_processing('../../', 'Tibia', '901-R', 2)
        # TODO: add for loops here to do this for all available atlases
    else:
        print(f'Invalid number of arguments: {len(sys.argv)}. Invoke the script with no arguments.')
        sys.exit(len(sys.argv))

