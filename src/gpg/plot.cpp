#include <gpg/plot.h>
#include "ros/ros.h"
#include "std_msgs/Bool.h"
#include "gpg/grasp_poses.h"
#include "geometry_msgs/Pose.h"
#include "cruzr_planner/Graspit_pose.h"
#include "cruzr_planner/Graspit_poses.h"
gpg::grasp_poses grasp_poses;
cruzr_planner::Graspit_poses grasp_poses_without_energy;
void Plot::plotFingers3D(const std::vector<GraspSet>& hand_set_list, const PointCloudRGBA::Ptr& cloud,
  std::string str, double outer_diameter, double finger_width, double hand_depth, double hand_height) const
{
  std::vector<Grasp> hands;

  for (int i = 0; i < hand_set_list.size(); i++)
  {
    for (int j = 0; j < hand_set_list[i].getIsValid().size(); j++)
    {
      if (hand_set_list[i].getIsValid()(j))
      {
        hands.push_back(hand_set_list[i].getHypotheses()[j]);
      }
    }
  }

  plotFingers3D(hands, cloud, str, outer_diameter, finger_width, hand_depth, hand_height);
}

void displace_to_object_position()
{
  typedef Eigen::Transform<double, 3, Eigen::Affine> EigenTransform;
  Eigen::Quaterniond quat, quat_2;
  for(int i = 0; i < grasp_poses_without_energy.poses.size(); i++)
  {
    EigenTransform Global_Robot_Transform, Local_Robot_Transform, Final_Robot_Transform, Object_Transform, Robot_Transform_before_shifting, Robot_Transform_short_shifting;
    Global_Robot_Transform.setIdentity();Local_Robot_Transform.setIdentity();Object_Transform.setIdentity();
    Global_Robot_Transform.translate(Eigen::Vector3d(grasp_poses_without_energy.poses[i].pose.position.x,
      grasp_poses_without_energy.poses[i].pose.position.y,grasp_poses_without_energy.poses[i].pose.position.z));
    quat.w() = grasp_poses_without_energy.poses[i].pose.orientation.w;
    quat.x() = grasp_poses_without_energy.poses[i].pose.orientation.x;
    quat.y() = grasp_poses_without_energy.poses[i].pose.orientation.y;
    quat.z() = grasp_poses_without_energy.poses[i].pose.orientation.z;
    quat.normalize();
    Global_Robot_Transform.rotate(quat);
    Object_Transform.translate(Eigen::Vector3d(0.55,-0.2,0.6));
    quat.w() = 0.924;
    quat.x() = 0;
    quat.y() = 0;
    quat.z() = 0.383;
    quat.normalize();
    Object_Transform.rotate(quat);

    Local_Robot_Transform.translate(Eigen::Vector3d(-0.12,0.0,0.0));
    Robot_Transform_before_shifting = Object_Transform * Global_Robot_Transform;
    Final_Robot_Transform = Robot_Transform_before_shifting * Local_Robot_Transform;

    Local_Robot_Transform.setIdentity();
    Local_Robot_Transform.translate(Eigen::Vector3d(-0.06,0.0,0.0));
    Robot_Transform_short_shifting = Robot_Transform_before_shifting * Local_Robot_Transform;
    Eigen::Matrix3d rotation_matrix= Robot_Transform_before_shifting.rotation();
    Eigen::Quaterniond q_Hand_to_world(rotation_matrix);

    grasp_poses_without_energy.poses[i].pose.position.x = Robot_Transform_short_shifting.translation()[0];
    grasp_poses_without_energy.poses[i].pose.position.y = Robot_Transform_short_shifting.translation()[1];
    grasp_poses_without_energy.poses[i].pose.position.z = Robot_Transform_short_shifting.translation()[2];
    grasp_poses_without_energy.poses[i].pose.orientation.w = q_Hand_to_world.w();
    grasp_poses_without_energy.poses[i].pose.orientation.x = q_Hand_to_world.x();
    grasp_poses_without_energy.poses[i].pose.orientation.y = q_Hand_to_world.y();
    grasp_poses_without_energy.poses[i].pose.orientation.z = q_Hand_to_world.z();
  }
}


void Plot::plotFingers3D(const std::vector<Grasp>& hand_list, const PointCloudRGBA::Ptr& cloud,
  std::string str, double outer_diameter, double finger_width, double hand_depth, double hand_height) const
{
  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer(str);

  for (int i = 0; i < hand_list.size(); i++)
  {
    plotHand3D(viewer, hand_list[i], outer_diameter, finger_width, hand_depth, hand_height, i);
  }

  ros::NodeHandle n;
  ros::Publisher chatter_pub = n.advertise<gpg::grasp_poses>("gpg_grasp_poses", 1000);
  ros::Publisher chatter_pub_without_energy = n.advertise<cruzr_planner::Graspit_poses>("Final_Grasps_with_energy", 1000);
  ros::Rate loop_rate(10);
  std::cout<<"About to publish"<<'\n';
  displace_to_object_position();
  while(ros::ok())
  {
    chatter_pub.publish(grasp_poses);
    chatter_pub_without_energy.publish(grasp_poses_without_energy);
    ros::param::set("gpg_flag",true);
    ros::spinOnce();
    loop_rate.sleep();
  }

  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, rgb, "cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");

  runViewer(viewer);
}



void Plot::plotHand3D(boost::shared_ptr<pcl::visualization::PCLVisualizer>& viewer, const Grasp& hand,
  double outer_diameter, double finger_width, double hand_depth, double hand_height, int idx) const
{
  double hw = 0.5*outer_diameter;
  double base_depth = 0.02;
  double approach_depth = 0.07;

  Eigen::Vector3d left_bottom = hand.getGraspBottom() - (hw - 0.5 * finger_width) * hand.getBinormal();
  Eigen::Vector3d right_bottom = hand.getGraspBottom() + (hw - 0.5 * finger_width) * hand.getBinormal();
  Eigen::VectorXd left_center = left_bottom + 0.5 * hand_depth * hand.getApproach();
  Eigen::VectorXd right_center = right_bottom + 0.5 * hand_depth * hand.getApproach();
  Eigen::Vector3d base_center = left_bottom + 0.5 * (right_bottom - left_bottom) - 0.01 * hand.getApproach();
  Eigen::Vector3d approach_center = base_center - 0.04 * hand.getApproach();

  Eigen::Quaterniond quat(hand.getFrame());
  geometry_msgs::Pose hand_pose;
  hand_pose.position.x = base_center.x();
  hand_pose.position.y = base_center.y();
  hand_pose.position.z = base_center.z();
  hand_pose.orientation.x = quat.x();
  hand_pose.orientation.y = quat.y();
  hand_pose.orientation.z = quat.z();
  hand_pose.orientation.w = quat.w();

  grasp_poses.poses.push_back(hand_pose);

  cruzr_planner::Graspit_pose grasp_pose;
  grasp_pose.pose = hand_pose;
  grasp_pose.energy = 0;
  grasp_pose.legal = true;
  grasp_poses_without_energy.poses.push_back(grasp_pose);

  // quat.normalize();
  // Eigen::Quaterniond quat_rot,quat_new;
  // quat_rot.x() = 0.0;
  // quat_rot.y() = 0.0;
  // quat_rot.z() = 0.0;
  // quat_rot.w() = 1;
  //
  //
  // quat_new = quat_rot * quat;
  //
  // quat_new.normalize();
  // quat = quat_new;
  std::string num = boost::lexical_cast<std::string>(idx);

  // if(hand.isFullAntipodal() == true || hand.isHalfAntipodal() == true)
  std::ofstream file;
  // if(idx <= 1)
  {
    file.open("/home/suhail/catkin_ws/src/gpg/Grasps/gpg_grasps.txt", ios::out | ios::app);
    file.write((char*)&base_center,sizeof(base_center));
    file.write((char*)&quat,sizeof(quat));

    std::cout<<base_center.x()<<' '<<base_center.y()<<' '<<base_center.z()<<" "<<
    " Quaternion x y z w "<<quat.x()<<" "<<quat.y()<<" "<<quat.z()<<" "<<quat.w()<<" "<<'\n';

    plotCube(viewer, left_center, quat, hand_depth, finger_width, hand_height, "left_finger_" + num);
    plotCube(viewer, right_center, quat, hand_depth, finger_width, hand_height, "right_finger_" + num);
    plotCube(viewer, base_center, quat, base_depth, outer_diameter, hand_height, "base_" + num);
    plotCube(viewer, approach_center, quat, approach_depth, finger_width, 0.5*hand_height, "approach_" + num);

    file.close();
  }
}


void Plot::plotCube(boost::shared_ptr<pcl::visualization::PCLVisualizer>& viewer, const Eigen::Vector3d& position,
  const Eigen::Quaterniond& rotation, double width, double height, double depth, const std::string& name) const
{
  viewer->addCube(position.cast<float>(), rotation.cast<float>(), width, height, depth, name);
  viewer->setShapeRenderingProperties(pcl::visualization::PCL_VISUALIZER_REPRESENTATION, pcl::visualization::PCL_VISUALIZER_REPRESENTATION_SURFACE, name);
  viewer->setShapeRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0, 0.5, 0.5, name);
  viewer->setShapeRenderingProperties(pcl::visualization::PCL_VISUALIZER_OPACITY, 0.4, name);
}


void Plot::plotFingers(const std::vector<GraspSet>& hand_set_list, const PointCloudRGBA::Ptr& cloud,
  std::string str, double outer_diameter) const
{
  std::vector<Grasp> hands;

  for (int i = 0; i < hand_set_list.size(); i++)
  {
    for (int j = 0; j < hand_set_list[i].getIsValid().size(); j++)
    {
      if (hand_set_list[i].getIsValid()(j))
      {
        hands.push_back(hand_set_list[i].getHypotheses()[j]);
      }
    }
  }

  plotFingers(hands, cloud, str, outer_diameter);
}


void Plot::plotFingers(const std::vector<Grasp>& hand_list, const PointCloudRGBA::Ptr& cloud,
  std::string str, double outer_diameter) const
{
  const int WIDTH = pcl::visualization::PCL_VISUALIZER_LINE_WIDTH;

  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer(str);

  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, rgb, "cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "cloud");

  PointCloudRGBA::Ptr cloud_fingers(new PointCloudRGBA);
  cloud_fingers = createFingersCloud(hand_list, outer_diameter);

  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb_fingers(cloud_fingers);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud_fingers, rgb_fingers, "fingers");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5, "fingers");

  runViewer(viewer);
}


PointCloudRGBA::Ptr Plot::createFingersCloud(const std::vector<Grasp>& hand_list,
  double outer_diameter) const
{
  PointCloudRGBA::Ptr cloud_fingers(new PointCloudRGBA);

  for (int i = 0; i < hand_list.size(); i++)
  {
    pcl::PointXYZRGBA pc = eigenVector3dToPointXYZRGBA(hand_list[i].getGraspBottom());
    setPointColor(hand_list[i], pc);

    double width = outer_diameter;
    double hw = 0.5 * width;
    double step = hw / 30.0;
    Eigen::Vector3d left_bottom = hand_list[i].getGraspBottom() + hw * hand_list[i].getBinormal();
    Eigen::Vector3d right_bottom = hand_list[i].getGraspBottom() - hw * hand_list[i].getBinormal();
    pcl::PointXYZRGBA p1 = eigenVector3dToPointXYZRGBA(left_bottom);
    setPointColor(hand_list[i], p1);
    pcl::PointXYZRGBA p2 = eigenVector3dToPointXYZRGBA(right_bottom);
    setPointColor(hand_list[i], p2);
    cloud_fingers->points.push_back(pc);
    cloud_fingers->points.push_back(p1);
    cloud_fingers->points.push_back(p2);

    for (double j=step; j < hw; j+=step)
    {
      Eigen::Vector3d lb, rb, a;
      lb = hand_list[i].getGraspBottom() + j * hand_list[i].getBinormal();
      rb = hand_list[i].getGraspBottom() - j * hand_list[i].getBinormal();
      a = hand_list[i].getGraspBottom() - j * hand_list[i].getApproach();
      pcl::PointXYZRGBA plb = eigenVector3dToPointXYZRGBA(lb);
      setPointColor(hand_list[i], plb);
      pcl::PointXYZRGBA prb = eigenVector3dToPointXYZRGBA(rb);
      setPointColor(hand_list[i], prb);
      pcl::PointXYZRGBA pa = eigenVector3dToPointXYZRGBA(a);
      setPointColor(hand_list[i], pa);
      cloud_fingers->points.push_back(plb);
      cloud_fingers->points.push_back(prb);
      cloud_fingers->points.push_back(pa);
    }

    double dist = (hand_list[i].getGraspTop() - hand_list[i].getGraspBottom()).norm();
    step = dist / 40.0;
    for (double j=step; j < dist; j+=step)
    {
      Eigen::Vector3d lt, rt;
      lt = left_bottom + j * hand_list[i].getApproach();
      rt = right_bottom + j * hand_list[i].getApproach();
      pcl::PointXYZRGBA plt = eigenVector3dToPointXYZRGBA(lt);
      setPointColor(hand_list[i], plt);
      pcl::PointXYZRGBA prt = eigenVector3dToPointXYZRGBA(rt);
      setPointColor(hand_list[i], prt);
      cloud_fingers->points.push_back(plt);
      cloud_fingers->points.push_back(prt);
    }
  }

  return cloud_fingers;
}


void Plot::plotSamples(const std::vector<int>& index_list, const PointCloudRGBA::Ptr& cloud) const
{
  PointCloudRGBA::Ptr samples_cloud(new PointCloudRGBA);
  for (int i = 0; i < index_list.size(); i++)
    samples_cloud->points.push_back(cloud->points[index_list[i]]);

  plotSamples(samples_cloud, cloud);
}


void Plot::plotSamples(const Eigen::Matrix3Xd& samples, const PointCloudRGBA::Ptr& cloud) const
{
  PointCloudRGBA::Ptr samples_cloud(new PointCloudRGBA);
  for (int i = 0; i < samples.cols(); i++)
  {
    pcl::PointXYZRGBA p;
    p.x = samples.col(i)(0);
    p.y = samples.col(i)(1);
    p.z = samples.col(i)(2);
    samples_cloud->points.push_back(p);
  }

  plotSamples(samples_cloud, cloud);
}


void Plot::plotSamples(const PointCloudRGBA::Ptr& samples_cloud, const PointCloudRGBA::Ptr& cloud) const
{
  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Samples");

  // draw the point cloud
  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, rgb, "registered point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "registered point cloud");

  // draw the samples
  viewer->addPointCloud<pcl::PointXYZRGBA>(samples_cloud, "samples cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 4, "samples cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 0.0, 1.0, "samples cloud");

  runViewer(viewer);
}


void Plot::plotNormals(const CloudCamera& cloud_cam)
{
  const int num_clouds = cloud_cam.getViewPoints().cols();
  std::vector<PointCloudPointNormal::Ptr> clouds;
  clouds.resize(num_clouds);

  for (int i = 0; i < num_clouds; i++)
  {
    PointCloudPointNormal::Ptr cloud(new PointCloudPointNormal);
    clouds[i] = cloud;
  }

  for (int i = 0; i < cloud_cam.getNormals().cols(); i++)
  {
    pcl::PointNormal p;
    p.x = cloud_cam.getCloudProcessed()->points[i].x;
    p.y = cloud_cam.getCloudProcessed()->points[i].y;
    p.z = cloud_cam.getCloudProcessed()->points[i].z;
    p.normal_x = cloud_cam.getNormals()(0,i);
    p.normal_y = cloud_cam.getNormals()(1,i);
    p.normal_z = cloud_cam.getNormals()(2,i);

    for (int j = 0; j < cloud_cam.getCameraSource().rows(); j++)
    {
      if (cloud_cam.getCameraSource()(j,i) == 1)
      {
        clouds[j]->push_back(p);
      }
    }
  }

  double colors[6][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 1.0, 0.0}, {1.0, 0.0, 1.0},
    {0.0, 1.0, 1.0}};
  double normal_colors[6][3] = {{0.5, 0.0, 0.0}, {0.0, 0.5, 0.0}, {0.0, 0.0, 0.5}, {0.5, 0.5, 0.0}, {0.5, 0.0, 0.5},
    {0.0, 0.5, 0.5}};

  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Normals");
  viewer->setBackgroundColor(0.1, 0.1, 0.1);
  for (int i = 0; i < num_clouds; i++)
  {
    std::string cloud_name = "cloud_" + boost::lexical_cast<std::string>(i);
    std::string normals_name = "normals_" + boost::lexical_cast<std::string>(i);
    int color_id = i % 6;
    viewer->addPointCloud<pcl::PointNormal>(clouds[i], cloud_name);
    viewer->addPointCloudNormals<pcl::PointNormal>(clouds[i], 1, 0.01, normals_name);
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, colors[color_id][0],
      colors[color_id][1], colors[color_id][2], cloud_name);
    viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, normal_colors[color_id][0],
      normal_colors[color_id][1], normal_colors[color_id][2], normals_name);

    // draw camera position as a cube
    const Eigen::Vector3d& cam_pos = cloud_cam.getViewPoints().col(i);
    Eigen::Vector4f centroid_4d;
    pcl::compute3DCentroid(*clouds[i], centroid_4d);
    Eigen::Vector3d centroid;
    centroid << centroid_4d(0), centroid_4d(1), centroid_4d(2);
    Eigen::Vector3d cone_dir = centroid - cam_pos;
    cone_dir.normalize();
    pcl::ModelCoefficients coeffs;
    coeffs.values.push_back(cam_pos(0));
    coeffs.values.push_back(cam_pos(1));
    coeffs.values.push_back(cam_pos(2));
    coeffs.values.push_back(cone_dir(0));
    coeffs.values.push_back(cone_dir(1));
    coeffs.values.push_back(cone_dir(2));
    coeffs.values.push_back(20.0);
    std::string cone_name = "cam" + boost::lexical_cast<std::string>(i);
    viewer->addCone(coeffs, cone_name, 0);
    viewer->setShapeRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, normal_colors[color_id][0],
      normal_colors[color_id][1], normal_colors[color_id][2], cone_name);
  }

  runViewer(viewer);
}


void Plot::plotNormals(const PointCloudRGBA::Ptr& cloud, const PointCloudRGBA::Ptr& cloud_samples, const Eigen::Matrix3Xd& normals) const
{
  PointCloudPointNormal::Ptr normals_cloud(new PointCloudPointNormal);
  for (int i=0; i < normals.cols(); i++)
  {
    pcl::PointNormal p;
    p.x = cloud_samples->points[i].x;
    p.y = cloud_samples->points[i].y;
    p.z = cloud_samples->points[i].z;
    p.normal_x = normals(0,i);
    p.normal_y = normals(1,i);
    p.normal_z = normals(2,i);
    normals_cloud->points.push_back(p);
  }
  std::cout << "Drawing " << normals_cloud->size() << " normals\n";

  double red[3] = {1.0, 0.0, 0.0};
  double blue[3] = {0.0, 0.0, 1.0};

  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Normals");

  // draw the point cloud
  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, rgb, "registered point cloud");

  // draw the normals
  addCloudNormalsToViewer(viewer, normals_cloud, 2, blue, red, std::string("cloud"), std::string("normals"));

  runViewer(viewer);
}


void Plot::plotNormals(const PointCloudRGBA::Ptr& cloud, const Eigen::Matrix3Xd& normals) const
{
  PointCloudPointNormal::Ptr normals_cloud(new PointCloudPointNormal);
  for (int i=0; i < normals.cols(); i++)
  {
    pcl::PointNormal p;
    p.x = cloud->points[i].x;
    p.y = cloud->points[i].y;
    p.z = cloud->points[i].z;
    p.normal_x = normals(0,i);
    p.normal_y = normals(1,i);
    p.normal_z = normals(2,i);
    normals_cloud->points.push_back(p);
  }
  std::cout << "Drawing " << normals_cloud->size() << " normals\n";

  double red[3] = {1.0, 0.0, 0.0};
  double blue[3] = {0.0, 0.0, 1.0};

  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Normals");

  // draw the point cloud
  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, rgb, "registered point cloud");

  // draw the normals
  addCloudNormalsToViewer(viewer, normals_cloud, 2, blue, red, std::string("cloud"), std::string("normals"));

  runViewer(viewer);
}


void Plot::plotNormals(const Eigen::Matrix3Xd& pts, const Eigen::Matrix3Xd& normals) const
{
  PointCloudPointNormal::Ptr normals_cloud(new PointCloudPointNormal);
  for (int i=0; i < normals.cols(); i++)
  {
    pcl::PointNormal p;
    p.x = pts(0,i);
    p.y = pts(1,i);
    p.z = pts(2,i);
    p.normal_x = normals(0,i);
    p.normal_y = normals(1,i);
    p.normal_z = normals(2,i);
    normals_cloud->points.push_back(p);
  }
  std::cout << "Drawing " << normals_cloud->size() << " normals\n";

  double red[3] = {1.0, 0.0, 0.0};
  double blue[3] = {0.0, 0.0, 1.0};

  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Normals");
  addCloudNormalsToViewer(viewer, normals_cloud, 2, blue, red, std::string("cloud"), std::string("normals"));
  runViewer(viewer);
}


void Plot::plotLocalAxes(const std::vector<LocalFrame>& quadric_list, const PointCloudRGBA::Ptr& cloud) const
{
  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Local Axes");
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud, "registered point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1,
    "registered point cloud");

  for (int i = 0; i < quadric_list.size(); i++)
    quadric_list[i].plotAxes((void*) &viewer, i);

  runViewer(viewer);
}


void Plot::plotCameraSource(const Eigen::VectorXi& pts_cam_source_in, const PointCloudRGBA::Ptr& cloud) const
{
  PointCloudRGBA::Ptr left_cloud(new PointCloudRGBA);
  PointCloudRGBA::Ptr right_cloud(new PointCloudRGBA);

  for (int i = 0; i < pts_cam_source_in.size(); i++)
  {
    if (pts_cam_source_in(i) == 0)
      left_cloud->points.push_back(cloud->points[i]);
    else if (pts_cam_source_in(i) == 1)
      right_cloud->points.push_back(cloud->points[i]);
  }

  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer("Camera Sources");
  viewer->addPointCloud<pcl::PointXYZRGBA>(left_cloud, "left point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1,
    "left point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 1.0, 0.0, 0.0,
    "left point cloud");
  viewer->addPointCloud<pcl::PointXYZRGBA>(right_cloud, "right point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1,
    "right point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, 0.0, 1.0, 0.0,
    "right point cloud");
  runViewer(viewer);
}


void Plot::addCloudNormalsToViewer(boost::shared_ptr<pcl::visualization::PCLVisualizer>& viewer,
  const PointCloudPointNormal::Ptr& cloud, double line_width, double* color_cloud,
  double* color_normals, const std::string& cloud_name, const std::string& normals_name) const
{
  viewer->addPointCloud<pcl::PointNormal>(cloud, cloud_name);
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, color_cloud[0],
    color_cloud[1], color_cloud[2], cloud_name);
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 6,
    cloud_name);
  viewer->addPointCloudNormals<pcl::PointNormal>(cloud, 1, 0.01, normals_name);
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_LINE_WIDTH, line_width,
    normals_name);
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_COLOR, color_normals[0],
    color_normals[1], color_normals[2], normals_name);
}


void Plot::runViewer(boost::shared_ptr<pcl::visualization::PCLVisualizer>& viewer) const
{
  while (!viewer->wasStopped())
  {
    viewer->spinOnce(100);
    boost::this_thread::sleep(boost::posix_time::microseconds(100000));
  }

  viewer->close();
}


boost::shared_ptr<pcl::visualization::PCLVisualizer> Plot::createViewer(std::string title) const
{
  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer(title));
  viewer->setPosition(0, 0);
  viewer->setSize(640, 480);
  viewer->setBackgroundColor(1.0, 1.0, 1.0);

  pcl::visualization::Camera camera;
  camera.clip[0] = 0.00130783;
  camera.clip[1] = 1.30783;
  camera.focal[0] = 0.776838;
  camera.focal[1] = -0.095644;
  camera.focal[2] = -0.18991;
  camera.pos[0] = 0.439149;
  camera.pos[1] = -0.10342;
  camera.pos[2] = 0.111626;
  camera.view[0] = 0.666149;
  camera.view[1] = -0.0276846;
  camera.view[2] = 0.745305;
  camera.fovy = 0.8575;
  camera.window_pos[0] = 0;
  camera.window_pos[1] = 0;
  camera.window_size[0] = 640;
  camera.window_size[1] = 480;
  viewer->setCameraParameters(camera);

  return viewer;
}


void Plot::plotCloud(const PointCloudRGBA::Ptr& cloud_rgb, const std::string& title) const
{
  boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer = createViewer(title);
  pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBA> rgb(cloud_rgb);
  viewer->addPointCloud<pcl::PointXYZRGBA>(cloud_rgb, rgb, "registered point cloud");
  viewer->setPointCloudRenderingProperties(pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "registered point cloud");
  runViewer(viewer);
}


pcl::PointXYZRGBA Plot::eigenVector3dToPointXYZRGBA(const Eigen::Vector3d& v) const
{
  pcl::PointXYZRGBA p;
  p.x = v(0);
  p.y = v(1);
  p.z = v(2);
  return p;
}


void Plot::setPointColor(const Grasp& hand, pcl::PointXYZRGBA& p) const
{
  p.a = 128;

  if (hand.isFullAntipodal())
  {
    p.r = 0;
    p.g = 255;
    p.b = 0;
  }
  else
  {
    p.r = 255;
    p.g = 0;
    p.b = 0;
  }
}
