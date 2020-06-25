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

#include "itkLandmarkBasedTransformInitializer.h"
#include "itkSignedMaurerDistanceMapImageFilter.h"
#include "itkRegionOfInterestImageFilter.h"
#include "itkImageRegistrationMethod.h"
#include "itkRegularStepGradientDescentOptimizer.h"
#include "itkMeanSquaresImageToImageMetric.h"
#include "itkResampleImageFilter.h"
#include "itkBinaryFillholeImageFilter.h"


#include "itkTransformFileWriter.h"
#include "itkImageFileWriter.h"

std::string outputBase = ".";


namespace itk
{
template <typename TInputImage, typename TOutputImage>
void
LandmarkAtlasSegmentationFilter<TInputImage, TOutputImage>::PrintSelf(std::ostream & os, Indent indent) const
{
  Superclass::PrintSelf(os, indent);
}

template <typename TInputImage, typename TOutputImage>
template <typename TImage>
typename TImage::Pointer
LandmarkAtlasSegmentationFilter<TInputImage, TOutputImage>::Duplicate(const TImage * input, const RegionType subRegion)
{
  using DuplicatorType = itk::RegionOfInterestImageFilter<TImage, TImage>;
  typename DuplicatorType::Pointer roi = DuplicatorType::New();
  roi->SetInput(input);
  roi->SetRegionOfInterest(subRegion);
  roi->Update();
  return roi->GetOutput();
}

template <typename TInputImage, typename TOutputImage>
void
LandmarkAtlasSegmentationFilter<TInputImage, TOutputImage>::AffineFromRigid()
{
  m_AffineTransform = AffineTransformType::New();
  m_AffineTransform->SetCenter(m_RigidTransform->GetCenter());
  m_AffineTransform->SetTranslation(m_RigidTransform->GetTranslation());
  m_AffineTransform->SetMatrix(m_RigidTransform->GetMatrix());
  if (this->GetDebug())
  {
    WriteTransform(m_AffineTransform, outputBase + "-affineInit.tfm"); // debug
  }
}

template <typename TImage>
void
WriteImage(TImage * out, std::string filename, bool compress)
{
  using WriterType = itk::ImageFileWriter<TImage>;
  typename WriterType::Pointer w = WriterType::New();
  w->SetInput(out);
  w->SetFileName(filename);
  w->SetUseCompression(compress);
  w->Update();
}

void
WriteTransform(const itk::Object * transform, std::string fileName)
{
  using TransformWriterType = itk::TransformFileWriterTemplate<double>;
  typename TransformWriterType::Pointer transformWriter = TransformWriterType::New();
  transformWriter->SetInput(transform);
  transformWriter->SetFileName(fileName);
  transformWriter->Update();
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


  m_LandmarksTransform = RigidTransformType::New();

  itkAssertOrThrowMacro(m_InputLandmarks.size() == 3, "There must be exactly 3 input landmarks");
  itkAssertOrThrowMacro(m_AtlasLandmarks.size() == 3, "There must be exactly 3 atlas landmarks");

  using LandmarkBasedTransformInitializerType =
    itk::LandmarkBasedTransformInitializer<RigidTransformType, InputImageType, InputImageType>;
  typename LandmarkBasedTransformInitializerType::Pointer landmarkBasedTransformInitializer =
    LandmarkBasedTransformInitializerType::New();

  landmarkBasedTransformInitializer->SetFixedLandmarks(m_InputLandmarks);
  landmarkBasedTransformInitializer->SetMovingLandmarks(m_AtlasLandmarks);

  m_LandmarksTransform->SetIdentity();
  landmarkBasedTransformInitializer->SetTransform(m_LandmarksTransform);
  landmarkBasedTransformInitializer->InitializeTransform();

  // force rotation to be around center of femur head
  m_LandmarksTransform->SetCenter(m_InputLandmarks.front());
  // and make sure that the other corresponding point maps to it perfectly
  m_LandmarksTransform->SetTranslation(m_AtlasLandmarks.front() - m_InputLandmarks.front());

  if (this->GetDebug())
  {
    WriteTransform(m_LandmarksTransform, outputBase + "-landmarks.tfm");
  }


  typename InputImageType::Pointer inputBone1 = this->Duplicate(this->GetInput(0), inputRegion);
  typename InputImageType::Pointer atlasBone1 = this->Duplicate(this->GetInput(1), inputRegion);

  auto perBoneProcessing = [](typename InputImageType::Pointer       bone1,
                              typename OutputImageType::Pointer      allLabels,
                              unsigned char                          howManyLabels,
                              typename OutputImageType::RegionType & contentRegion,
                              bool                                   fillHoles) {
    contentRegion = bone1->GetBufferedRegion();
    typename InputImageType::PointType p;
    // bone1 and label images might have different extents (bone1 will be a strict subset)
    using IndexType = typename InputImageType::IndexType;
    IndexType index = contentRegion.GetIndex();
    bone1->TransformIndexToPhysicalPoint(index, p);
    allLabels->TransformPhysicalPointToIndex(p, index);
    itk::Offset<3> indexAdjustment = index - allLabels->GetBufferedRegion().GetIndex();

    typename OutputImageType::Pointer bone1whole = OutputImageType::New();
    bone1whole->CopyInformation(bone1);
    bone1whole->SetRegions(contentRegion);
    bone1whole->Allocate(true);

    IndexType  minInd{ static_cast<IndexValueType>(contentRegion.GetSize()[0]) + index[0],
                      static_cast<IndexValueType>(contentRegion.GetSize()[1]) + index[1],
                      static_cast<IndexValueType>(contentRegion.GetSize()[2]) + index[2] };
    IndexType  maxInd = index;
    std::mutex minMaxMutex;

    // construct whole-bone1 segmentation by ignoring other bones and the split
    // into corical and trabecular bone, and bone marrow (labels 1, 2 and 3)
    itk::MultiThreaderBase::Pointer mt = itk::MultiThreaderBase::New();
    mt->ParallelizeImageRegion<OutputImageType::ImageDimension>(
      contentRegion,
      [bone1whole, allLabels, indexAdjustment, howManyLabels, &minInd, &maxInd, &minMaxMutex, contentRegion](
        const typename OutputImageType::RegionType region) {
        typename InputImageType::RegionType labelRegion = region;
        labelRegion.SetIndex(labelRegion.GetIndex() + indexAdjustment);

        IndexType minIndThread{ static_cast<IndexValueType>(contentRegion.GetSize()[0]) + contentRegion.GetIndex()[0],
                                static_cast<IndexValueType>(contentRegion.GetSize()[1]) + contentRegion.GetIndex()[1],
                                static_cast<IndexValueType>(contentRegion.GetSize()[2]) + contentRegion.GetIndex()[2] };
        IndexType maxIndThread = contentRegion.GetIndex();

        auto updateMinMax = [](IndexType & minIndex, IndexType & maxIndex, IndexType newIndex) {
          for (unsigned d = 0; d < Dimension; d++)
          {
            minIndex[d] = std::min(newIndex[d], minIndex[d]);
            maxIndex[d] = std::max(newIndex[d], maxIndex[d]);
          }
        };

        itk::ImageRegionConstIterator<OutputImageType>     iIt(allLabels, labelRegion);
        itk::ImageRegionIteratorWithIndex<OutputImageType> oIt(bone1whole, region);
        for (; !oIt.IsAtEnd(); ++iIt, ++oIt)
        {
          auto label = iIt.Get();
          if (label >= 1 && label <= howManyLabels)
          {
            oIt.Set(1);
            updateMinMax(minIndThread, maxIndThread, oIt.GetIndex());
          }
        }

        if (minIndThread[0] <= maxIndThread[0]) // bounds valid, update outer min/max
        {
          std::lock_guard<std::mutex> lock(minMaxMutex);
          updateMinMax(minInd, maxInd, minIndThread);
          updateMinMax(minInd, maxInd, maxIndThread);
        }
      },
      nullptr);

    contentRegion.SetIndex(minInd);
    for (unsigned d = 0; d < Dimension; d++)
    {
      contentRegion.SetSize(d, maxInd[d] - minInd[d]);
    }
    WriteImage(bone1whole.GetPointer(), outputBase + "-bone1-label.nrrd", true);

    if (fillHoles)
    {
      using HoleFillType = itk::BinaryFillholeImageFilter<OutputImageType>;
      typename HoleFillType::Pointer holeFiller = HoleFillType::New();
      holeFiller->SetInput(bone1whole);
      holeFiller->SetForegroundValue(1);
      // holeFiller->SetFullyConnected(false); // this is the default
      holeFiller->Update();
      bone1whole = holeFiller->GetOutput();
      bone1whole->DisconnectPipeline();
      WriteImage(bone1whole.GetPointer(), outputBase + "-bone1filled-label.nrrd", true);
    }

    using RealImageType = itk::Image<float, 3>;
    using DistanceFieldType = itk::SignedMaurerDistanceMapImageFilter<OutputImageType, RealImageType>;
    typename DistanceFieldType::Pointer distF = DistanceFieldType::New();
    distF->SetInput(bone1whole);
    distF->SetSquaredDistance(false);
    distF->SetInsideIsPositive(true);
    // distF->GetOutput()->SetRequestedRegion(contentRegion);
    distF->Update();
    typename RealImageType::Pointer distanceField = distF->GetOutput();
    distanceField->DisconnectPipeline();
    WriteImage(distanceField.GetPointer(), outputBase + "-bone1DF.nrrd", false);
    bone1whole = nullptr; // deallocate it

    mt->ParallelizeImageRegion<3>(
      bone1->GetBufferedRegion(),
      [bone1, distanceField](const typename InputImageType::RegionType region) {
        itk::ImageRegionConstIterator<RealImageType> iIt(distanceField, region);
        itk::ImageRegionIterator<InputImageType>     oIt(bone1, region);
        for (; !oIt.IsAtEnd(); ++iIt, ++oIt)
        {
          float dist = iIt.Get();
          // set pixels outside the bone to scaled distance to bone
          // this should prevent background from throwing registration off track
          if (dist < 0)
          {
            oIt.Set(dist * 1024);
          }
        }
      },
      nullptr);

    return distanceField;
  };

  typename RealImageType::RegionType bone1Region;
  typename RealImageType::Pointer    inputDF1 =
    perBoneProcessing(inputBone1, m_InputLabels, 3, bone1Region, false); // just the first bone
  WriteImage(inputDF1.GetPointer(), outputBase + "-inputDF1.nrrd", false);

  if (this->GetDebug())
  {
    WriteImage(inputBone1.GetPointer(), outputBase + "-bone1i.nrrd", false);
  }
  inputDF1 = this->Duplicate<RealImageType>(inputDF1, bone1Region);
  WriteImage(inputDF1.GetPointer(), outputBase + "-inputDF1.nrrd", false);

  typename RealImageType::RegionType atlasRegion;
  typename RealImageType::Pointer    atlasDF1 = perBoneProcessing(
    atlasBone1, m_AtlasLabels, 255, atlasRegion, true); // keep all atlas labels and fill possible holes
  WriteImage(atlasDF1.GetPointer(), outputBase + "-atlasDF1.nrrd", false);
  if (this->GetDebug())
  {
    WriteImage(atlasBone1.GetPointer(), outputBase + "-bone1a.nrrd", false);
  }
  atlasDF1 = this->Duplicate<RealImageType>(atlasDF1, atlasRegion);
  WriteImage(atlasDF1.GetPointer(), outputBase + "-atlasDF1.nrrd", false);


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
  registration1->SetFixedImageRegion(bone1Region);
  registration1->SetMovingImage(atlasDF1);

  // Auxiliary identity transform.
  using IdentityTransformType = typename itk::IdentityTransform<double, Dimension>;
  typename IdentityTransformType::Pointer identityTransform = IdentityTransformType::New();

  // registration1->SetFixedImageRegion(bone1Region);
  registration1->SetInitialTransformParameters(m_LandmarksTransform->GetParameters());
  registration1->SetTransform(m_LandmarksTransform);

  //  Define optimizer normalization to compensate for different dynamic range
  //  of rotations and translations.
  double avgSpacing = 1.0;
  for (unsigned d = 0; d < Dimension; d++)
  {
    avgSpacing *= inputBone1->GetSpacing()[d];
  }
  avgSpacing = std::pow(avgSpacing, 1 / 3.0); // geometric mean has equivalent voxel volume

  using OptimizerScalesType = OptimizerType::ScalesType;
  OptimizerScalesType optimizerScales(m_LandmarksTransform->GetNumberOfParameters());
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
      std::cout << optimizer->GetCurrentIteration() << ": " << optimizer->GetValue() << "\n";
    }
  };

  // Create the Command observer and register it with the optimizer.
  typename CommandIterationUpdate::Pointer observer = CommandIterationUpdate::New();
  optimizer->AddObserver(itk::IterationEvent(), observer);


  if (false) // debugging
  {
    m_RigidTransform = m_LandmarksTransform;
    this->AffineFromRigid();
  }
  else
  {
    registration1->Initialize();
    std::cout << "m_LandmarksTransform: " << metric1->GetValue(m_LandmarksTransform->GetParameters()) << std::endl;
    registration1->Update();
    if (this->GetDebug())
    {
      std::cout << "Stop condition = " << registration1->GetOptimizer()->GetStopConditionDescription() << std::endl;
    }
    m_RigidTransform = RigidTransformType::New();
    m_RigidTransform->SetParameters(registration1->GetLastTransformParameters());
    if (this->GetDebug())
    {
      WriteTransform(m_RigidTransform, outputBase + "-rigid.tfm");
    }
    std::cout << "m_RigidTransform: " << metric1->GetValue(m_RigidTransform->GetParameters()) << std::endl;


    //  Perform Affine Registration
    this->AffineFromRigid();
    registration1->SetTransform(m_AffineTransform);
    registration1->SetInitialTransformParameters(m_AffineTransform->GetParameters());

    optimizerScales = OptimizerScalesType(m_AffineTransform->GetNumberOfParameters());
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

    // The Affine transform has 12 parameters so we use more samples to run this stage.
    metric1->SetNumberOfSpatialSamples(500000L);

    registration1->Initialize();
    std::cout << "m_AffineTransform: " << metric1->GetValue(m_AffineTransform->GetParameters()) << std::endl;

    std::cout << " Starting Affine Registration" << std::endl;
    registration1->Update();
    std::cout << " Affine Registration completed" << std::endl;
    m_AffineTransform->SetParameters(registration1->GetLastTransformParameters());
    if (this->GetDebug())
    {
      WriteTransform(m_AffineTransform, outputBase + "-affine.tfm");
    }
  }

  inputDF1 = nullptr; // deallocate it
  atlasDF1 = nullptr; // deallocate it

  using ResampleFilterType = itk::ResampleImageFilter<OutputImageType, OutputImageType, double>;
  typename ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();
  resampleFilter->SetInput(m_AtlasLabels);
  resampleFilter->SetReferenceImage(inputBone1);
  resampleFilter->SetUseReferenceImage(true);
  resampleFilter->SetDefaultPixelValue(0);

  m_FinalTransform = CompositeTransformType::New();
  m_FinalTransform->AddTransform(m_AffineTransform);

  if (!m_StopAtAffine)
  {
    //  Perform Deformable Registration
    typename DeformableTransformType::Pointer bsplineTransformCoarse = DeformableTransformType::New();
    m_FinalTransform->AddTransform(bsplineTransformCoarse);
    m_FinalTransform->SetOnlyMostRecentTransformToOptimizeOn();

    unsigned int numberOfGridNodesInOneDimensionCoarse = 5;

    typename DeformableTransformType::PhysicalDimensionsType fixedPhysicalDimensions;
    typename DeformableTransformType::MeshSizeType           meshSize;
    typename DeformableTransformType::OriginType             fixedOrigin;

    for (unsigned int i = 0; i < Dimension; i++)
    {
      fixedOrigin[i] = inputBone1->GetOrigin()[i];
      fixedPhysicalDimensions[i] = inputBone1->GetSpacing()[i] * static_cast<double>(bone1Region.GetSize()[i] - 1);
    }
    meshSize.Fill(numberOfGridNodesInOneDimensionCoarse - SplineOrder);

    bsplineTransformCoarse->SetTransformDomainOrigin(fixedOrigin);
    bsplineTransformCoarse->SetTransformDomainPhysicalDimensions(fixedPhysicalDimensions);
    bsplineTransformCoarse->SetTransformDomainMeshSize(meshSize);
    bsplineTransformCoarse->SetTransformDomainDirection(inputBone1->GetDirection());

    using ParametersType = typename DeformableTransformType::ParametersType;

    unsigned int numberOfBSplineParameters = bsplineTransformCoarse->GetNumberOfParameters();

    optimizerScales = OptimizerScalesType(numberOfBSplineParameters);
    optimizerScales.Fill(1.0);
    optimizer->SetScales(optimizerScales);

    ParametersType initialDeformableTransformParameters(numberOfBSplineParameters);
    initialDeformableTransformParameters.Fill(0.0);
    bsplineTransformCoarse->SetParameters(initialDeformableTransformParameters);

    // for deformable registration part, we want actual bone intensities
    using MetricType = itk::MeanSquaresImageToImageMetric<InputImageType, InputImageType>;
    typename MetricType::Pointer metric2 = MetricType::New();
    metric2->ReinitializeSeed(76926294);

    using InterpolatorType = itk::LinearInterpolateImageFunction<InputImageType, double>;
    typename InterpolatorType::Pointer interpolator2 = InterpolatorType::New();
    using RegistrationType = itk::ImageRegistrationMethod<InputImageType, InputImageType>;
    typename RegistrationType::Pointer registration2 = RegistrationType::New();
    registration2->SetMetric(metric2);
    registration2->SetOptimizer(optimizer);
    registration2->SetInterpolator(interpolator2);
    registration2->SetInitialTransformParameters(m_FinalTransform->GetParameters());
    registration2->SetTransform(m_FinalTransform);
    // registration2->SetFixedImageRegion(bone1Region);
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
    std::cout << " Starting BSpline Deformable Registration" << std::endl;
    registration2->Update();
    std::cout << " BSpline Deformable Registration completed" << std::endl;
    OptimizerType::ParametersType finalParameters = registration2->GetLastTransformParameters();
    m_FinalTransform->SetParameters(finalParameters);
    if (this->GetDebug())
    {
      WriteTransform(m_FinalTransform, outputBase + "-BSpline.tfm");
    }
  }

  resampleFilter->SetTransform(m_FinalTransform);

  // grafting pattern spares us from allocating an intermediate image
  resampleFilter->GraftOutput(this->GetOutput());
  resampleFilter->Update();
  this->GraftOutput(resampleFilter->GetOutput());
  if (this->GetDebug())
  {
    WriteImage(resampleFilter->GetOutput(), outputBase + "-label.nrrd", true);
  }
}

} // end namespace itk

#endif // itkLandmarkAtlasSegmentationFilter_hxx
