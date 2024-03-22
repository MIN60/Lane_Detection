✨ If you need a README in Korean, please check the 'korean' branch.

✨한국어로 된 readme가 필요하시다면 'korean' branch를 확인해주세요

# Lane_Detection
허프 변환과 케니 에지 연산자, 버드아이뷰를 이용한 차선 검출

2023 2학기 컴퓨터비전 수업 프로젝트

## 과제 구상
1.	ROI 선택
2.	Bird’s-eye view 변환
3.	HSV 색공간으로 변환
4.	밝기 채널(V)적응형 이진화
5.	잡음 제거(닫힘 연산)
6.	케니 에지 연산자로 에지 검출
7.	허프 변환 차선 후보 검출
8.	대표 차선 처리
    * 차선 병합
    * 가로선 필터링
    * 차선들의 간격이 가깝게 검출 시 필터링
9.	추가 처리
    * 한 쪽 차선만 검출될 시 가상의 차선 생성
    * 도로 영역 확장 및 표시
    * 차선 변경 시 화면 색 전환 및 경고음


## 주요 알고리즘 설명

### ROI 설정
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

영상이 재생되기 전에 ROI를 직접 사용자가 지정해줍니다. Frame 화면에 총 4번의 클릭을 통해 영역을 지정합니다. 선택된 point는 초록색 점으로 표시됩니다. 4개의 point를 모두 지정하면 자동으로 지정된 ROI를 고려하여 차선 검출 영상이 재생됩니다. ROI 값을 지정해주지 않으면 "ROI 선택 미완료"라는 출력과 함께 오류가 발생합니다. 
실제 차량에서 주행하며 차선인식을 할 경우에는 차량에 맞추어 ROI값을 고정해주면 되지만, 이번 과제에서는 영상으로 차선인식을 하는 것이기 때문에 활용도를 높이고자 사용자가 ROI를 선택할 수 있도록 만들었습니다.

### Bird’s-eye view로 변환
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

warpPerspective 함수를 이용하여 변환행렬을 계산하여 원근 변환을 수행합니다. 이를 통해 사용자가 선택한 ROI값이 Bird’s-eye view로 변환됩니다.
버드아이뷰로 변환할 경우 양쪽 차선의 기울기 값이 일정하게 나와 차선 인식 시 이점이 있습니다. 2D 평면에서의 점 (x, y)가 있고 이 점을 변환 매트릭스를 사용하여 (x', y')로 이동하게 되면, 변환 행렬 M을 이렇게 표기할 수 있습니다.

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/73a06fbc-4c07-4bf6-a9bc-ca4fb8ced6f8)

추후에 역변환 행렬(inverseMatrix)을 이용하면 버드아이뷰에서 검출한 차선을 원래 이미지에 맞추어 변환할 수 있습니다.

### HSV색 공간 변화

```C++
// HSV 색 공간으로 변환
Mat hsvbirdEyeView;
cvtColor(birdEyeView, hsvbirdEyeView, COLOR_BGR2HSV);

Mat vChannel;
Mat channels[3];
split(hsvbirdEyeView, channels);
```

기존의 RGB 컬러 공간의 이미지에서 밝기 값만을 추출하여 분석하기 위해 HSV로 컬러 공간을 변환합니다.

### 밝기 채널(V)적응형 이진화

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/0b504dd5-c047-462e-a6e4-79743f20fc6d)

▲이진화를 수행한 모습

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/0910595d-4800-44d9-8a65-587301fd25a1)

▲threshold를 이용하였을 때 차선 검출(조도변화에 민감)

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/153bdcc1-2b2b-49c0-8375-f6682b8d0cfc)

▲adaptiveThreshold를 이용하였을 때 차선 검출

HSV로 컬러 공간을 변환하여 나온 밝기 채널 V에서 차선을 검출하기 위해 이진화를 진행합니다. 이때 이미지를 작은 영역으로 나누어 각 영역별로 다른 임계값을 사용하는 adaptiveThreshold를 사용합니다. 이미지의 다양한 부분에서 다른 조명 환경에 적응하여 이진화를 수행하기 때문에, 터널 입출입시 조도 변화에도 일반적인 이진화보다 성능이 좋은 것을 확인할 수 있었습니다. 임계 값으로 이웃 지역의 평균값을 활용하기 위해 ADAPTIVE_THRESH_MEAN_C를 파라미터로 사용합니다.

### 잡음제거
```C++
//잡음제거를 위한 커널 사이즈 정의
Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
Mat kernel2 = getStructuringElement(MORPH_RECT, Size(5, 5));


// 닫힘
morphologyEx(binarybirdEyeView, binarybirdEyeView, MORPH_CLOSE, kernel2);
```
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/5eae656a-2ab7-424f-bda1-39469d78bd7d)

▲ 잡음제거를 하지 않았을 때

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/03d6348d-74e9-4cc7-b11e-bd58b24b9695)

▲닫힘 연산을 통해 잡음제거를 했을 때

잡음 제거를 위해 ‘닫힘’을 활용합니다. 이전에 비해 잡음이 많이 제거되었음을 확인할 수 있습니다. 닫힘 연산을 하기 위해 lmorphologyEx의 파라미터에 MORPH_CLOSE를 활용합니다.

## 케니 에지 연산자로 에지 검출
```C++
//케니 에지 연산자로 에지 검출
Canny(binarybirdEyeView, edges, 50, 150);
```
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/c3124b16-2c64-4934-8f1f-8f034a01d0ee)

▲에지가 검출된 3번째 이미지

잡음제거가 된 이진화 영상에서 Canny를 활용해 에지를 검출합니다. 50을 낮은 경계값, 150을 높은 경계값으로 저정합니다.

### 허프 변환 차선 후보 검출
```C++
 // 허프변환으로 차선 검출
vector<Vec4i> lines;
HoughLinesP(edges, lines, 1, CV_PI / 180, 40, 90, 300);
```

차선을 검출하기 위해 먼저 에지를 검출한 이미지에서 선분을 검출합니다. 이때 HoughLinesP 함수를 활용해 확률적으로 픽셀을 검사합니다. rho은 1로 지정하고, theta는 모든 방향에서 직선을 검출하기 위해 CV_PI/180를 지정합니다. threshold는 40, 검출할 직선의 최소 길이인 minLineLength는 90, 검출할 선 위의 점들 사이의 최대 거리를 의미하는 maxLineGap은 300으로 지정합니다.

### 차선 병합
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/3ed42d49-d6e3-4325-bfd3-0639f526eaf9)

▲ 도로 보수 흔적으로 인해 차선이 혼동되는 모습

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/2e1a3907-520c-436b-86f6-0e6d277ee869)

▲가장 바깥쪽 차선 하나를 표시하도록 한 모습

검은색으로 도로 보수 흔적이 차선으로 혼동되는 경우가 있었습니다. 이를 해결하기 위하여 기울기가 비슷한 차선끼리 병합하고, 바깥쪽 차선 하나만 인식하도록 하여 허위 데이터가 검출되는 빈도를 줄였습니다. 차선 병합의 경우에는 merge_lines라는 함수를 만들어 lines라는 이름의 벡터를 입력으로 받아 비슷한 기울기일 경우 차선을 합쳤습니다. 가장 바깥쪽 차선의 경우에는 leftMost는 가장 오른쪽 끝 지점을 가지므로 INT_MAX로 초기화하고, rightMost는 가장 왼쪽 끝 지점을 가지므로 0으로 초기화한 후 각 차선별로 가장 왼쪽에 있는 차선과 가장 오른쪽에 있는 차선을 검출했습니다.

### 가로선 없애기
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/1600c23a-dea0-465a-824b-170f32580f91)

▲도로에 있는 가로선들이 차선으로 인식되는 모습

가끔 도로에 가로선들이 나타날 때가 있었습니다. 이를 기울기 값으로 필터링 하였습니다. 기울기가 1 미만이면 가로선으로 간주하여 표시하지 않도록 했습니다.

### 가까운 선 필터링
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/1f93c158-3b2a-465c-9fc7-f8ff565ab9bd)

▲두 차선이 가깝고, 이미지 중점에 가깝게 나오는 모습

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/9f4ef51e-e8da-4b46-af9e-8024827bf958)

▲필터링 처리 이후

주행 중 차선을 변경하거나 차선 검출이 불안정할 때 차선이 이미지 중점에 가깝게 나오거나 두 차선이 너무 가깝게 검출되는 경우가 있었습니다. 이를 필터링하기 위해서 두 차선 사이의 간격에 임계값을 정해 임계값 미만의 거리값일 경우 차선을 표시하지 않도록 했습니다.

### 한쪽 차선만 검출될 때 반대편에 가상의 차선 생성
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

▲분홍색: 임의로 생성된 차선 (실제론 검출되지 않음)

한쪽 차선이 희미할 경우 검출된 차선을 기반으로 반대편에 차선을 그려줍니다. 왼쪽 차선만 감지될 경우 오른쪽 차선을 생성하고, 오른쪽 차선이 검출되면 왼쪽 차선을 생성합니다. 이 처리로 인해 한쪽 차선이 희미하거나 없어서 검출되지 않을 때 더욱 안정적인 주행이 가능합니다.

### 도로 영역 확장 및 표시
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/f2098a1f-f00c-4d1d-a4e3-f178783c5e5c)

▲도로 영역 확장 전 검출 사진

![image](https://github.com/MIN60/Lane_Detection/assets/49427080/055ca6b7-f59c-47ca-97d8-49642a6536d2)

▲도로 영역 확장 후 검출 사진

검출되는 두 차선을 활용하여 도로의 영역을 검출합니다. 
이때 매우 짧은 차선이 검출되는 경우, 차선의 길이에 맞게 도로가 좁게 표시되는 문제가 있었습니다. 이를 보완하기 위해 해당 차선의 두 점과 직선의 방정식을 활용하여 차선의 길이를 더 늘립니다. 이를 토대로 ROI 영역 내부에서 도로의 영역을 표시합니다.


### 차선 이탈 및 전방 주시 경고화면 및 경고음
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

특정 프레임에서 양쪽 차선이 모두 검출이 안되는 경우 경고를 1회 누적합니다. 이 경고가 25회 이상 연속으로 누적이 되는 경우 차선 변경 혹은 차선 이탈로 인식해 화면을 붉은 색으로 바꾸고 경고음이 출력되게 하여 경고를 주도록 만들었습니다.
차선을 변경하거나 차선 검출이 안되는 경우에 운전자에게 경고를 통해 전방을 주시할 것을 요청합니다.

## 결과 이미지
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/b9554aee-bdb2-45c1-b48e-2686bb5ed994)
![image](https://github.com/MIN60/Lane_Detection/assets/49427080/3bb046e6-179e-4eac-b281-731c50dd077e)

## 결과 영상

[![Video Label](http://img.youtube.com/vi/EPrIHE92vEo/0.jpg)](https://youtu.be/EPrIHE92vEo)
