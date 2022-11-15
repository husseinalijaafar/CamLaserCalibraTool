//
// Created by heyijia on 18-12-11.
//
#include "selectScanPoints.h"

int img_w = 608;
double focal = 450;
double z = 10;    // 在 10m 高处装一个相机，咔咔给激光点云拍照

struct LineSeg
{
    int id_start;
    int id_end;
    double dist;
};

/**
 * @brief FUnction seems to convert scan points to a line of points
 * @param points: a vector of x, y, and distance values
 * @param debug: boolean if it is debug mode or not
 */
std::vector< Eigen::Vector3d > AutoGetLinePts(const std::vector<Eigen::Vector3d> points, bool debug)
{
    // Parses each 
//    cv::Mat img(img_w, img_w, CV_8UC1, cv::Scalar::all(0));
    cv::Mat img(img_w, img_w, CV_8UC3, cv::Scalar(0,0,0)); // CV_8UC3 is rgb image
    for (auto pt: points) { // loops through all Points passed, as pt 
        // std::cout << "Pt loop: x " << pt.x() << " y " << pt.y() <<  std::endl;
        int col = (int)(pt.x() / z * focal + img_w/2);
        int row = (int)(- pt.y() / z * focal + img_w/2);  // -Y/Z 加了一个负号, 是为了抵消针孔投影时的倒影效果

        if(col > img_w-1 || col< 0 || row > img_w-1 || row < 0)
            continue;
        // std::cout << "col: " << col << " row: " << row << std::endl;
        // std::cout << "past continue" << std::endl;
        cv::Vec3b color_value(255,0,0); // RED
        img.at<cv::Vec3b>(row, col) = color_value; // img at that point = red 
//        img.at<uchar>(row, col) = 255;
    }

    /// detect and get line
    // 直接从每一帧激光的正前方开始搜索一定距离范围内的符合平面标定板形状的激光线段
    // Search for the laser line segment within a certain distance that conforms to the shape of the
    // plane calibration plate directly from the front of each frame of the laser
    int n = points.size(); // 360
    // std::cout << "n" << n << std::endl;
    int id = n/2; // 180
    //    std::cout << points.at(id).transpose() <<" "<<points.at(id+1).transpose() <<std::endl;
    // 假设每个激光点之间的夹角为0.3deg,
    // step 1: 如果有激光标定板，那么激光标定板必须出现在视野的正前方 120 deg 范围内(通常相机视野也只有 120 deg)，也就是左右各 60deg.
    // Assume the angle between each laser point is 0.3deg,
     // step 1: If there is a laser calibration board, 
     //then the laser calibration board must appear within 120 deg in front of the field of view 
     //(usually the camera field of view is only 120 deg), that is, 60deg on the left and right.
    int delta = 80/1; //TODO: the angle between measurements is actually 1 deg not 0.3 deg
    // id + delta = 260
    // id - delta = 100
    int id_left = std::min( id + delta, n-1);
    int id_right = std::max( id - delta , 0);

    int dist_left = points.at(id_left).norm(); // distance to point left of current point 
    int dist_right = points.at(id_right).norm(); // distance to point right of current point

    // 逻辑别搞复杂了。
    std::vector<LineSeg> segs; // create a vector of line segments
    // line segments have the structure
    /*
    struct LineSeg
    {
    int id_start;
    int id_end;
    double dist;
    };
    */

    double dist_thre = 0.8; // 
    int skip = 1; // decrease from 3 to 2
    int currentPt = id_right;
    int nextPt = currentPt + skip; 
    bool newSeg = true;
    LineSeg seg; // create single line segment
    for (int i = id_right; i < id_left - skip; i += skip) {

        if(newSeg)
        {
            seg.id_start = currentPt;
            seg.id_end = nextPt;
            newSeg = false;
        }

        double d1 = points.at(currentPt).head(2).norm(); // d1 = distance to current pt
        double d2 = points.at(nextPt).head(2).norm(); // d2 = distance to next point
        double range_max = 100;
        if(d1 < range_max && d2 < range_max)    // if distances are less than max distance
        {
            if(fabs(d1-d2) < dist_thre)  //  8cm // if the difference between the points is less than the distance threshold, you move to next point
            {
                seg.id_end = nextPt; 
                std::cout << "seg extended" << std::endl;
                // ? Segment _is_ getting extended 
            } else
            {   // if the diff in distance between points is too big (i.e scan leaves board), you evaluate  the segment 
                // std::cout << "seg threshold limit is reached" << std::endl; 
                // ? Yes the seg threshold is reached
                newSeg = true;
                Eigen::Vector3d dist = points.at(seg.id_start) - points.at(seg.id_end); // vector representing segment length
                if(dist.head(2).norm() > 0.3 // * length of line segment 
                   && points.at(seg.id_start).head(2).norm() < 3 // * line segment start cannot be farther than 2m
                   && points.at(seg.id_end).head(2).norm() < 3 //* line segment end cannot be farther than 2m
                   && seg.id_end-seg.id_start > 30   )  // * line segment should have at least 30 points
                   /*
                    At least longer than 20 cm, the calibration plate cannot be more than 2m
                    away from the laser, and there must be more than 50 laser points on the 
                    calibration plate
                   */
                {
                    // ! Error is here I believe 
                    // ! Seg is never pushed back
                    std::cout << "seg is established !!!! !!!! !!!!" << std::endl;
                    seg.dist = dist.head(2).norm();
                    segs.push_back(seg); // save the segment // never happening 
                }
            }

            currentPt = nextPt;        // next point
            nextPt = nextPt + skip;
        } else
        {
            if(d1 > range_max)
            {
                currentPt = nextPt;        // 下一个点
            }
            nextPt = nextPt + skip;
        }
    }
    // ? Error is here 
    std::cout <<"segs size: "<< segs.size() << std::endl;
    // 对 right segs 的边界进行扩充
    for (int i = 0; i < segs.size(); ++i) {
        LineSeg tmp = segs.at(i);

        for (int j = 1; j < 4; ++j)
        {
            int boundaryPt = tmp.id_end + j;
            double d1 = points.at(tmp.id_end).head(2).norm();
            double d2 = points.at(boundaryPt).head(2).norm();
            if(fabs(d1-d2) < dist_thre)  //  8cm
            {
                segs.at(i).id_end = boundaryPt;
            }
        }

        for (int j = -1; j > -4; --j)
        {
            int boundaryPt = tmp.id_start + j;
            double d1 = points.at(tmp.id_start).head(2).norm();
            double d2 = points.at(boundaryPt).head(2).norm();
            if(fabs(d1-d2) < dist_thre)  //  8cm
            {
                segs.at(i).id_start = boundaryPt;
            }
        }

    }

    std::vector<Eigen::Vector3d> ptsLine;
    if(segs.size() > 0)
    {
        LineSeg bestLine;
        int maxpts = -1;
        for (int i = 0; i < segs.size(); ++i) {
            LineSeg tmp = segs.at(i);
            int cnt = tmp.id_end - tmp.id_start;
            if(cnt > maxpts)
            {
                bestLine = tmp;
                maxpts = cnt;
            }
        }

        for (int i = bestLine.id_start; i < bestLine.id_end+1; ++i) {
            ptsLine.push_back(points.at(i));
        }
    }

    if(debug)
    {
        for (int j = 0; j < segs.size(); ++j) {
            LineSeg tmp = segs.at(j);
            for (int i = tmp.id_start; i < tmp.id_end; ++i)
            {
                Eigen::Vector3d pt = points.at(i);
                int col = (int)(pt.x() / z * focal + img_w/2);
                int row = (int)(- pt.y() / z * focal + img_w/2);  // -Y/Z 加了一个负号, 是为了抵消针孔投影时的倒影效果

                if(col > img_w-1 || col< 0 || row > img_w-1 || row < 0)
                    continue;

                cv::Vec3b color_value(0,255,0); // green
                img.at<cv::Vec3b>(row, col) = color_value;
            }
        }

        for (int j = 0; j < ptsLine.size(); ++j) {

            Eigen::Vector3d pt = ptsLine.at(j);
            int col = (int)(pt.x() / z * focal + img_w/2);
            int row = (int)(- pt.y() / z * focal + img_w/2);  // -Y/Z 加了一个负号, 是为了抵消针孔投影时的倒影效果

            if(col > img_w-1 || col< 0 || row > img_w-1 || row < 0)
                continue;

            cv::Vec3b color_value(0,0,200);  // blue
            img.at<cv::Vec3b>(row, col) = color_value;

        }

        cv::putText(img, "Detecting the Laser Points on the calibra planar!",
                    cvPoint(5,30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.7, cvScalar(255,255,255), 1, CV_AA);
        cv::imshow("ScanPoint",img);
        cv::waitKey(10);
    }

    return ptsLine;

}

std::vector<cv::Rect> PointsToImg(const std::vector<Eigen::Vector3d> points, bool selectPoint)
{

    cv::Mat img(img_w, img_w, CV_8UC1, cv::Scalar::all(0));

    for (auto pt: points) {
        int col = (int)(pt.x() / z * focal + img_w/2);
        int row = (int)(- pt.y() / z * focal + img_w/2);  // -Y/Z 加了一个负号, 是为了抵消针孔投影时的倒影效果

        if(col > img_w-1 || col< 0 || row > img_w-1 || row < 0)
            continue;

        img.at<uchar>(row, col) = 255;
    }

    std::vector<cv::Rect> rects;
    if(selectPoint)
    {
        GetRect getrect;
        getrect.gettingROI(img);  // 选取感兴趣的图像区域
        rects = getrect.rects;
    } else
    {
        cv::imshow("ScanPoint",img);
        cv::waitKey(0);
    }

    return rects;

}


std::vector<Eigen::Vector3d> GetROIScanPoints(const std::vector<Eigen::Vector3d> points, const std::vector<cv::Rect> rects)
{
    std::vector<Eigen::Vector3d> RoiPoints;
    for (auto rect : rects) {

        for (auto pt: points)
        {
            // 将点转换成图像坐标，然后看是否落在感兴趣区域内部
            int col = (int)(pt.x() / z * focal + img_w/2);
            int row = (int)(- pt.y() / z * focal + img_w/2);  // -Y/Z 加了一个负号, 是为了抵消针孔投影时的倒影效果
            if(col > rect.x + rect.width || col< rect.x || row > rect.y +rect.height || row < rect.y)
                continue;
            RoiPoints.push_back(pt);
        }
    }
    return RoiPoints;
}

void GetRect::onMouse(int event, int x, int y, int flags, void *userdata) {
    // Check for null pointer in userdata and handle the error
    GetRect* temp = reinterpret_cast<GetRect*>(userdata);
    temp->CallBackFunc(event, x, y);

}
void GetRect::CallBackFunc(int event, int x, int y) {

    if ( event == cv::EVENT_LBUTTONDOWN )
    {
        std::cout << "Left button of the mouse is clicked - position (" << x << ", " << y << ")" << std::endl;

        // Init your rect
        base.x = x;
        base.y = y;
        r.x = x;
        r.y = y;
        r.width = 0;
        r.height = 0;
        bDraw = true;
    }
    else if ( event == cv::EVENT_MOUSEMOVE )
    {
//        std::cout << "Mouse move over the window - position (" << x << ", " << y << ")" << std::endl;

        // If drawing, update rect width and height
        if(!bDraw) return;

        int dx = abs(r.x - x);
        int dy = abs(r.y - y);

        if(x < base.x) {
            r.x = x;
            r.width = abs(x - base.x);
        } else {
            r.width = dx;
        }

        if(y < base.y) {
            r.y = y;
            r.height = abs(y - base.y);
        } else {
            r.height = dy;
        }

        // Refresh
        working = layer.clone();
        cv::rectangle(working, r, cv::Scalar(125));
        cv::imshow("ScanPoint", working);
    }
    else if ( event == cv::EVENT_LBUTTONUP)
    {
        std::cout << "Left button released" << std::endl;

        // Save rect, draw it on layer
        rects.push_back(r);
        cv::rectangle(layer, r, cv::Scalar(125));

        r = cv::Rect();
        bDraw = false;

        // Refresh
        working = layer.clone();
        cv::rectangle(working, r, cv::Scalar(125));
        cv::imshow("ScanPoint", working);
    }
}
void GetRect::gettingROI(cv::Mat img)
{
    layer = img.clone();
    cv::putText(layer, "Please Select ROI scan data with your mouse. Then, Put any key to continue!",
                cvPoint(5,30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.7, cvScalar(255), 1, CV_AA);
    cv::namedWindow("ScanPoint", 1);
    cv::imshow("ScanPoint", layer);
    cv::setMouseCallback("ScanPoint", GetRect::onMouse, this);
    cv::waitKey(0);
}
