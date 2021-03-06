/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University
* of Zaragoza)
* For more information see <https://github.com/raulmur/Stereo_VO>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <chrono>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include <cv_bridge/cv_bridge.h>
#include <ros/ros.h>

#include <opencv2/core/core.hpp>

#include "../../../include/System.h"

using namespace std;
using namespace cv;

class ImageGrabber {
public:
  ImageGrabber(Stereo_VO::System *pSLAM) : mpSLAM(pSLAM) {}

  void GrabImage(const sensor_msgs::ImageConstPtr &msg);

  Stereo_VO::System *mpSLAM;
  bool do_rectify;
  cv::Mat M1l, M2l, M1r, M2r;
};

int main(int argc, char **argv) {
  ros::init(argc, argv, "Stereo");
  ros::start();

  Stereo_VO::System SLAM("/home/zh/VO/ORB_SLAM2/Vocabulary/ORBvoc.txt",
                         "/home/zh/VO/ORB_SLAM2/far_stereo.yaml",
                         Stereo_VO::System::STEREO, true);

  ImageGrabber igb(&SLAM);

  if (1) {
    igb.do_rectify = true;

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
      cout << "Current working dir: " << cwd << endl;
    else
      perror("getcwd() error");

    // Load settings related to stereo calibration
    cv::FileStorage fsSettings("/home/zh/VO/ORB_SLAM2/far_stereo.yaml",
                               cv::FileStorage::READ);
    if (!fsSettings.isOpened()) {
      cerr << "ERROR: Wrong path to settings" << endl;
      return -1;
    }

    cv::Mat K_l, K_r, P_l, P_r, R_l, R_r, D_l, D_r;
    fsSettings["LEFT.K"] >> K_l;
    fsSettings["RIGHT.K"] >> K_r;

    fsSettings["LEFT.P"] >> P_l;
    fsSettings["RIGHT.P"] >> P_r;

    fsSettings["LEFT.R"] >> R_l;
    fsSettings["RIGHT.R"] >> R_r;

    fsSettings["LEFT.D"] >> D_l;
    fsSettings["RIGHT.D"] >> D_r;

    int rows_l = fsSettings["LEFT.height"];
    int cols_l = fsSettings["LEFT.width"];
    int rows_r = fsSettings["RIGHT.height"];
    int cols_r = fsSettings["RIGHT.width"];

    if (K_l.empty() || K_r.empty() || P_l.empty() || P_r.empty() ||
        R_l.empty() || R_r.empty() || D_l.empty() || D_r.empty() ||
        rows_l == 0 || rows_r == 0 || cols_l == 0 || cols_r == 0) {
      cerr << "ERROR: Calibration parameters to rectify stereo are missing!"
           << endl;
      return -1;
    }

    cv::initUndistortRectifyMap(
        K_l, D_l, R_l, P_l.rowRange(0, 3).colRange(0, 3),
        cv::Size(cols_l, rows_l), CV_32F, igb.M1l, igb.M2l);
    cv::initUndistortRectifyMap(
        K_r, D_r, R_r, P_r.rowRange(0, 3).colRange(0, 3),
        cv::Size(cols_r, rows_r), CV_32F, igb.M1r, igb.M2r);
  }

  ros::NodeHandle nodeHandler;
  ros::Subscriber sub = nodeHandler.subscribe("/wide/image_raw", 1,
                                              &ImageGrabber::GrabImage, &igb);
  ros::spin();

  // Stop all threads
  SLAM.Shutdown();

  // Save camera trajectory
  SLAM.SaveKeyFrameTrajectoryTUM("KeyFrameTrajectory.txt");
  SLAM.SaveTrajectoryTUM("FrameTrajectory.txt");

  ros::shutdown();

  return 0;
}

void ImageGrabber::GrabImage(const sensor_msgs::ImageConstPtr &msg) {
  // Copy the ros image message to cv::Mat.
  cv_bridge::CvImagePtr cv_ptr;
  try {
    cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::MONO8);
  } catch (cv_bridge::Exception &e) {
    ROS_ERROR("cv_bridge exception: %s", e.what());
    return;
  }
  cv::Mat imLeft, imRight, imLeft1, imRight1;
  // imLeft = cv_ptr->image(Range(1,1024),Range(1,1280)).clone();
  // imRight = cv_ptr->image(Range(1,1024),Range(1281,2560)).clone();
  imLeft1 = cv_ptr->image(Range::all(), Range(0, 1280)).clone();
  imRight1 = cv_ptr->image(Range::all(), Range(1280, 2560)).clone();
  cv::Size rzSize(640, 512);
  cv::resize(imLeft1, imLeft, rzSize);
  cv::resize(imRight1, imRight, rzSize);
  if (1) {
    Mat imLeftRec, imRightRec;
    remap(imLeft, imLeftRec, M1l, M2l, cv::INTER_LINEAR);
    remap(imRight, imRightRec, M1r, M2r, cv::INTER_LINEAR);
    // mpSLAM->TrackStereo(imLeftRec(Range(310, 834), Range::all()),
    //                     imRightRec(Range(310, 834), Range::all()),
    //                     cv_ptr->header.stamp.toSec());
    mpSLAM->TrackStereo(imLeftRec(Range(115, 317), Range::all()),
                        imRightRec(Range(115, 317), Range::all()),
                        cv_ptr->header.stamp.toSec());
  } else {
    mpSLAM->TrackStereo(imLeft, imRight, cv_ptr->header.stamp.toSec());
  }
}
