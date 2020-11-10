#include <chrono>
#include <iostream>

#include "itkImageFileReader.h"
#include "itkLandmarkBasedTransformInitializer.h"
#include "itkImageFileWriter.h"
#include "itkTransformFileWriter.h"
#include "itkImageRegionIteratorWithIndex.h"
#include "itkConstantPadImageFilter.h"
#include "itkQuadEdgeMesh.h"
#include "itkCuberilleImageToMeshFilter.h"
#include "itkTransformMeshFilter.h"
#include "itkMeshFileWriter.h"

auto startTime = std::chrono::steady_clock::now();

template <typename TImage>
itk::SmartPointer<TImage>
ReadImage(std::string filename)
{
  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Reading " << filename << std::endl;

  using ReaderType = itk::ImageFileReader<TImage>;
  typename ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(filename);
  reader->Update();
  itk::SmartPointer<TImage> out = reader->GetOutput();
  out->DisconnectPipeline();

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Done!" << std::endl;
  return out;
}

template <typename TImage>
void
WriteImage(itk::SmartPointer<TImage> out, std::string filename, bool compress)
{
  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Writing " << filename << std::endl;

  using WriterType = itk::ImageFileWriter<TImage>;
  typename WriterType::Pointer w = WriterType::New();
  w->SetInput(out);
  w->SetFileName(filename);
  w->SetUseCompression(compress);
  w->Update();

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Done!" << std::endl;
}

template <typename TMesh>
void
WriteMesh(itk::SmartPointer<TMesh> out, std::string filename, bool compress)
{
  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Writing " << filename << std::endl;

  using WriterType = itk::MeshFileWriter<TMesh>;
  typename WriterType::Pointer w = WriterType::New();
  w->SetInput(out);
  w->SetFileName(filename);
  w->SetUseCompression(compress);
  w->Update();

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Done!" << std::endl;
}

void
WriteTransform(const itk::Object * transform, std::string fileName)
{
  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Writing " << fileName << std::endl;

  using TransformWriterType = itk::TransformFileWriterTemplate<double>;
  typename TransformWriterType::Pointer transformWriter = TransformWriterType::New();
  transformWriter->SetInput(transform);
  transformWriter->SetFileName(fileName);
  transformWriter->Update();
}

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


template <typename ImageType>
void
mainProcessing(std::string inputBase, std::string poseFile, std::string outputBase)
{
  constexpr unsigned Dimension = ImageType::ImageDimension;
  using LabelImageType = itk::Image<unsigned char, Dimension>;
  using SizeType = typename LabelImageType::SizeType;
  using PointType = typename ImageType::PointType;
  using RigidTransformType = itk::VersorRigid3DTransform<double>;
  typename RigidTransformType::Pointer rigidTransform = RigidTransformType::New();

  std::vector<PointType> inputLandmarks = readSlicerFiducials(inputBase + ".fcsv");
  std::vector<PointType> atlasLandmarks = readSlicerFiducials(poseFile);
  itkAssertOrThrowMacro(inputLandmarks.size() == 3, "There must be exactly 3 input landmarks");
  itkAssertOrThrowMacro(atlasLandmarks.size() == 3, "There must be exactly 3 atlas landmarks");

  using LandmarkBasedTransformInitializerType =
    itk::LandmarkBasedTransformInitializer<RigidTransformType, ImageType, ImageType>;
  typename LandmarkBasedTransformInitializerType::Pointer landmarkBasedTransformInitializer =
    LandmarkBasedTransformInitializerType::New();

  landmarkBasedTransformInitializer->SetFixedLandmarks(atlasLandmarks);
  landmarkBasedTransformInitializer->SetMovingLandmarks(inputLandmarks);

  rigidTransform->SetIdentity();
  landmarkBasedTransformInitializer->SetTransform(rigidTransform);
  landmarkBasedTransformInitializer->InitializeTransform();

  // force rotation to be around center of femur head
  rigidTransform->SetCenter(atlasLandmarks.front());
  // and make sure that the other corresponding point maps to it perfectly
  rigidTransform->SetTranslation(inputLandmarks.front() - atlasLandmarks.front());

  WriteTransform(rigidTransform, outputBase + "-landmarks.tfm");
  typename RigidTransformType::Pointer inverseTransform = RigidTransformType::New();
  rigidTransform->GetInverse(inverseTransform);

  std::string fileName = inputBase + "-femur-label.nrrd";

  typename LabelImageType::Pointer inputLabels = ReadImage<LabelImageType>(fileName);

  // crop the pixel data to cutplane at 2.5 mm from origin along X (left-right) axis
  itk::ImageRegionIteratorWithIndex<LabelImageType> it(inputLabels, inputLabels->GetBufferedRegion());
  while (!it.IsAtEnd())
  {
    if (it.Get()) // only examine foreground voxels
    {
      PointType p = inputLabels->TransformIndexToPhysicalPoint<double>(it.GetIndex());
      p = inverseTransform->TransformPoint(p);
      if (p[0] > 2.5) // needs to be removed
      {
        it.Set(0);
      }
    }
    ++it;
  }
  WriteImage(inputLabels, inputBase + "-femur-label-cropped.nrrd", true);

  SizeType padding;
  padding.Fill(1);
  using PadType = itk::ConstantPadImageFilter<LabelImageType, LabelImageType>;
  typename PadType::Pointer pad = PadType::New();
  pad->SetInput(inputLabels);
  pad->SetPadUpperBound(padding);
  pad->SetPadLowerBound(padding);
  pad->SetConstant(0);
  pad->Update();

  std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Executing cuberille filter on " << fileName << std::endl;
  using TMesh = itk::QuadEdgeMesh<double, Dimension>;
  using TExtract = itk::CuberilleImageToMeshFilter<LabelImageType, TMesh>;
  typename TExtract::Pointer extract = TExtract::New();
  extract->SetInput(pad->GetOutput());
  extract->Update();
  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " Done!" << std::endl;

  WriteMesh<TMesh>(extract->GetOutput(), outputBase + "-mesh.obj", false);

  using MeshTransformType = itk::TransformMeshFilter<TMesh, TMesh, RigidTransformType>;
  typename MeshTransformType::Pointer filter = MeshTransformType::New();
  filter->SetInput(extract->GetOutput());
  filter->SetTransform(inverseTransform);
  filter->Update();
  WriteMesh<TMesh>(filter->GetOutput(), outputBase + "-mesh.vtk", false);

  diff = std::chrono::steady_clock::now() - startTime;
  std::cout << diff.count() << " All done!" << std::endl;
}

int
main(int argc, char * argv[])
{
  if (argc < 4)
  {
    std::cerr << "Usage:\n" << argv[0];
    std::cerr << " <InputBase> <pose.fcsv> <OutputBase>" << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    mainProcessing<itk::Image<short, 3>>(argv[1], argv[2], argv[3]);
    return EXIT_SUCCESS;
  }
  catch (itk::ExceptionObject & exc)
  {
    std::cerr << exc;
  }
  catch (std::exception & exc)
  {
    std::cerr << exc.what();
  }
  catch (...)
  {
    std::cerr << "Unknown error has occurred" << std::endl;
  }
  return EXIT_FAILURE;
}
