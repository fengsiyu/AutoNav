#include <bits/stdc++.h>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;
using namespace std;

typedef pair<int, int> floorpoint;

vector<floorpoint> floorPoints;
vector<floorpoint> floorPointsCoarse;
vector<floorpoint> boundaryPoints;
const int WindowSize = 200;
const int WindowSizeCoarse = 500;
const int StepSize = 40;
const int FLOOR_DIFF_THRES = 15;
const int FLOOR_COLOR_THRES = 100;
const float EMPTY_THRES = 0.05;

map<floorpoint, int> ComponentNumber;
int numComponents = 0;

Mat img;
int imagewidth, imageheight;

// bool isNeighbour(Point a, Point b)
// {
//     return (abs(a.x - b.x) <= StepSize or abs(a.y - b.y) <= StepSize);
// }

// dfs through points in image
void dfs(floorpoint currPoint, set<floorpoint>& visited, set<floorpoint>& allPoints) {
    int dx[4] = {0, 0, StepSize, -StepSize};
    int dy[4] = {StepSize, -StepSize, 0, 0};

    // if(visited.count(currPoint))
    //     return;

    ComponentNumber[currPoint] = numComponents;
    visited.insert(currPoint);

    for(int k=0; k<4; k++)
    {
        floorpoint newPoint = make_pair(currPoint.first + dx[k], currPoint.second + dy[k]);

        if(allPoints.count(newPoint) and (!visited.count(newPoint)))
            dfs(newPoint, visited, allPoints);
    }

}


// Mark connected components in point vector
void mark(vector<floorpoint>& v) {
    set<floorpoint> allPoints;
    set<floorpoint> visited;

    numComponents = 0;

    for(auto it: v)
        allPoints.insert(it);

    for(auto it: allPoints)
    {
        if(visited.count(it))
            continue;
        numComponents++;
        dfs(it, visited, allPoints);
    }
}

// Get yaw angle of bot
double getZAngle(Point point, double imagewidth, double focalLength) {
    double theta = atan((double)(point.x - imagewidth)/focalLength);
    return theta;
}

void checkGround(bool coarse) {
    int N;
    if(coarse) {
        N = WindowSizeCoarse / StepSize;
    }
    else {
        N = WindowSize / StepSize;
    }
    const int w = imagewidth / StepSize;
    const int h = imageheight / StepSize;

    int fCount[w][h];   // floor count dp in boxes
    int nfCount[w][h];  // non-floor count dp in boxes
    memset(fCount, -1, sizeof(fCount));
    memset(nfCount, -1, sizeof(nfCount));

    int countFloor = 0, countNFloor = 0;
    int start_i, end_i, start_j, end_j;
    Vec3b pixel; int pixel_b, pixel_g, pixel_r, pixel_avg;
    // Count number of floor and non-floor pixels in boxes of image
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            countFloor = 0; countNFloor = 0;
            start_i = i * StepSize; end_i = start_i + StepSize;
            start_j = j * StepSize; end_j = start_j + StepSize;
            for (int k = start_i; k < end_i; k++) {
                for (int l = start_j; l < end_j; l++) {
                    pixel = img.at<Vec3b>(l, k);
                    pixel_b = (int)pixel.val[0];
                    pixel_g = (int)pixel.val[1];
                    pixel_r = (int)pixel.val[2];
                    pixel_avg = (pixel_b + pixel_g + pixel_r)/3;
                    if(pixel_b > FLOOR_COLOR_THRES and pixel_g > FLOOR_COLOR_THRES and pixel_r > FLOOR_COLOR_THRES and abs(pixel_b - pixel_avg) < FLOOR_DIFF_THRES and abs(pixel_g - pixel_avg) < FLOOR_DIFF_THRES and abs(pixel_r - pixel_avg) < FLOOR_DIFF_THRES) {
                        countFloor++;
                    }
                    else {
                        countNFloor++;
                    }
                }
            }
            fCount[i][j] = countFloor;
            nfCount[i][j] = countNFloor;
            // cout << countFloor << "," << countNFloor << " ";
        }
        // cout << endl;
    }

    // Find floor points in image
    for (int i = 0; i < w-N; i++) {
        for (int j = 0; j < h-N; j++) {
            countFloor = 0; countNFloor = 0;
            for (int k = i; k < i+N; k++) {
                for (int l = j; l < j+N; l++) {
                    countFloor += fCount[k][l];
                    countNFloor += nfCount[k][l];
                }
            }
            float floorRatio = ((float)countNFloor)/((float)countFloor + (float)countNFloor);
            // cout << floorRatio << " ";
            if(floorRatio < EMPTY_THRES) {
                if(coarse) {
                    floorPointsCoarse.push_back(make_pair((i+N/2) * StepSize, (j+N/2) * StepSize) );
                }
                else {
                    floorPoints.push_back(make_pair((i+N/2) * StepSize, (j+N/2) * StepSize) );
                }
            }
        }
        // cout << endl;
    }


}


void findTileLines() {
    Mat gray, filt, thres;
    // Convert to grayscale
    cvtColor(img, gray, CV_BGR2GRAY);
    // Median filter
    medianBlur(gray, filt, 5);
    // Threshold image
    // adaptiveThreshold(filt, thres, 255, ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY, 11, 2);
    Canny(filt, thres, 50, 125);
    imwrite("floor_thres.jpg", thres);
    // Find hough transform
    vector<Vec2f> lines;
    HoughLines(thres, lines, 1, CV_PI/180, 500, 0, 0 );
    cout << lines.size() << endl;

    vector<pair<float, float> > line_params;
    for(auto tile_line:lines) {
        line_params.push_back(make_pair(tile_line[0], tile_line[1]));
    }
    sort(line_params.begin(), line_params.end());

    // Reduce lines by choosing only 1 from each cluster
    vector<pair<float, float> > line_params_unique;
    float cluster_thres = 10.0;
    float current_cluster_rho = line_params[0].first;
    line_params_unique.push_back(line_params[0]);
    for(auto params:line_params) {
        if(params.first - current_cluster_rho > cluster_thres) {
            current_cluster_rho = params.first;
            line_params_unique.push_back(params);
            cluster_thres = max((float)(0.05 * current_cluster_rho), cluster_thres);
        }
    }

    // draw lines
    const int len = 3000;
    for(auto tile_param: line_params_unique)
    {
        float rho = tile_param.first, theta = tile_param.second;
        cout << rho << " " << theta << endl;
        Point pt1, pt2;
        double a = cos(theta), b = sin(theta);
        double x0 = a*rho, y0 = b*rho;
        pt1.x = cvRound(x0 + len*(-b));
        pt1.y = cvRound(y0 + len*(a));
        pt2.x = cvRound(x0 - len*(-b));
        pt2.y = cvRound(y0 - len*(a));
        line( img, pt1, pt2, Scalar(0,0,255), 2, CV_AA);
    }
    imwrite("floor_lines.jpg", img);
}

// finds distance(pixels) of obstacle in given direction
// Direction of bot is currently assumed to be same as camera
int findDistance() {
    const int X_SPAN = 6 * StepSize;

    vector<int> pointYVec;
    for(auto point: floorPoints) {
        if(abs(point.first - imagewidth) < X_SPAN) {
            pointYVec.push_back(point.first);
        }
    }
    sort(pointYVec.begin(), pointYVec.end());

    return 0;
}


int main(int argc, char const *argv[]) {
    img = imread("./floor/floor_21.jpg", CV_LOAD_IMAGE_COLOR);
    imagewidth = img.cols;
    imageheight = img.rows;
    // cout << imagewidth << " " << imageheight << endl;

    checkGround(false);     // find points using fine window
    checkGround(true);     // find points using coarse window
    // findTileLines();

    // Draw points on image
    cout << "No. of coarse floor points: " << floorPointsCoarse.size() << endl;
    for(auto point: floorPointsCoarse) {
        // cout << point;
        circle(img, Point(point.first, point.second), 10, Scalar(0, 255, 0), -1);
    }

    // // Mark connected components in points
    // mark(floorPointsCoarse);
    // cout << "Marked: " << ComponentNumber.size() << endl;
    // cout << "Number of components: " << numComponents << endl;
    //
    // floorpoint target = getMeanLargestComp(numComponents);
    // cout << target.first << " " << target.second << endl;
    // Point targetPoint(target.first, target.second);
    // circle(img, targetPoint, 30, Scalar(0, 0, 255), -1);
    //
    // // Get Boundary points
    // getBoundaryPoints();
    // for(auto boundPoint: boundaryPoints) {
    //     Point boundaryPoint(boundPoint.first, boundPoint.second);
    //     circle(img, boundaryPoint, 10, Scalar(255, 0, 0), -1);
    }

    imwrite("floor_boundary.jpg", img);


    // int distanceToObstacle = findDistance();
    // cout << "Distance to obstacle: " << distanceToObstacle << " pixels" << endl;

    return 0;
}
