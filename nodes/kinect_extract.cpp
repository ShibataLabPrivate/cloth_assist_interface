// kinect_extract.cpp: Program to extract different features from constructed point cloud
// Requirements: rosbag file as input and pcl libraries
// Author: Nishanth Koganti
// Date: 2015/9/2

// TODO:
// 1) Implement K-means center extraction for smooth cluster center transition.

// preprocessor directives
#define ESFSIZE 640
#define VFHSIZE 308

// CPP headers
#include <string>
#include <vector>
#include <stdio.h>
#include <sstream>
#include <stdlib.h>
#include <iostream>

// ROS headers
#include <ros/ros.h>
#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <rosbag/query.h>

// PCL headers
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/features/vfh.h>
#include <pcl/features/esf.h>
#include <pcl_ros/point_cloud.h>
#include <pcl/common/transforms.h>
#include <pcl/features/normal_3d.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/histogram_visualizer.h>
#include <pcl/filters/statistical_outlier_removal.h>

// help function
void help(const std::string &path)
{
  std::cout << path << " [options]" << std::endl
            << "  fileName: name of rosbag file" << std::endl
            << "  calibName: name of calibration file" << std::endl;
}

int main(int argc, char **argv)
{
  // create ros node with a random name to avoid conflicts
  ros::init(argc, argv, "kinect_extract", ros::init_options::AnonymousName);

  // check for failure
  if(!ros::ok())
  {
    return 0;
  }

  // filename default
  char c;
  std::string fileName, calibName;
  char cloudName[200], esfName[200], vfhName[200], centerName[200], filterName[200];

  // printing help information
  if(argc != 3)
  {
    help(argv[0]);
    ros::shutdown();
    return 0;
  }
  else
  {
    fileName = argv[1];
    calibName = argv[2];
  }

  // create fileNames for different output files
  sprintf(esfName, "../%sESF", fileName.c_str());
  sprintf(vfhName, "../%sVFH", fileName.c_str());
  sprintf(cloudName, "%sCloud.bag", fileName.c_str());
  sprintf(centerName, "../%sCentered", fileName.c_str());
  sprintf(filterName, "../%sFiltered", fileName.c_str());

  ofstream esfDat(esfName, ofstream::out);
  ofstream vfhDat(vfhName, ofstream::out);
	ofstream centerDat(centerName, ofstream::out);
  ofstream filterDat(filterName, ofstream::out);

  // initializing color and depth topic names
  std::string topicCloud = "/cloth/cloud";
  std::cout << "topic cloud: " << topicCloud << std::endl;

  std::vector<std::string> topics;
  topics.push_back(topicCloud);

  rosbag::Bag bag;
  bag.open(cloudName, rosbag::bagmode::Read);
  rosbag::View view(bag, rosbag::TopicQuery(topics));

  ifstream calibDat(calibName, ifstream::in);
  Eigen::Matrix4f transform = Eigen::Matrix4f::Identity();

  for (int i = 0; i < 4; i++)
    calibDat >> transform(i,0) >> c >> transform(i,1) >> c >> transform(i,2) >> c >> transform(i,3);

  // wait for key press
  // std::cin >> c;

  // pcl point cloud
  pcl::visualization::CloudViewer viewer("Feature Extraction");

  // pcl feature extraction Initialization
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloudVOG(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloudSOR(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloudESF(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloudVFH(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::Normal>::Ptr cloudNormals(new pcl::PointCloud<pcl::Normal>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloudCentered(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloudTransform(new pcl::PointCloud<pcl::PointXYZ>());
  pcl::search::KdTree<pcl::PointXYZ>::Ptr neTree(new pcl::search::KdTree<pcl::PointXYZ>());
  pcl::search::KdTree<pcl::PointXYZ>::Ptr vfhTree(new pcl::search::KdTree<pcl::PointXYZ>());
  pcl::PointCloud<pcl::VFHSignature308>::Ptr vfhs(new pcl::PointCloud<pcl::VFHSignature308>());
  pcl::PointCloud<pcl::ESFSignature640>::Ptr esfs(new pcl::PointCloud<pcl::ESFSignature640>());

  // feature instances
  Eigen::Vector4f centroid;
  pcl::VoxelGrid<pcl::PointXYZ> vog;
  pcl::StatisticalOutlierRemoval<pcl::PointXYZ> sor;
  pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
  pcl::ESFEstimation<pcl::PointXYZ, pcl::ESFSignature640> esf;
  pcl::VFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::VFHSignature308> vfh;

  // Parameter setting for filtering
  sor.setMeanK(100);
  ne.setRadiusSearch(0.03);
  ne.setSearchMethod(neTree);
  sor.setStddevMulThresh(0.7);
  vfh.setSearchMethod(vfhTree);
  vog.setLeafSize(0.005f, 0.005f, 0.005f);

  // ros time init
  ros::Time::init();

  // ros iterator initialization
  int frame = 0;
  ros::Rate rate(30);
  ros::Duration tPass;
  ros::Time now, begin;
  rosbag::View::iterator iter = view.begin();

  while(iter != view.end())
  {
    now = (*iter).getTime();
    if (frame == 0)
      begin = (*iter).getTime();
    tPass = now - begin;

    rosbag::MessageInstance const m = *iter;

    cloud = m.instantiate<pcl::PointCloud <pcl::PointXYZ> >();
    ++iter;

    vog.setInputCloud(cloud);
    vog.filter(*cloudVOG);

    sor.setInputCloud(cloudVOG);
    sor.filter(*cloudSOR);

    pcl::transformPointCloud(*cloudSOR, *cloudTransform, transform);

    // Center point cloud
    pcl::compute3DCentroid(*cloudTransform, centroid);
    pcl::demeanPointCloud(*cloudTransform, centroid, *cloudCentered);

    // Normal Estimation
    ne.setInputCloud(cloudCentered);
    ne.compute(*cloudNormals);

    // Viewpoint Feature Histogram
    vfh.setInputCloud(cloudCentered);
    vfh.setInputNormals(cloudNormals);
    vfh.compute(*vfhs);

    // Ensemble of Shape Functions
    esf.setInputCloud(cloudCentered);
    esf.compute(*esfs);

    vfhDat << tPass.toSec() << ",";
    for (int i = 0; i < VFHSIZE; i++)
      vfhDat << vfhs->points[0].histogram[i] << ",";
    vfhDat << endl;

    esfDat << tPass.toSec() << ",";
    for (int i = 0; i < ESFSIZE; i++)
      esfDat << esfs->points[0].histogram[i] << ",";
    esfDat << endl;

    centerDat << tPass.toSec() << "," << cloudCentered->size() << endl;
    for (int i = 0; i < cloudCentered->size(); i++)
      centerDat << cloudCentered->points[i].x << "," << cloudCentered->points[i].y << "," << cloudCentered->points[i].z << endl;

    filterDat << tPass.toSec() << "," << cloudTransform->size() << endl;
    for (int i = 0; i < cloudTransform->size(); i++)
      filterDat << cloudTransform->points[i].x << "," << cloudTransform->points[i].y << "," << cloudTransform->points[i].z << endl;

    viewer.showCloud(cloudCentered);
    std::cout << "Frame: " << frame << ", Time: " << tPass.toSec() << std::endl;
    std::cout << "Cloud: " << cloud->size() << ", VOG: " << cloudVOG->size() << ", SOR: " << cloudSOR->size() << ", VFH: " << vfhs->points.size() << ", ESF: " << esfs->points.size() << endl;
    frame++;

    rate.sleep();
  }

  bag.close();
  esfDat.close();
  vfhDat.close();
  centerDat.close();
  filterDat.close();

  // clean shutdown
  ros::shutdown();
  return 0;
}
