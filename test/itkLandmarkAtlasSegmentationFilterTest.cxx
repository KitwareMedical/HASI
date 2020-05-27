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

#include "itkLandmarkAtlasSegmentationFilter.h"

#include "itkCommand.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkTestingMacros.h"
#include "itkTransformFileWriter.h"

namespace
{
class ShowProgress : public itk::Command
{
public:
  itkNewMacro(ShowProgress);

  void
  Execute(itk::Object * caller, const itk::EventObject & event) override
  {
    Execute((const itk::Object *)caller, event);
  }

  void
  Execute(const itk::Object * caller, const itk::EventObject & event) override
  {
    if (!itk::ProgressEvent().CheckEvent(&event))
    {
      return;
    }
    const auto * processObject = dynamic_cast<const itk::ProcessObject *>(caller);
    if (!processObject)
    {
      return;
    }
    std::cout << " " << processObject->GetProgress();
  }
};

std::vector<itk::Point<double, 3>>
readSlicerFiducials(std::string fileName)
{
  using PointType = itk::Point<double, 3>;
  std::ifstream pointsFile(fileName.c_str());
  std::string   line;
  // ignore first 3 lines (comments of fiducials savefile)
  std::getline(pointsFile, line); //# Markups fiducial file version = 4.10
  std::getline(pointsFile, line); //# CoordinateSystem = 0
  line = line.substr(line.length() - 3);
  bool ras = false;
  if (line == "RAS" || line[line.length() - 1] == '0')
    ras = true;
  else if (line == "LPS" || line[line.length() - 1] == '1')
    ; // LPS, great
  else if (line == "IJK" || line[line.length() - 1] == '2')
    throw itk::ExceptionObject(
      __FILE__, __LINE__, "Fiducials file with IJK coordinates is not supported", __FUNCTION__);
  else
    throw itk::ExceptionObject(__FILE__, __LINE__, "Unrecognized coordinate system", __FUNCTION__);
  std::getline(pointsFile, line); //# columns = id,x,y,z,ow,ox,oy,oz,vis,sel,lock,label,desc,associatedNodeID

  std::vector<PointType> points;
  std::getline(pointsFile, line);
  while (!pointsFile.eof())
  {
    if (!pointsFile.good())
      break;
    PointType         p;
    std::stringstream iss(line);

    std::string val;
    std::getline(iss, val, ','); // ignore ID
    for (int col = 0; col < 3; ++col)
    {
      std::getline(iss, val, ',');
      if (!iss.good())
        break;

      std::stringstream convertor(val);
      convertor >> p[col];

      if (ras && col < 2)
        p[col] *= -1;
    }
    points.push_back(p);
    std::getline(pointsFile, line);
  }
  return points;
}

template <typename TImage>
itk::SmartPointer<TImage>
ReadImage(std::string filename)
{
  using ReaderType = itk::ImageFileReader<TImage>;
  typename ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(filename);
  reader->Update();
  itk::SmartPointer<TImage> out = reader->GetOutput();
  out->DisconnectPipeline();
  return out;
}

//template <typename TImage>
//void
//WriteImage(TImage * out, std::string filename, bool compress)
//{
//  using WriterType = itk::ImageFileWriter<TImage>;
//  typename WriterType::Pointer w = WriterType::New();
//  w->SetInput(out);
//  w->SetFileName(filename);
//  w->SetUseCompression(compress);
//  w->Update();
//}
//
//void
//WriteTransform(const itk::Object * transform, std::string fileName)
//{
//  using TransformWriterType = itk::TransformFileWriterTemplate<double>;
//  typename TransformWriterType::Pointer transformWriter = TransformWriterType::New();
//  transformWriter->SetInput(transform);
//  transformWriter->SetFileName(fileName);
//  transformWriter->Update();
//}
} // namespace

int
itkLandmarkAtlasSegmentationFilterTest(int argc, char * argv[])
{
  if (argc < 8)
  {
    std::cerr << "Missing parameters." << std::endl;
    std::cerr << "Usage: " << itkNameOfTestExecutableMacro(argv);
    std::cerr << " <inputImage> <atlasImage> <inputBones> <atlasLabels> <inputLandmarks> <atlasLandmarks> <outputBase>";
    std::cerr << std::endl;
    return EXIT_FAILURE;
  }
  const char * inputImageFilename = argv[1];
  const char * atlasImageFilename = argv[2];
  const char * inputBonesFilename = argv[3];
  const char * atlasLabelsFilename = argv[4];
  const char * inputLandmarksFilename = argv[5];
  const char * atlasLandmarksFilename = argv[6];

  const std::string outputBase = argv[7];


  constexpr unsigned int Dimension = 3;
  using ShortImageType = itk::Image<short, Dimension>;
  using LabelImageType = itk::Image<unsigned char, Dimension>;

  using FilterType = itk::LandmarkAtlasSegmentationFilter<ShortImageType, LabelImageType>;
  FilterType::Pointer filter = FilterType::New();

  ITK_EXERCISE_BASIC_OBJECT_METHODS(filter, LandmarkAtlasSegmentationFilter, ImageToImageFilter);

  ShortImageType::Pointer inputImage;
  ITK_TRY_EXPECT_NO_EXCEPTION(inputImage = ReadImage<ShortImageType>(inputImageFilename));
  LabelImageType::Pointer inputBones;
  ITK_TRY_EXPECT_NO_EXCEPTION(inputBones = ReadImage<LabelImageType>(inputBonesFilename));
  ShortImageType::Pointer atlasImage;
  ITK_TRY_EXPECT_NO_EXCEPTION(atlasImage = ReadImage<ShortImageType>(atlasImageFilename));
  LabelImageType::Pointer atlasLabels;
  ITK_TRY_EXPECT_NO_EXCEPTION(atlasLabels = ReadImage<LabelImageType>(atlasLabelsFilename));

  using PointsVector = std::vector<itk::Point<double, 3>>;

  PointsVector inputLandmarks;
  ITK_TRY_EXPECT_NO_EXCEPTION(inputLandmarks = readSlicerFiducials(inputLandmarksFilename));
  PointsVector atlasLandmarks;
  ITK_TRY_EXPECT_NO_EXCEPTION(atlasLandmarks = readSlicerFiducials(atlasLandmarksFilename));

  ShowProgress::Pointer showProgress = ShowProgress::New();
  filter->AddObserver(itk::ProgressEvent(), showProgress);
  filter->SetInput(inputImage);
  filter->SetInput(1, atlasImage);
  filter->SetInputLabels(inputBones);
  filter->SetAtlasLabels(atlasLabels);
  filter->SetInputLandmarks(inputLandmarks);
  filter->SetAtlasLandmarks(atlasLandmarks);

  ITK_TRY_EXPECT_NO_EXCEPTION(filter->Update());
  ITK_TRY_EXPECT_NO_EXCEPTION(WriteTransform(filter->GetLandmarksTransform(), outputBase + "L.h5"));
  ITK_TRY_EXPECT_NO_EXCEPTION(WriteTransform(filter->GetRigidTransform(), outputBase + "R.h5"));
  ITK_TRY_EXPECT_NO_EXCEPTION(WriteTransform(filter->GetAffineTransform(), outputBase + "A.h5"));
  ITK_TRY_EXPECT_NO_EXCEPTION(WriteImage(filter->GetOutput(), outputBase + "A.nrrd", true));
  filter->SetStopAtAffine(false);
  ITK_TRY_EXPECT_NO_EXCEPTION(filter->Update());
  ITK_TRY_EXPECT_NO_EXCEPTION(WriteTransform(filter->GetFinalTransform(), outputBase + "BS.h5"));
  ITK_TRY_EXPECT_NO_EXCEPTION(WriteImage(filter->GetOutput(), outputBase + "BS.nrrd", true));

  std::cout << "Test finished successfully." << std::endl;
  return EXIT_SUCCESS;
}
