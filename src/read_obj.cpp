#include <iostream>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/io/obj_io.h>
int
main (int argc, char** argv)
{
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);
  pcl::OBJReader reader;
  reader.read("/home/suhail/catkin_ws/src/gpg/PCL/Gas_Can/15222_Gas_Can_v1_NEW.obj", *cloud);
  // if (pcl::OBJReader::read<pcl::PointXYZ> ("/home/suhail/catkin_ws/src/gpg/PCL/002_master_chef_can_2/clouds/outfilefile.pcd", *cloud)) //* load the file
  // {
  //   PCL_ERROR ("Couldn't read file test_pcd.pcd \n");
  //   return (-1);
  // }
  std::cout << "Loaded "
            << cloud->width * cloud->height
            << " data points from test_pcd.pcd with the following fields: "
            << std::endl;
  for (size_t i = 0; i < cloud->points.size (); ++i)
    {
      cloud->points[i].x *=20;
      cloud->points[i].y *=20;
      cloud->points[i].z *=20;
    }
    pcl::io::savePCDFileASCII ("/home/suhail/catkin_ws/src/gpg/PCL/Gas_Can/15222_Gas_Can_v1_NEW.pcd", *cloud);

              // << " "    << cloud->points[i].y
              // << " "    << cloud->points[i].z << std::endl;

  return (0);
}
