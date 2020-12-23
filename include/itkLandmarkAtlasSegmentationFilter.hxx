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
#include "itkResampleImageFilter.h"

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

  using ResampleFilterType = itk::ResampleImageFilter<OutputImageType, OutputImageType, double>;
  typename ResampleFilterType::Pointer resampleFilter = ResampleFilterType::New();
  resampleFilter->SetInput(m_AtlasLabels);
  resampleFilter->SetReferenceImage(input);
  resampleFilter->SetUseReferenceImage(true);
  resampleFilter->SetDefaultPixelValue(0);
  resampleFilter->SetTransform(m_LandmarksTransform);

  // grafting pattern spares us from allocating an intermediate image
  resampleFilter->GraftOutput(this->GetOutput());
  resampleFilter->Update();
  this->GraftOutput(resampleFilter->GetOutput());
}

} // end namespace itk

#endif // itkLandmarkAtlasSegmentationFilter_hxx
