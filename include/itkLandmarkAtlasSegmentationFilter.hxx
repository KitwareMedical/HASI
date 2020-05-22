/*=========================================================================
 *
 *  Copyright NumFOCUS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#ifndef itkLandmarkAtlasSegmentationFilter_hxx
#define itkLandmarkAtlasSegmentationFilter_hxx

#include "itkLandmarkAtlasSegmentationFilter.h"

#include "itkImageRegionIterator.h"
#include "itkImageRegionConstIterator.h"

namespace itk
{
template <typename TInputImage, typename TOutputImage>
void
LandmarkAtlasSegmentationFilter<TInputImage, TOutputImage>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}

template <typename TInputImage, typename TOutputImage>
void
LandmarkAtlasSegmentationFilter<TInputImage, TOutputImage>::GenerateData()
{
  this->AllocateOutputs();

  OutputImageType *      output = this->GetOutput();
  const InputImageType * input = this->GetInput();
  const RegionType &     outputRegion = output->GetRequestedRegion();
  RegionType             inputRegion = RegionType(outputRegion.GetSize());

  using RigidTransformType = itk::VersorRigid3DTransform<double>;
  typename RigidTransformType::Pointer rigidTransform = RigidTransformType::New();

  // std::vector<PointType> inputLandmarks = readSlicerFiducials(inputBase + ".fcsv");
  // std::vector<PointType> atlasLandmarks = readSlicerFiducials(atlasBase + ".fcsv");
  itkAssertOrThrowMacro(m_InputLandmarks.size() == 3, "There must be exactly 3 input landmarks");
  itkAssertOrThrowMacro(m_AtlasLandmarks.size() == 3, "There must be exactly 3 atlas landmarks");

  using LandmarkBasedTransformInitializerType =
    itk::LandmarkBasedTransformInitializer<RigidTransformType, ImageType, ImageType>;
  typename LandmarkBasedTransformInitializerType::Pointer landmarkBasedTransformInitializer =
    LandmarkBasedTransformInitializerType::New();

  landmarkBasedTransformInitializer->SetFixedLandmarks(m_InputLandmarks);
  landmarkBasedTransformInitializer->SetMovingLandmarks(m_AtlasLandmarks);

  rigidTransform->SetIdentity();
  landmarkBasedTransformInitializer->SetTransform(rigidTransform);
  landmarkBasedTransformInitializer->InitializeTransform();

  // force rotation to be around center of femur head
  rigidTransform->SetCenter(inputLandmarks.front());
  // and make sure that the other corresponding point maps to it perfectly
  rigidTransform->SetTranslation(atlasLandmarks.front() - inputLandmarks.front());

  WriteTransform(rigidTransform, outputBase + "-landmarks.tfm");


  typename ImageType::Pointer inputBone1 = ReadImage<ImageType>(inputBase + ".nrrd");
  typename ImageType::Pointer atlasBone1 = ReadImage<ImageType>(atlasBase + ".nrrd");

  typename LabelImageType::Pointer inputLabels = ReadImage<LabelImageType>(inputBase + "-label.nrrd");
  typename LabelImageType::Pointer atlasLabels = ReadImage<LabelImageType>(atlasBase + "-label.nrrd");


  auto perBoneProcessing =
    [](typename ImageType::Pointer bone1, typename LabelImageType::Pointer allLabels, unsigned char howManyLabels) {
      // bone1 and label images might have different extents (bone1 will be a strict subset)
      typename LabelImageType::RegionType bone1Region = bone1->GetBufferedRegion();
      typename ImageType::IndexType       index = bone1Region.GetIndex();
      typename ImageType::PointType       p;
      bone1->TransformIndexToPhysicalPoint(index, p);
      allLabels->TransformPhysicalPointToIndex(p, index);
      itk::Offset<3> indexAdjustment = index - allLabels->GetBufferedRegion().GetIndex();

      typename LabelImageType::Pointer bone1whole = LabelImageType::New();
      bone1whole->CopyInformation(bone1);
      bone1whole->SetRegions(bone1Region);
      bone1whole->Allocate(true);

      // construct whole-bone1 segmentation by ignoring other bones and the split
      // into corical and trabecular bone, and bone marrow (labels 1, 2 and 3)
      itk::MultiThreaderBase::Pointer mt = itk::MultiThreaderBase::New();
      mt->ParallelizeImageRegion<LabelImageType::ImageDimension>(
        bone1Region,
        [bone1whole, allLabels, indexAdjustment, howManyLabels](const typename LabelImageType::RegionType region) {
          typename ImageType::RegionType labelRegion = region;
          labelRegion.SetIndex(labelRegion.GetIndex() + indexAdjustment);

          itk::ImageRegionConstIterator<LabelImageType> iIt(allLabels, labelRegion);
          itk::ImageRegionIterator<LabelImageType>      oIt(bone1whole, region);
          for (; !oIt.IsAtEnd(); ++iIt, ++oIt)
          {
            auto label = iIt.Get();
            if (label >= 1 && label <= howManyLabels)
            {
              oIt.Set(1);
            }
          }
        },
        nullptr);

      using RealImageType = itk::Image<float, 3>;
      using DistanceFieldType = itk::SignedMaurerDistanceMapImageFilter<LabelImageType, RealImageType>;
      typename DistanceFieldType::Pointer distF = DistanceFieldType::New();
      distF->SetInput(bone1whole);
      distF->SetSquaredDistance(false);
      distF->SetInsideIsPositive(true);
      distF->Update();
      typename RealImageType::Pointer distanceField = distF->GetOutput();
      distanceField->DisconnectPipeline();
      bone1whole = nullptr; // deallocate it

      mt->ParallelizeImageRegion<3>(
        bone1Region,
        [bone1, distanceField](const typename ImageType::RegionType region) {
          itk::ImageRegionConstIterator<RealImageType> iIt(distanceField, region);
          itk::ImageRegionIterator<ImageType>          oIt(bone1, region);
          for (; !oIt.IsAtEnd(); ++iIt, ++oIt)
          {
            float dist = iIt.Get();
            // set pixels outside the bone to scaled distance to bone
            // this should prevent trabecular texture from throwing registration off track
            if (dist < 0)
            {
              oIt.Set(dist * 1024);
            }
          }
        },
        nullptr);

      return distanceField;
    };


  typename RealImageType::Pointer inputDF1 = perBoneProcessing(inputBone1, inputLabels, 3); // just the first bone
  WriteImage(inputBone1, outputBase + "-bone1i.nrrd", false);
  typename RealImageType::Pointer atlasDF1 = perBoneProcessing(atlasBone1, atlasLabels, 255); // keep all atlas labels!
  WriteImage(atlasBone1, outputBase + "-bone1a.nrrd", false);

  inputLabels = nullptr; // deallocate it, as we want to make a better version of this


  using AffineTransformType = itk::AffineTransform<double, Dimension>;
  constexpr unsigned int SplineOrder = 3;
  using CoordinateRepType = double;
  using DeformableTransformType = itk::BSplineTransform<CoordinateRepType, Dimension, SplineOrder>;
  using OptimizerType = itk::RegularStepGradientDescentOptimizer;
  using RealMetricType = itk::MeanSquaresImageToImageMetric<RealImageType, RealImageType>;
  using RealInterpolatorType = itk::LinearInterpolateImageFunction<RealImageType, double>;
  using RealRegistrationType = itk::ImageRegistrationMethod<RealImageType, RealImageType>;

  typename RealMetricType::Pointer metric1 = RealMetricType::New();
  metric1->ReinitializeSeed(76926294);
  typename OptimizerType::Pointer        optimizer = OptimizerType::New();
  typename RealInterpolatorType::Pointer interpolator1 = RealInterpolatorType::New();
  typename RealRegistrationType::Pointer registration1 = RealRegistrationType::New();

  registration1->SetMetric(metric1);
  registration1->SetOptimizer(optimizer);
  registration1->SetInterpolator(interpolator1);
  registration1->SetFixedImage(inputDF1);
  registration1->SetMovingImage(atlasDF1);

  // Auxiliary identity transform.
  using IdentityTransformType = itk::IdentityTransform<double, Dimension>;
  IdentityTransformType::Pointer identityTransform = IdentityTransformType::New();

  ImageType::RegionType fixedRegion = inputBone1->GetBufferedRegion();
  registration1->SetFixedImageRegion(fixedRegion);
  registration1->SetInitialTransformParameters(rigidTransform->GetParameters());
  registration1->SetTransform(rigidTransform);


  //  Define optimizer normalization to compensate for different dynamic range
  //  of rotations and translations.
  double avgSpacing = 1.0;
  for (unsigned d = 0; d < Dimension; d++)
  {
    avgSpacing *= inputBone1->GetSpacing()[d];
  }
  avgSpacing = std::pow(avgSpacing, 1 / 3.0); // geometric mean has equivalent voxel volume

  using OptimizerScalesType = OptimizerType::ScalesType;
  OptimizerScalesType optimizerScales(rigidTransform->GetNumberOfParameters());
  const double        translationScale = 1.0 / (1000.0 * avgSpacing);

  optimizerScales[0] = 1.0;
  optimizerScales[1] = 1.0;
  optimizerScales[2] = 1.0;
  optimizerScales[3] = translationScale;
  optimizerScales[4] = translationScale;
  optimizerScales[5] = translationScale;

  optimizer->SetScales(optimizerScales);

  optimizer->SetMaximumStepLength(0.2000);
  optimizer->SetMinimumStepLength(0.0001);

  optimizer->SetNumberOfIterations(200);

  // The rigid transform has 6 parameters we use therefore a few samples to run
  // this stage.
  //
  // Regulating the number of samples in the Metric is equivalent to performing
  // multi-resolution registration because it is indeed a sub-sampling of the
  // image.
  metric1->SetNumberOfSpatialSamples(100000L);
  // metric1->SetUseFixedImageSamplesIntensityThreshold(-1000);

  class CommandIterationUpdate : public itk::Command
  {
  public:
    using Self = CommandIterationUpdate;
    using Superclass = itk::Command;
    using Pointer = itk::SmartPointer<Self>;
    itkNewMacro(Self);

  protected:
    CommandIterationUpdate() = default;

  public:
    using OptimizerType = itk::RegularStepGradientDescentOptimizer;
    using OptimizerPointer = const OptimizerType *;

    void
    Execute(itk::Object * caller, const itk::EventObject & event) override
    {
      Execute((const itk::Object *)caller, event);
    }

    void
    Execute(const itk::Object * object, const itk::EventObject & event) override
    {
      auto optimizer = static_cast<OptimizerPointer>(object);
      if (!(itk::IterationEvent().CheckEvent(&event)))
      {
        return;
      }
      std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
      std::cout << diff.count() << "  " << optimizer->GetCurrentIteration() << "  ";
      std::cout << optimizer->GetValue() << std::endl;
    }
  };

  // Create the Command observer and register it with the optimizer.
  CommandIterationUpdate::Pointer observer = CommandIterationUpdate::New();
  optimizer->AddObserver(itk::IterationEvent(), observer);

  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Starting Rigid Registration " << std::endl;
  registration1->Update();
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Rigid Registration completed" << std::endl;
  std::cout << "Stop condition = " << registration1->GetOptimizer()->GetStopConditionDescription() << std::endl;
  rigidTransform->SetParameters(registration1->GetLastTransformParameters());
  WriteTransform(rigidTransform, outputBase + "-rigid.tfm");


  //  Perform Affine Registration
  AffineTransformType::Pointer affineTransform = AffineTransformType::New();
  affineTransform->SetCenter(rigidTransform->GetCenter());
  affineTransform->SetTranslation(rigidTransform->GetTranslation());
  affineTransform->SetMatrix(rigidTransform->GetMatrix());
  // WriteTransform(affineTransform, outputBase + "-affineInit.tfm"); // debug

  registration1->SetTransform(affineTransform);
  registration1->SetInitialTransformParameters(affineTransform->GetParameters());

  optimizerScales = OptimizerScalesType(affineTransform->GetNumberOfParameters());
  optimizerScales[0] = 1.0;
  optimizerScales[1] = 1.0;
  optimizerScales[2] = 1.0;
  optimizerScales[3] = 1.0;
  optimizerScales[4] = 1.0;
  optimizerScales[5] = 1.0;
  optimizerScales[6] = 1.0;
  optimizerScales[7] = 1.0;
  optimizerScales[8] = 1.0;

  optimizerScales[9] = translationScale;
  optimizerScales[10] = translationScale;
  optimizerScales[11] = translationScale;

  optimizer->SetScales(optimizerScales);
  optimizer->SetMaximumStepLength(0.2000);
  optimizer->SetMinimumStepLength(0.0001);
  optimizer->SetNumberOfIterations(200);

  // The Affine transform has 12 parameters we use therefore a more samples to run
  // this stage.
  //
  // Regulating the number of samples in the Metric is equivalent to performing
  // multi-resolution registration because it is indeed a sub-sampling of the
  // image.
  metric1->SetNumberOfSpatialSamples(500000L);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Starting Affine Registration" << std::endl;
  registration1->Update();
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Affine Registration completed" << std::endl;
  affineTransform->SetParameters(registration1->GetLastTransformParameters());
  WriteTransform(affineTransform, outputBase + "-affine.tfm");

  inputDF1 = nullptr; // deallocate it
  atlasDF1 = nullptr; // deallocate it

  //  Perform Deformable Registration
  using CompositeTransformType = itk::CompositeTransform<>;

  typename DeformableTransformType::Pointer bsplineTransformCoarse = DeformableTransformType::New();
  typename CompositeTransformType::Pointer  compositeTransform = CompositeTransformType::New();
  compositeTransform->AddTransform(affineTransform);
  compositeTransform->AddTransform(bsplineTransformCoarse);
  compositeTransform->SetOnlyMostRecentTransformToOptimizeOn();

  unsigned int numberOfGridNodesInOneDimensionCoarse = 5;

  typename DeformableTransformType::PhysicalDimensionsType fixedPhysicalDimensions;
  typename DeformableTransformType::MeshSizeType           meshSize;
  typename DeformableTransformType::OriginType             fixedOrigin;

  for (unsigned int i = 0; i < Dimension; i++)
  {
    fixedOrigin[i] = inputBone1->GetOrigin()[i];
    fixedPhysicalDimensions[i] = inputBone1->GetSpacing()[i] * static_cast<double>(fixedRegion.GetSize()[i] - 1);
  }
  meshSize.Fill(numberOfGridNodesInOneDimensionCoarse - SplineOrder);

  bsplineTransformCoarse->SetTransformDomainOrigin(fixedOrigin);
  bsplineTransformCoarse->SetTransformDomainPhysicalDimensions(fixedPhysicalDimensions);
  bsplineTransformCoarse->SetTransformDomainMeshSize(meshSize);
  bsplineTransformCoarse->SetTransformDomainDirection(inputBone1->GetDirection());

  using ParametersType = DeformableTransformType::ParametersType;

  unsigned int numberOfBSplineParameters = bsplineTransformCoarse->GetNumberOfParameters();

  optimizerScales = OptimizerScalesType(numberOfBSplineParameters);
  optimizerScales.Fill(1.0);
  optimizer->SetScales(optimizerScales);

  ParametersType initialDeformableTransformParameters(numberOfBSplineParameters);
  initialDeformableTransformParameters.Fill(0.0);
  bsplineTransformCoarse->SetParameters(initialDeformableTransformParameters);

  // for deformable registration part, we want actual bone intensities
  using MetricType = itk::MeanSquaresImageToImageMetric<ImageType, ImageType>;
  typename MetricType::Pointer metric2 = MetricType::New();
  metric2->ReinitializeSeed(76926294);

  using InterpolatorType = itk::LinearInterpolateImageFunction<ImageType, double>;
  typename InterpolatorType::Pointer interpolator2 = InterpolatorType::New();
  using RegistrationType = itk::ImageRegistrationMethod<ImageType, ImageType>;
  typename RegistrationType::Pointer registration2 = RegistrationType::New();
  registration2->SetMetric(metric2);
  registration2->SetOptimizer(optimizer);
  registration2->SetInterpolator(interpolator2);
  registration2->SetInitialTransformParameters(compositeTransform->GetParameters());
  registration2->SetTransform(compositeTransform);
  registration2->SetFixedImageRegion(fixedRegion);
  registration2->SetFixedImage(inputBone1);
  registration2->SetMovingImage(atlasBone1);

  optimizer->SetMaximumStepLength(10.0);
  optimizer->SetMinimumStepLength(0.01);
  optimizer->SetRelaxationFactor(0.7);
  optimizer->SetNumberOfIterations(20);

  // The BSpline transform has a large number of parameters, we use therefore a
  // much larger number of samples to run this stage.
  //
  // Regulating the number of samples in the Metric is equivalent to performing
  // multi-resolution registration because it is indeed a sub-sampling of the
  // image.
  metric2->SetNumberOfSpatialSamples(numberOfBSplineParameters * 1000);
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Starting BSpline Deformable Registration" << std::endl;
  registration2->Update();
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " BSpline Deformable Registration completed" << std::endl;
  OptimizerType::ParametersType finalParameters = registration2->GetLastTransformParameters();
  compositeTransform->SetParameters(finalParameters);
  WriteTransform(compositeTransform, outputBase + "-BSpline.tfm");


  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Resampling the atlas into the space of input image" << std::endl;
  using ResampleFilterType = itk::ResampleImageFilter<LabelImageType, LabelImageType, double>;
  ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();
  resampleFilter->SetInput(atlasLabels);
  resampleFilter->SetTransform(affineTransform);
  resampleFilter->SetReferenceImage(inputBone1);
  resampleFilter->SetUseReferenceImage(true);
  resampleFilter->SetDefaultPixelValue(0);
  resampleFilter->Update();
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Affine Resampling complete!" << std::endl;
  typename LabelImageType::Pointer segmentedImage = resampleFilter->GetOutput();
  WriteImage(segmentedImage, outputBase + "-A-label.nrrd", true);

  resampleFilter->SetTransform(compositeTransform);
  resampleFilter->Update();
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " BSpline Resampling complete!" << std::endl;
  segmentedImage = resampleFilter->GetOutput();
  WriteImage(segmentedImage, outputBase + "-BS-label.nrrd", true);
}

} // end namespace itk

#endif // itkLandmarkAtlasSegmentationFilter_hxx
