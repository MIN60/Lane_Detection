✨ If you need a README in Korean, please check the 'korean' branch.

✨한국어로 된 readme가 필요하시다면 'korean' branch를 확인해주세요

# Lane_Detection
Lane Detection Using Hough Transform and Bird's Eye View

Computer Vision Class for the Second Semester of 2023

## Project Concept
1.	Select ROI (Region of Interest)
2.	Transform to Bird’s-eye view
3.	Convert to HSV color space
4.	Adaptive binarization of brightness channel (V)
5.	Noise removal (closing operation)
6.	Edge detection with Canny edge operator
7.	Lane candidate detection with Hough Transform
8.	Representative lane processing
    * Lane merging
    * Filtering horizontal lines
    * Filtering when lanes are detected too close together
9. Additional processing
    * Creating a virtual lane if only one lane is detected
    * Expanding and marking the road area
    * Screen color transition and warning sound when changing lanes


## Explanation of Key Algorithms

### Select ROI
```C++
// roi 설정  
//선택한 점을 roiPoints 벡터에 추가하고, 선택한 점을 원으로 표시
void selectROI(int event, int x, int y, int flags, void* userdata) {
    if (event == EVENT_LBUTTONDOWN) {
        roiPoints.push_back(Point2f((float)x, (float)y));
        std::cout << "Point selected: " << x << ", " << y << std::endl;
        circle(*(Mat*)userdata, Point(x, y), 5, Scalar(0, 255, 0), FILLED);
        imshow("Frame", *(Mat*)userdata);
        if (roiPoints.size() == 4) {
            std::cout << "ROI선택완료" << std::endl;
            destroyWindow("Frame");
        }
    }
}

```

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/262d61d9-7bad-4837-89c1-49329c4dd7c7)

Before the video starts, the user manually specifies the ROI (Region of Interest) through four clicks on the frame screen to define the area. The selected points are marked with green dots. Once all four points are specified, the lane detection video plays automatically, taking the defined ROI into account. If the ROI values are not specified, an error occurs with the message "ROI selection incomplete."
In real driving scenarios, the ROI values can be fixed according to the vehicle for lane detection. However, in this project, since lane detection is performed through a video, the user is allowed to select the ROI to increase usability.

### Transform to Bird’s-eye view
```C++
 // 원근 변환을 위한 ROI 점 저장
Point2f perspectiveROI[4];
perspectiveROI[0] = Point2f(0, 0);
perspectiveROI[1] = Point2f(originalSize.width - 1, 0);
perspectiveROI[2] = Point2f(0, originalSize.height - 1);
perspectiveROI[3] = Point2f(originalSize.width - 1, originalSize.height - 1);

//원근 변환 행렬
Mat Matrix = getPerspectiveTransform(roiPoints.data(), perspectiveROI);

//역 변환 행렬
Mat inverseMatrix = getPerspectiveTransform(perspectiveROI, roiPoints.data());
```

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/12caab55-799a-4688-a83b-2c39b99642c3)

The warpPerspective function is used to calculate a transformation matrix to perform a perspective transformation. This converts the ROI values selected by the user into a Bird’s-eye view. Converting to a Bird's-eye view ensures that the slopes of both lanes appear constant, which is advantageous for lane detection. If there's a point (x, y) in a 2D plane, and it is transformed to (x', y') using a transformation matrix, then the transformation matrix M can be denoted as follows.

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/73a06fbc-4c07-4bf6-a9bc-ca4fb8ced6f8)

By using the inverse transformation matrix (inverseMatrix) later on, the lanes detected in the Bird's-eye view can be transformed back to align with the original image.

### HSV Color Space Transformation

```C++
// HSV 색 공간으로 변환
Mat hsvbirdEyeView;
cvtColor(birdEyeView, hsvbirdEyeView, COLOR_BGR2HSV);

Mat vChannel;
Mat channels[3];
split(hsvbirdEyeView, channels);
```

To analyze only the brightness values from the original RGB color space image, the color space is converted to HSV.

### Adaptive Binarization of the Brightness Channel (V)

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/0b504dd5-c047-462e-a6e4-79743f20fc6d)

▲ Appearance after binarization

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/0910595d-4800-44d9-8a65-587301fd25a1)

▲ Lane detection using threshold (sensitive to changes in illumination)

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/153bdcc1-2b2b-49c0-8375-f6682b8d0cfc)

▲ Lane detection using adaptiveThreshold

Binarization is performed on the brightness channel V obtained after converting the color space to HSV to detect lanes. At this time, adaptiveThreshold, which divides the image into small areas and uses different threshold values for each area, is utilized. This method adapts to different lighting conditions across various parts of the image, allowing for binarization that performs better than standard methods, especially in conditions with changing illumination such as entering or exiting tunnels. The parameter ADAPTIVE_THRESH_MEAN_C is used to leverage the mean value of neighboring areas as the threshold value.

### Noise Removal
```C++
//잡음제거를 위한 커널 사이즈 정의
Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
Mat kernel2 = getStructuringElement(MORPH_RECT, Size(5, 5));


// 닫힘
morphologyEx(binarybirdEyeView, binarybirdEyeView, MORPH_CLOSE, kernel2);
```
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/5eae656a-2ab7-424f-bda1-39469d78bd7d)

▲ Without noise removal

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/03d6348d-74e9-4cc7-b11e-bd58b24b9695)

▲ After noise removal using the closing operation

For noise removal, the 'closing' operation is utilized, which significantly reduces noise compared to before. To perform the closing operation, the parameter MORPH_CLOSE is used in the function morphologyEx.

## Edge Detection with Canny Edge Operator
```C++
//케니 에지 연산자로 에지 검출
Canny(binarybirdEyeView, edges, 50, 150);
```
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/c3124b16-2c64-4934-8f1f-8f034a01d0ee)

▲ The third image with detected edges

Edges are detected in the binarized image, which has undergone noise removal, using the Canny method. A low threshold of 50 and a high threshold of 150 are set.

### Lane Candidate Detection with Hough Transform
```C++
 // 허프변환으로 차선 검출
vector<Vec4i> lines;
HoughLinesP(edges, lines, 1, CV_PI / 180, 40, 90, 300);
```

To detect lanes, line segments are first detected in the image with detected edges. Here, the HoughLinesP function is used to probabilistically inspect pixels. Rho is set to 1, and theta is set to CV_PI/180 to detect lines in all directions. The threshold is set to 40, the minimum line length to detect, minLineLength, is 90, and the maximum distance between points on a line, maxLineGap, is set to 300.

### Lane Merging
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/3ed42d49-d6e3-4325-bfd3-0639f526eaf9)

▲ Appearance of lanes being confused due to road maintenance marks

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/2e1a3907-520c-436b-86f6-0e6d277ee869)

▲ Only the outermost lane is marked

There were instances where black road maintenance marks were mistaken for lanes. To solve this, lanes with similar slopes were merged, and only the outermost lane was recognized to reduce the frequency of false data detection. For lane merging, a function named merge_lines was created. It takes a vector named lines as input and merges lanes with similar slopes. For the outermost lanes, leftMost is initialized to INT_MAX since it represents the farthest right end point, and rightMost is initialized to 0 as it represents the farthest left end point. Then, the leftmost lane and the rightmost lane for each lane were detected.

### Removing Horizontal Lines
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/1600c23a-dea0-465a-824b-170f32580f91)

▲ Horizontal lines on the road being recognized as lanes

Occasionally, horizontal lines appear on the road. These were filtered out based on their slope values. Lines with a slope less than 1 were considered horizontal and were not displayed.

### Filtering Close Lines
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/1f93c158-3b2a-465c-9fc7-f8ff565ab9bd)

▲ Two lanes appearing close together and near the center of the image

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/9f4ef51e-e8da-4b46-af9e-8024827bf958)

▲ After filtering processing

When changing lanes during driving or when lane detection is unstable, there are instances where lanes appear too close to the image's center or two lanes are detected too closely together. To filter these cases, a threshold for the distance between two lanes was set, and lanes closer than this threshold were not displayed.

### Creating a Virtual Lane When Only One Lane is Detected
```C++
// 한쪽 차선만 검출될 때 반대편에 가상의 차선을 그려주는 함수
Vec4i helperLine(Mat& frame, const Vec4i& line, const Mat& inverseMatrix, Scalar color, int frameCenterX) {
    // 라인에서 포인트 추출
    Point pt1 = Point(line[0], line[1]);
    Point pt2 = Point(line[2], line[3]);

    // 선 중심점 
    Point midPoint = (pt1 + pt2) * 0.5;

    // 프레임 센터
    int mirrorMidX = frameCenterX + (frameCenterX - midPoint.x);

    // 거리 계산
    int dx = (pt1.x - midPoint.x);
    int dy = (pt1.y - midPoint.y);

    // 반대편에 가상의 차선 계산
    Point mirrorPt1 = Point(mirrorMidX - dx, pt1.y);
    Point mirrorPt2 = Point(mirrorMidX + dx, pt2.y);

    // 포인트를 역변환행렬을 이용하여 다시 원위치로 계산
    vector<Point2f> linePoints, transLinePoints;
    linePoints.push_back(Point2f(mirrorPt1.x, mirrorPt1.y));
    linePoints.push_back(Point2f(mirrorPt2.x, mirrorPt2.y));
    perspectiveTransform(linePoints, transLinePoints, inverseMatrix);

    // Point2f를 Point로 변환
    Point fakePt1 = Point(cvRound(transLinePoints[0].x), cvRound(transLinePoints[0].y));
    Point fakePt2 = Point(cvRound(transLinePoints[1].x), cvRound(transLinePoints[1].y));

    // 가상의 차선 그리기
    if (fakePt1.x >= 0 && fakePt1.x < frame.cols && fakePt2.x >= 0 && fakePt2.x < frame.cols) {
        cv::line(frame, fakePt1, fakePt2, color, 3, cv::LINE_AA);
    }
    Vec4i fakeLine;
    fakeLine[0] = mirrorPt1.x;
    fakeLine[1] = mirrorPt1.y;
    fakeLine[2] = mirrorPt2.x;
    fakeLine[3] = mirrorPt2.y;

    return fakeLine;
}
```
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/c38883fe-4ea9-40ad-a632-5b287198e2bc)

▲ Pink lane: Arbitrarily created lane (not actually detected)

When one lane is faint, a lane on the opposite side is drawn based on the detected lane. If the left lane is detected, the right lane is created, and vice versa. This process allows for more stable driving when one lane is faint or missing.

### Expanding and Marking the Road Area
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/f2098a1f-f00c-4d1d-a4e3-f178783c5e5c)

▲ Photo before road area expansion

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/055ca6b7-f59c-47ca-97d8-49642a6536d2)

▲ Photo after road area expansion

The detected lanes are used to identify the road area.
There was an issue where the road appeared narrower in cases where very short lanes were detected. 
To address this, the length of the lane is extended using the equation of the line and two points of the lane. Based on this, the road area within the ROI is marked.


### Lane Departure and Forward Collision Warning Screen and Sound
```C++
bool warningsound = false;
// 25회 이상 검출 불가일 경우 화면을 붉은색으로 바꿔서 경고를 줌
if (warning && missingLaneCounter > 25) {
    if (redOverlay.empty()) {
        redOverlay = Mat::zeros(frame.size(), frame.type());
        redOverlay.setTo(Scalar(0, 0, 255));
        warningsound = true;
    }
    
    const double alpha = 0.5;

    addWeighted(finalLaneFrame, 1 - alpha, redOverlay, alpha, 0, finalLaneFrame);
}

if (warningsound) {
    system("C:/opencv_vision/warning_sound.mp3");
    warningsound = false;
}
```

[![Video Label](http://img.youtube.com/vi/N5THdY4K43M/0.jpg)](https://youtu.be/N5THdY4K43M)

If both lanes are not detected in a specific frame, a warning is accumulated once. If this warning accumulates more than 25 times consecutively, it is recognized as a lane change or lane departure, changing the screen to red and emitting a warning sound to alert the driver. The system is designed to request the driver to pay attention to the road ahead through warnings in cases of lane changes or undetected lanes.

## Result Image
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/b9554aee-bdb2-45c1-b48e-2686bb5ed994)
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/3bb046e6-179e-4eac-b281-731c50dd077e)

## Result Video

[![Video Label](http://img.youtube.com/vi/EPrIHE92vEo/0.jpg)](https://youtu.be/EPrIHE92vEo)
