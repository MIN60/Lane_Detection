#include <iostream> 
#include <opencv2/opencv.hpp>
#include <vector>
#include <map>
#include <limits>
#include <cstdlib>

using namespace cv;
using std::vector;
using std::map;

vector<Point2f> roiPoints; //사용자가 선택한 ROI(Region of Interest)의 꼭지점 좌표를 저장
vector<Vec4i> lastDetected; // 마지막으로 검출된 선들을 저장
float alpha = 0.8; // 이전 프레임의 선 가중치
float beta = 1 - alpha; // 새 프레임의 선 가중치

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

// 차선의 기울기 계산
float calculateSlope(const Point& pt1, const Point& pt2) {
    if (pt2.x - pt1.x == 0) {
        return std::numeric_limits<float>::max();
    }
    return static_cast<float>(pt2.y - pt1.y) / (pt2.x - pt1.x);
}

// 수평선 판단 함수
bool isHorizontalLine(const Vec4i& line) {
    Point pt1(line[0], line[1]);
    Point pt2(line[2], line[3]);
    float slope = calculateSlope(pt1, pt2);
    // 기울기가 거의 0에 가까우면 수평선으로 판단
    return std::abs(slope) < 1;
}

// 짧은 선 판단
bool shortLine(const Vec4i& line) {
    Point pt1(line[0], line[1]);
    Point pt2(line[2], line[3]);
    double length = norm(pt1 - pt2); // 선분의 길이 계산

    // 짧은 선 길이
    double minLength = 350.0;

    return length >= minLength; // 최소 길이보다 긴 선분인지 확인
}

bool shortLine2(const Vec4i& line) {
    Point pt1(line[0], line[1]);
    Point pt2(line[2], line[3]);
    double length = norm(pt1 - pt2); // 선분의 길이 계산

    // 짧은 선 길이
    double minLength = 50.0;

    return length >= minLength; // 최소 길이보다 긴 선분인지 확인
}

Vec4i merge_lines(vector<Vec4i>& lines) {
    // 기울기 비슷한 선들만 병합

    // 일정 기울기 임계값 설정 (수직선에 가까운 선들을 위한)
    float slope_threshold = std::tan(CV_PI / 180 * 35); // 대략 35도 내외의 기울기를 갖는 선들만 병합
    map<int, vector<Vec4i>> grouped_lines; // 기울기를 키로 하는 선분 그룹

    for (const Vec4i& l : lines) {
        Point pt1(l[0], l[1]);
        Point pt2(l[2], l[3]);
        float slope = calculateSlope(pt1, pt2);

        // 수직에 가까운 선들만 포함
        if (std::abs(slope) > slope_threshold) {
            int rounded_slope = static_cast<int>(std::round(slope)); // 기울기를 반올림하여 그룹화
            grouped_lines[rounded_slope].push_back(l);
        }
    }

    vector<Vec4i> merged_lines;
    // 각 그룹별로 선분 병합
    for (auto& group : grouped_lines) {
        Point2f sum_start(0, 0), sum_end(0, 0);
        int count = 0;

        for (const Vec4i& l : group.second) {
            if (!shortLine2(l)) {
                continue; // 짧은 선분 걸러내기
            }

            Point2f pt1(l[0], l[1]);
            Point2f pt2(l[2], l[3]);
            sum_start += pt1;
            sum_end += pt2;
            count++;
        }

        if (count == 0) {
            continue; // 유효한 선분이 없으면 다음 그룹으로 이동
        }

        Point2f avg_start = sum_start / static_cast<float>(count);
        Point2f avg_end = sum_end / static_cast<float>(count);

        if (!isHorizontalLine(Vec4i(avg_start.x, avg_start.y, avg_end.x, avg_end.y))) {
            merged_lines.push_back(Vec4i(avg_start.x, avg_start.y, avg_end.x, avg_end.y));
        }
    }

    if (!merged_lines.empty()) {
        return merged_lines[0];
    }

    return Vec4i(0, 0, 0, 0);
}

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




// 차선의 중앙점을 계산하는 함수
Point2f getLineCenter(const Vec4i& line) {
    return Point2f((line[0] + line[2]) * 0.5f, (line[1] + line[3]) * 0.5f);
}

// 두 차선 사이의 거리를 계산하는 함수
float lineDistance(const Vec4i& line1, const Vec4i& line2) {
    Point2f center1 = getLineCenter(line1);
    Point2f center2 = getLineCenter(line2);

    // 유클리디안 거리 공식을 사용하여 거리 계산
    float distance = sqrt(pow(center1.x - center2.x, 2) + pow(center1.y - center2.y, 2));
    return distance;
}

// 왼쪽 차선과 오른쪽 차선을 이미지에 그리는 함수
void drawLane(Mat& frame, const Vec4i& leftLine, const Vec4i& rightLine, const Mat& inverseMatrix) {
    // 왼쪽차선 그리기
    vector<Point2f> leftLinePt = { Point2f(leftLine[0], leftLine[1]), Point2f(leftLine[2], leftLine[3]) };
    vector<Point2f> transLeftLinePt;
    perspectiveTransform(leftLinePt, transLeftLinePt, inverseMatrix);
    line(frame, Point(transLeftLinePt[0].x, transLeftLinePt[0].y), Point(transLeftLinePt[1].x, transLeftLinePt[1].y), Scalar(0, 165, 255), 3, LINE_AA);

    // 오른족 차선 그리기
    vector<Point2f> rightLinePt = { Point2f(rightLine[0], rightLine[1]), Point2f(rightLine[2], rightLine[3]) };
    vector<Point2f> transRightLinePt;
    perspectiveTransform(rightLinePt, transRightLinePt, inverseMatrix);
    line(frame, Point(transRightLinePt[0].x, transRightLinePt[0].y), Point(transRightLinePt[1].x, transRightLinePt[1].y), Scalar(255, 0, 0), 3, LINE_AA);
}

//이미지 중심과 주어진 선 사이의 거리를 계산
float distanceFromCenter(const Point2f center, const Vec4i& line) {
    Point2f lineCenter = getLineCenter(line);
    return abs(center.x - lineCenter.x);
}


int main(int argc, char** argv) {
    // 파일 열기
    VideoCapture cap("C:/opencv_vision/clip3.mp4");
    if (!cap.isOpened()) {
        std::cout << "Error opening video" << std::endl;
        return -1;
    }

    Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cout << "Error loading frame" << std::endl;
        return -1;
    }

    // 프레임 사이즈 조정
    resize(frame, frame, Size(), 0.3, 0.3, INTER_LINEAR);
    Size originalSize = frame.size();

    namedWindow("Frame", WINDOW_AUTOSIZE);
    setMouseCallback("Frame", selectROI, &frame);

    imshow("Frame", frame);
    waitKey(0);

    // 차선이탈 및 주의 경고를 위한 변수 선언
    bool warning = false;
    Mat redOverlay;

    //ROI 선택 체크
    if (roiPoints.size() != 4) {
        std::cout << "ROI 선택 미완료" << std::endl;
        return -1;
    }

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

    Mat birdEyeView, binarybirdEyeView, finalLaneFrame, edges, blueOverlay;

    int missingLaneCounter = 0; // 양쪽 차선이 검출되지 않는 프레임 수 카운트


    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        //프레임 크기 조정
        resize(frame, frame, Size(), 0.3, 0.3, INTER_LINEAR);

        //버드아이뷰 생성
        warpPerspective(frame, birdEyeView, Matrix, originalSize);

        // HSV 색 공간으로 변환
        Mat hsvbirdEyeView;
        cvtColor(birdEyeView, hsvbirdEyeView, COLOR_BGR2HSV);

        Mat vChannel;
        Mat channels[3];
        split(hsvbirdEyeView, channels);

        // V (밝기) 채널
        vChannel = channels[2];

        // V 채널 이진화
        Mat binarybirdEyeView;
        // 적응형 이진화
        adaptiveThreshold(vChannel, binarybirdEyeView, 255, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 11, 2);

        //잡음제거를 위한 커널 사이즈 정의
        Mat kernel = getStructuringElement(MORPH_RECT, Size(5, 5));
        Mat kernel2 = getStructuringElement(MORPH_RECT, Size(5, 5));


        // 닫힘
        morphologyEx(binarybirdEyeView, binarybirdEyeView, MORPH_CLOSE, kernel2);

        //케니 에지 연산자로 에지 검출
        Canny(binarybirdEyeView, edges, 50, 150);

        // 허프변환으로 차선 검출
        vector<Vec4i> lines;
        HoughLinesP(edges, lines, 1, CV_PI / 180, 40, 90, 300);

        //이전에 검출된 선들과 현재 검출된 선들을 비교
        if (lastDetected.size() != lines.size()) {
            lastDetected = lines;
        }

        // 최종 차선 표시할 프레임 생성
        finalLaneFrame = frame.clone();

        //검출된 차선들을 기울기를 기준으로 분류
        map<float, vector<Vec4i>> slopes;
        for (const Vec4i& l : lines) {
            Point pt1(l[0], l[1]);
            Point pt2(l[2], l[3]);
            float slope = calculateSlope(pt1, pt2);
            slopes[slope].push_back(l);
        }

        //기울기 비슷한 차선 병합
        vector<Vec4i> merged_lines;
        for (auto& pair : slopes) {
            vector<Vec4i>& grouped_lines = pair.second;

            // 수평선 제외
            vector<Vec4i> filtered_lines;
            for (const Vec4i& line : grouped_lines) {
                if (!isHorizontalLine(line)) {
                    filtered_lines.push_back(line);
                }
            }

            Vec4i merged_line = merge_lines(filtered_lines);
            if (merged_line != Vec4i(0, 0, 0, 0)) { // 병합된 선이 유효하다면
                std::cout << "Merged line: " << merged_line << std::endl; // 병합된 선을 출력

                //선분 길이를 확인하고 짧으면 늘리기
               //if (!shortLine(merged_line)) {
               //    float factor = 150.0; // 늘릴 길이
               //    merged_line = extendLine(merged_line, factor);
               //}

                merged_lines.push_back(merged_line); // 병합된 선을 최종 목록에 추가


            }
            else {
                std::cout << "Merged line 미사용" << std::endl; // 병합된 선을 출력
            }
        }


        // 이미지 중심 좌표 계산
        int centerX = originalSize.width / 2; // 이미지 중심 x-좌표
        Point2f imageCenter = Point2f(originalSize.width / 2, originalSize.height / 2);


        // 중심 좌표를 기준으로 왼쪽, 오른쪽 차선 분류
        vector<Vec4i> leftLines, rightLines;
        for (const auto& line : merged_lines) {
            Point pt1(line[0], line[1]);
            Point pt2(line[2], line[3]);
            int midX = (pt1.x + pt2.x) / 2;
            if (midX < centerX) {
                leftLines.push_back(line);
            }
            else {
                rightLines.push_back(line);
            }
        }

        // 가장 바깥쪽 차선으로 선택
        Vec4i leftMost = { INT_MAX, 0, INT_MAX, 0 };
        Vec4i rightMost = { 0, 0, 0, 0 };
        for (const auto& line : leftLines) {
            if (line[0] < leftMost[0]) {
                leftMost = line;
            }
        }
        for (const auto& line : rightLines) {
            if (line[2] > rightMost[2]) {
                rightMost = line;
            }
        }


        //차선 사이의 거리 계산
        float distance = lineDistance(leftMost, rightMost);

        // 중심으로부터 각 차선 거리 계산
        float distanceLeft = distanceFromCenter(imageCenter, leftMost);
        float distanceRight = distanceFromCenter(imageCenter, rightMost);

        // 왼쪽 차선과 오른쪽 차선 그리기
        if (leftMost != Vec4i(INT_MAX, 0, INT_MAX, 0) && distanceLeft > 150) {
            if (distance > 300) {
                drawLane(finalLaneFrame, leftMost, rightMost, inverseMatrix);
            }
            else {
                std::cout << "Line detection ignored due to close proximity." << std::endl;
            }
            warning = false;
            missingLaneCounter = 0;
        }
        else {
            warning = true;
            missingLaneCounter++;
        }
        if (rightMost != Vec4i(0, 0, 0, 0) && distanceRight > 150) {
            if (distance > 300) {
                drawLane(finalLaneFrame, leftMost, rightMost, inverseMatrix);
            }
            else {
                std::cout << "Line detection ignored due to close proximity." << std::endl;
            }
            warning = false;
            missingLaneCounter = 0;
        }
        else {
            warning = true;
            missingLaneCounter++;
        }


        // 한 쪽 차선만 검출될때 반대편에 가상의 차선을 그려줌
        if (leftMost != Vec4i(INT_MAX, 0, INT_MAX, 0) && rightMost == Vec4i(0, 0, 0, 0) && distanceLeft > 100) {
            // 왼쪽 차선만 검출되면 오른쪽 그려줌
            drawLane(finalLaneFrame, leftMost, leftMost, inverseMatrix);
            rightMost = helperLine(finalLaneFrame, leftMost, inverseMatrix, Scalar(255, 0, 255), centerX);
            std::cout << "왼쪽 차선만 검출됨" << std::endl;
            warning = false;
            missingLaneCounter = 0;
        }
        else if (rightMost != Vec4i(0, 0, 0, 0) && leftMost == Vec4i(INT_MAX, 0, INT_MAX, 0) && distanceRight > 100) {
            // 오른쪽 차선만 검출되면 왼쪽 그려줌
            drawLane(finalLaneFrame, rightMost, rightMost, inverseMatrix);
            leftMost = helperLine(finalLaneFrame, rightMost, inverseMatrix, Scalar(255, 0, 255), centerX);
            std::cout << "오른쪽 차선만 검출됨" << std::endl;
            warning = false;
            missingLaneCounter = 0;
        }
        else if (leftMost == Vec4i(INT_MAX, 0, INT_MAX, 0) && rightMost == Vec4i(0, 0, 0, 0)) {
            // 둘다 검출 안되면 경고 누적
            warning = true;
            missingLaneCounter++;
        }

        bool warningsound = false;
        // 25회 이상 검출 불가일 경우 화면을 붉은색으로 바꿔서 경고를 줌
        if (warning && missingLaneCounter > 25) {
            if (redOverlay.empty()) {
                redOverlay = Mat::zeros(frame.size(), frame.type());
                redOverlay.setTo(Scalar(0, 0, 255));
                warningsound = true;
            }
            // 오버레이 생선
            const double alpha = 0.5;

            addWeighted(finalLaneFrame, 1 - alpha, redOverlay, alpha, 0, finalLaneFrame);
        }

        if (warningsound) {
            system("C:/opencv_vision/warning_sound.mp3");
            warningsound = false;
        }


        //차선 채우기
        blueOverlay = frame.clone();
        if (leftMost != Vec4i(INT_MAX, 0, INT_MAX, 0) && rightMost != Vec4i(0, 0, 0, 0)) {
            vector<Point2f> LeftLinePt = { Point2f(leftMost[0], leftMost[1]), Point2f(leftMost[2], leftMost[3]) };
            vector<Point2f> transLeftLinePt;
            perspectiveTransform(LeftLinePt, transLeftLinePt, inverseMatrix);

            //선 길이 늘이기
            float a = static_cast<float>(transLeftLinePt[1].y - transLeftLinePt[0].y) / (transLeftLinePt[1].x - transLeftLinePt[0].x);
            float b = static_cast<float>(transLeftLinePt[1].x * transLeftLinePt[0].y - transLeftLinePt[0].x * transLeftLinePt[1].y) / (transLeftLinePt[1].x - transLeftLinePt[0].x);
            leftMost[0] = (roiPoints[0].y - b) / a;
            leftMost[1] = roiPoints[0].y;
            leftMost[2] = (roiPoints[2].y - b) / a;
            leftMost[3] = roiPoints[2].y;

            vector<Point2f> rightLinePt = { Point2f(rightMost[0], rightMost[1]), Point2f(rightMost[2], rightMost[3]) };
            vector<Point2f> transRightLinePt;
            perspectiveTransform(rightLinePt, transRightLinePt, inverseMatrix);

            //선 길이 늘이기
            a = static_cast<float>(transRightLinePt[1].y - transRightLinePt[0].y) / (transRightLinePt[1].x - transRightLinePt[0].x);
            b = static_cast<float>(transRightLinePt[1].x * transRightLinePt[0].y - transRightLinePt[0].x * transRightLinePt[1].y) / (transRightLinePt[1].x - transRightLinePt[0].x);
            rightMost[0] = (roiPoints[1].y - b) / a;
            rightMost[1] = roiPoints[1].y;
            rightMost[2] = (roiPoints[3].y - b) / a;
            rightMost[3] = roiPoints[3].y;

            vector<Point> road_pts;
            if (leftMost[1] < leftMost[3]) {
                road_pts.push_back(Point(leftMost[0], leftMost[1]));
                road_pts.push_back(Point(leftMost[2], leftMost[3]));
            }
            else {
                road_pts.push_back(Point(leftMost[2], leftMost[3]));
                road_pts.push_back(Point(leftMost[0], leftMost[1]));
            }
            if (rightMost[1] < rightMost[3]) {
                road_pts.push_back(Point(rightMost[2], rightMost[3]));
                road_pts.push_back(Point(rightMost[0], rightMost[1]));
            }
            else {
                road_pts.push_back(Point(rightMost[0], rightMost[1]));
                road_pts.push_back(Point(rightMost[2], rightMost[3]));
            }
            cv::fillPoly(blueOverlay, { road_pts }, Scalar(255, 0, 0), LINE_AA);

            //도로 영역 파란색으로 색칠
            for (int i = 0; i < frame.rows; ++i) {
                for (int j = 0; j < frame.cols; ++j) {
                    Scalar intensity = blueOverlay.at<cv::Vec3b>(i, j);
                    if (intensity == Scalar(255, 0, 0)) {
                        finalLaneFrame.at<cv::Vec3b>(i, j)[0] = 200;
                    }
                }
            }
        }


        // 각 차선의 대표적인 선 사이 거리
        std::cout << "Distance between lines: " << distance << std::endl;

        // 출력
        imshow("Original Frame", frame);
        imshow("Bird's Eye View", birdEyeView);
        imshow("Binary Bird's Eye View", binarybirdEyeView);
        imshow("Edges", edges);
        imshow("Final Lane", finalLaneFrame);

        char c = (char)waitKey(25);
        if (c == 27) break;
    }

    cap.release();

    return 0;
}
