#include <typeinfo>
#include "header.h"
#include "stdafx.h"
// helper function to save
// image with appending the path
void saveImage(string imagePath, const Mat image) {
  time_t timev;
  time(&timev);
  size_t position = imagePath.find('.');
  string toReplace = "-" + to_string(time(&timev));
  imagePath.replace(position, 0, toReplace);
  imwrite(imagePath, image);
}

struct region {
  static const int x = 30;
  static const int y = 30;
  static const int width = 200;
  static const int height = 350;
} roi;

class BottleDetection {
 private:
  string imagePath;
  Mat inputImage, outputImage;
  vector<int> linePoints;
  vector<int> lineUniquePoints;

 public:
  BottleDetection();
  BottleDetection(const string);
  BottleDetection(const Mat);
  const Mat applyFilters(const Mat);
  void findLineUniquePoints();
  void computeResults();
  void applyHoughTransform(const Mat);
  void applyProbabilisticHoughTransform(const Mat);
  void getPSNR(Mat&, Mat&);
  void performBlobDetection();
  void reduceImageDensity(Mat&, const bool, const int, const int);
  void getRegionOfInterest(Mat&, const int, const int, const int, const int);
};

// default ctor
BottleDetection::BottleDetection() { imagePath = "/Meeting-7/original-1.bmp"; }

// ctor with image path as string
BottleDetection::BottleDetection(const string imagePath)
    : imagePath(imagePath) {
  cout << "image path: " << imagePath << endl;
  inputImage = imread(imagePath);
}

// ctor with image as matrix
BottleDetection::BottleDetection(const Mat inputImage)
    : inputImage(inputImage) {}

// find the hough line
const Mat BottleDetection::applyFilters(const Mat image) {
  // grayscale conversion
  Mat gray;
  cvtColor(inputImage,     // source
           gray,           // destination
           COLOR_BGR2GRAY  // src, output, option
           );
  imshow("gray: ", gray);

  // median blur to reduce the noise
  Mat median;
  medianBlur(gray,    // source
             median,  // destination
             7        // aperture size (odd and >1)
             );
  imshow("median: ", median);

  // canny contour detection
  Mat canny;
  Canny(median,  // source
        canny,   // destination
        120,     // low threshold
        200,     // high threshold
        3        // kernel size 3x3
        );

  // thresholding
  Mat thresh;
  threshold(median, thresh, 170, 255, THRESH_BINARY);
  imshow("thresh", thresh);
  if (false /*!imagePath.empty()*/) {
    saveImage(imagePath, canny);
  }

  // Mat regionOfInterest =
  //     getRegionOfInterest(thresh, roi.x, roi.y, roi.width, roi.height);
  // imshow("region of interest", regionOfInterest);

  return thresh;
}

// draw a line using results of hough line transform
void BottleDetection::applyHoughTransform(const Mat thresh) {
  // to save lines from hough transform
  // Vector<Vec4i> is used for probabilistic hough transform
  vector<Vec2f> lines;
  // save a copy of inputImage in outputImage
  inputImage.copyTo(outputImage);
  // it will generate the accumulator cell
  // which is a 2xn matrix containing r->firstrow and theta->secondrow
  HoughLines(thresh,               // source
             lines,                // destination
             1,                    // rho resolution (0...2pi)
             180 * (CV_PI / 180),  // theta resolution (1 degree here)
             10,  // threshold -- vmin # of intersection to detect a line
             0,   // min. # of points to form a line
             0    // max gap b/w 2 points to be consider as 1 line.
             );

  vector<int> lineCoordinates;
  for (int loopVar = 0; loopVar < lines.size(); loopVar++) {
    // hough coordinates [rho, theta]
    float rho = lines[loopVar][0];    // first row
    float theta = lines[loopVar][1];  // second row
    Point pt1, pt2;                   // cartesian coordinates [x, y]

    double a = cos(theta), b = sin(theta);

    double x0 = rho * a;  // r * cos(theta)
    double y0 = rho * b;  // r * sin(theta)

    // rows are used to draw line to cover the whole vertical space for
    // particular x
    // extrapolation to draw a line

    // point 1
    pt1.x = cvRound(x0 + inputImage.rows * (-b));
    pt1.y = cvRound(y0 + inputImage.rows * (a));
    // point 2
    pt2.x = cvRound(x0 - inputImage.rows * (-b));
    pt2.y = cvRound(y0 - inputImage.rows * (a));

    //    cout << "x: " << x0 << endl;

    // save x coordinates
    lineCoordinates.push_back(x0);  // push point1 x coords

    line(outputImage, pt1, pt2, Scalar(0, 0, 255),  // red colour
         1,                                         // line width
         CV_AA);
  }

  sort(lineCoordinates.begin(), lineCoordinates.end(),
       greater<int>());  // sort in descending order

  linePoints = lineCoordinates;
}

// draw a line using results of hough line transform
void BottleDetection::applyProbabilisticHoughTransform(const Mat thresh) {
  // to save lines from hough transform
  // Vector<Vec4i> is used for probabilistic hough transform
  vector<Vec4i> lines;

  // it will generate the accumulator cell
  // which is a 2xn matrix containing r->firstrow and theta->secondrow
  HoughLinesP(thresh,  // source
              lines,  // destination (each line: x-start, y-start, x-end, y-end)
              1,      // rho resolution (0...2pi)
              180 * (CV_PI / 180),  // theta resolution (1 degree here)
              10,  // threshold -- vmin # of intersection to detect a line
              10,  // min. # of points to form a line
              10   // max gap b/w 2 points to be consider as 1 line.
              );

  getRegionOfInterest(inputImage, roi.x, roi.y, roi.width, roi.height);
  // save a copy of inputImage in outputImage
  inputImage.copyTo(outputImage);
  vector<int> lineCoordinates;

  for (size_t i = 0; i < lines.size(); i++) {
    Vec4i singleLine = lines[i];

    line(outputImage,                          // destination
         Point(singleLine[0], singleLine[1]),  // x and y of start point
         Point(singleLine[2], singleLine[3]),  // x and y of end point
         Scalar(0, 0, 255),                    // color of line -- red
         1,                                    // thickness of line -- thin
         CV_AA                                 //
         );

    lineCoordinates.push_back(singleLine[0]);
    lineCoordinates.push_back(singleLine[3]);
  }

  // sort in ascending order all the x axis values
  sort(lineCoordinates.begin(),  // first element
       lineCoordinates.end(),    // last element
       greater<int>());          // sort in descending order

  // removeDuplicate(lineCoordinates);
  linePoints = lineCoordinates;
}

void BottleDetection::findLineUniquePoints() {
  vector<int> condensedArray;
  if (linePoints.size() < 3) {
    condensedArray = linePoints;
  } else {
    size_t valueRange = linePoints.at(0) - linePoints.at(linePoints.size() - 1);
    cout << "range is: " << valueRange << endl;
    condensedArray.push_back(linePoints.at(0));
    for (int loopVar = 0; loopVar < linePoints.size() - 1; loopVar++) {
      //      cout << endl
      //           << "comparing " << linePoints.at(loopVar) << " with "
      //           << linePoints.at(loopVar + 1) << endl;
      if ((linePoints.at(loopVar) - linePoints.at(loopVar + 1)) >
          (valueRange / 2))
        condensedArray.push_back(linePoints.at(loopVar + 1));
    }
  }
  lineUniquePoints = condensedArray;
}

void BottleDetection::getPSNR(Mat& I1, Mat& I2) {
  getRegionOfInterest(I1, roi.x, roi.y, roi.width, roi.height);
  getRegionOfInterest(I2, roi.x, roi.y + 10, roi.width, roi.height);

  threshold(I1, I1, 150, 255, THRESH_BINARY);
  threshold(I2, I2, 150, 255, THRESH_BINARY);
  saveImage(masterproject::cwd + "/image-1.bmp", I1);
  saveImage(masterproject::cwd + "/image-2.bmp", I2);

  Mat s1;
  absdiff(I1, I2, s1);       // |I1 - I2|
  s1.convertTo(s1, CV_32F);  // cannot make a square on 8 bits
  s1 = s1.mul(s1);           // |I1 - I2|^2
  Scalar s = sum(s1);        // sum elements per channel
  double sse = s.val[0] + s.val[1] + s.val[2];  // sum channels

  // for small values return zero
  if (sse <= 1e-10) {
    cout << "sse: " << sse << endl;
  } else {
    double mse = sse / (double)(I1.channels() * I1.total());
    double psnr = 10.0 * log10((255 * 255) / mse);
    cout << "SNR: " << psnr << endl;
  }
  waitKey(0);
}

void BottleDetection::getRegionOfInterest(Mat& referenceImage, const int x,
                                          const int y, const int width,
                                          const int height) {
  // rectangular mask
  Rect Rec(x,      // x coordinate
           y,      // y coordinate
           width,  // width
           height  // height
           );
  // region of interest
  referenceImage = referenceImage(Rec);
  referenceImage.copyTo(inputImage);
}

void BottleDetection::reduceImageDensity(Mat& inputImage,
                                         const bool APPLY_FILTERS,
                                         const int minThreshValue,
                                         const int filterKernelSize) {
  Size s1 = inputImage.size();
  Mat median;
  
  if (APPLY_FILTERS == true) {
    medianBlur(inputImage,       // source
               median,           // destination
               filterKernelSize  // aperture size (odd and >1)
               );

    threshold(median, inputImage, minThreshValue, 255, CV_THRESH_BINARY);
  }

  getRegionOfInterest(inputImage,      // image
                      s1.width / 20,   // remove 1/5.2th from left
                      s1.height / 10,  // remove 1/10th from top
                      s1.width - (2 * s1.width / 20),
                      s1.height - s1.height / 5);
}

// get the blob of white colors
// in our case, these are the areas from
// where the light is coming out of chain
// we get the blobs align with their pose and size
// when the size is more than a threshold value ...
// ... (say average of the blobs sizes) it means
// something is present their besides only the chain
void BottleDetection::performBlobDetection() {
  reduceImageDensity(inputImage, true, 170, 7);

  SimpleBlobDetector::Params params;
  params.filterByArea = false;
  params.filterByCircularity = false;
  params.filterByConvexity = false;
  params.filterByColor = true;
  params.blobColor = 255;

  // Set up the detector with default parameters.
  SimpleBlobDetector detector(params);
  vector<KeyPoint> keypoints;
  detector.detect(inputImage, keypoints);
  //  float totalArea = 0;
  const float avgArea = 9.1;
  vector<KeyPoint> unqiue_keypoints;
  for (const auto& point : keypoints)
    if (point.size > avgArea) unqiue_keypoints.push_back(point);

  // find the average area from the teach image
  // for (const auto& point : keypoints) totalArea += point.size;
  // totalArea /= keypoints.size();
  // cout << "avg. area: " << totalArea << endl;

  // cout << "x: " << std::setw(3) << std::setfill('0') << int(point.pt.x)
  //      << "   |    y: " << std::setw(3) << std::setfill('0')
  //      << int(point.pt.y) << "   |    size: " << std::setw(3)
  //      << std::setfill('0') << float(point.size) << endl;

  // Draw detected blobs as red circles.
  // DrawMatchesFlags::DRAW_RICH_KEYPOINTS flag ensures the size of the circle
  // corresponds to the size of blob
  drawKeypoints(inputImage,         // input image
                unqiue_keypoints,   // keypoints found using blob detection
                inputImage,         // output image
                Scalar(0, 0, 255),  // colour for the points
                DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
  // Show blobs
  if (false) {
    saveImage(
        masterproject::cwd + "/meeting-11/blob-detection-result/result.bmp",
        inputImage);
  }
  imshow("keypoints", inputImage);
}

void BottleDetection::computeResults() {
  Mat thresh = applyFilters(inputImage);
  applyProbabilisticHoughTransform(thresh);
  imshow("6-hough line transform ", outputImage);
  waitKey();
}
