#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>

#include <queue>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <ros/ros.h>
#include "sensor_msgs/Imu.h"
#include "std_msgs/String.h"
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <opencv2/opencv.hpp>

#include "device.h"

using namespace std;
using namespace sn;
using namespace cv;


#define CFG_SYS_REG_NUM 32
#define L_SENSOR_CFG_BASE_ADDR      0x10000
#define R_SENSOR_CFG_BASE_ADDR      0x20000

#define L_REMAP_CFG_BASE_ADDR       0x30000
#define R_REMAP_CFG_BASE_ADDR       0x40000



#define L_PRE_REMAP_BASE_ADDR       0x00000000
#define R_PRE_REMAP_BASE_ADDR       0x00400000
#define L_POST_REMAP_BASE_ADDR      0x00800000
#define R_POST_REMAP_BASE_ADDR      0x00C00000
#define L_REMAP_PARAMETER_BASE_ADDR 0x01400000
#define R_REMAP_PARAMETER_BASE_ADDR 0x01C00000
#define DISPARITY_IMG_BASE_ADDR     0x04000000
#define SGM_LRPD_BASE_ADDR          0x05000000
#define TEST_0_BASE_ADDR            0x07000000
#define TEST_1_BASE_ADDR            0x07400000


//#define ONEYEAR (ros::Time(0) + ros::Duration(365*24*60*60))
#define BASETIME (ros::Time(0) + ros::Duration(10000000))


double fc = 0.0;
double tc = 0.0;
int mulVal = 16;



typedef struct{
  double timestamp;
  Mat left;
  Mat right;
}imageData;


std::mutex m_buf,i_buf;
queue<imuData> imu_buf;
queue<imageData> image_buf;

ros::Publisher imu_pub;
image_transport::Publisher pub0;
image_transport::Publisher pub1;

void registerPub(ros::NodeHandle &n)
{
  imu_pub = n.advertise<sensor_msgs::Imu>("imu0", 1000);
  image_transport::ImageTransport it(n);
  pub0 = it.advertise("cam0/image_raw", 1);
  pub1 = it.advertise("cam1/image_raw", 1);
}

int main(int argc, char** argv)
{
  //1.ros init
  ros::init(argc, argv, "image_publisher");
  ros::NodeHandle nh;

  registerPub(nh);
    

  int interval = 1;
  double imu_timeoffset = 0;
  if(argc == 2)
    interval = atoi(argv[1]);
  else if(argc == 3) {
    interval = atoi(argv[1]);
    imu_timeoffset = atof(argv[2]);
  }
  
  //2.open device
  device *cam = new device();
  resolution rn = cam->getImageSize();
  int width = rn.width;
  int height = rn.height;

  cv::Size imageSize = cv::Size(width, height);
  if (!cam->init()) {
    return 0;
  }

  //3.device settings

  //
  //sensor reg list
  string regAddr = "/home/sst/catkin_ws/src/sensors_ros/fisheye7251/config/ov7251_registers_addrs_640x480.coe";
  string regData = "/home/sst/catkin_ws/src/sensors_ros/fisheye7251/config/ov7251_registers_datas_640x480.coe";
  if (!cam->setup(regAddr, regData)) {
    return 0;
  }  

#if 0
  //zengyi                                                                                     
  cam->sendCmd(0x350b,0x40,L_SENSOR_CFG_BASE_ADDR);
  cam->sendCmd(0x350b,0x40,R_SENSOR_CFG_BASE_ADDR);

  //baoguang                                                                                   
  double expTime = 5000.0f / 19.3333333;
  uint reg3501 = (uint)(expTime / 16);
  uint reg3502 = (uint)((expTime - reg3501) * 16);

  cam->sendCmd(0x3501, reg3501, 0x10000, 0x00);
  cam->sendCmd(0x3502, reg3502, 0x10000, 0x00);
  cam->sendCmd(0x3501, reg3501, 0x20000, 0x00);
  cam->sendCmd(0x3502, reg3502, 0x20000, 0x00);
#endif

  // imu配置
  cam->sendCmd(0x1FD, 0x1);
  //cam->sendCmd(0x102, 2*100000);//2ms,500hz?
  cam->sendCmd(0x102, 5*100000);//5ms,200hz

  //
  uint sys_reg_cfg[CFG_SYS_REG_NUM][2] = 
    {
      {0x000,L_PRE_REMAP_BASE_ADDR },
      {0x004,R_PRE_REMAP_BASE_ADDR },
      {0x008,L_POST_REMAP_BASE_ADDR},
      {0x00C,R_POST_REMAP_BASE_ADDR},
      {0x010,L_REMAP_PARAMETER_BASE_ADDR},
      {0x014,R_REMAP_PARAMETER_BASE_ADDR},
      {0x018,DISPARITY_IMG_BASE_ADDR}, 
      {0x01C,SGM_LRPD_BASE_ADDR}, 
      {0x020,TEST_0_BASE_ADDR}, 
      {0x024,TEST_1_BASE_ADDR}, 

      //{0x100,   1666667}, //30hz
      //{0x100,   1400000}, //35hz
      {0x100, 5000000}, //20fps
      //{0x100, 2000000}, //50fps
      //{0x100, 4000000}, //50fps
      //{0x100, 1400000}, //17.6fps
      {0x104,  200000}, /* Exposuer time ,unit=10ns */
      {0x108,  0x199a}, /* Filter err threshold */
      {0x10C,    8192}, /* Curve fit fact */
      {0x110,      63}, /* btcost sobel threshold */
      {0x114,       1}, /* Dlx_sub_drx_check_threshold */
      {0x118,      24}, /* sgm_p1 */
      {0x11C,      96}, /* sgm_p2 */
      {0x120,       0}, /* test_en */
      {0x124,    0x01}, /* image_display_en */  //0x11
      {0x128,     200}, /* display threshold row num*/
      {0x12C,      30}, /* uniqueness_check_factor */
      {0x1FC,       1}  /* sensor dout enable */
    };

  for(int i=0; i<CFG_SYS_REG_NUM; i++) {
    cam->sendCmd(sys_reg_cfg[i][0],sys_reg_cfg[i][1],0x0);
    //cv::waitKey(5);
  }


  cam->sendCmd(0x100,0x00,0,1);
    
  //start capture image
  cam->start();

  Mat remapLeft = Mat(Size(width, height), CV_16UC1);
  Mat remapRight = remapLeft.clone();

  Mat left8u = Mat(Size(width, height), CV_8UC1);
  Mat right8u = left8u.clone();

  Mat stereoFrame(Size(width*2, height), CV_8UC1);

  //data process
  //std::thread imu_process{ImuProcess};
  //std::thread image_process{ImageProcess};
    
  uint64 imgTimeStamp = 0;  
  imuData imudata;
  imageData imagedata;

  double lastimu_time,lastimage_time;

  double accS = 0.000488281 * 9.8; // m/s^2
  double gyrS = 0.06103515625*3.141592653/180.0; // rad/s


#if 0
  //imu数据预处理,算先验的偏置
  int n = 100,count= 0;
  int offset_acc[3] = { 0, 0, 0};
  int offset_gyr[3] = { 0, 0, 0};
  while(nh.ok()) {
      
    if(cam->readIMU(&imudata)) {
      if(n-- > 0)
	continue;
      count++;
      ROS_INFO("%d ,%d, %d, %d, %d, %d,   norm %.f, mean %d, %d ,%d, %d, %d, %d",imudata.gyr_x, imudata.gyr_y,imudata.gyr_z,
	       imudata.accel_x, imudata.accel_y,imudata.accel_z,
	       sqrt(pow(imudata.accel_x,2)+pow(imudata.accel_y,2)+pow(imudata.accel_z,2)),
	       offset_gyr[0] / count,
	       offset_gyr[1] / count,
	       offset_gyr[2] / count,
	       offset_acc[0] / count,
	       offset_acc[1] / count,
	       offset_acc[2] / count );

      	int w[] = {imudata.gyr_x, imudata.gyr_y, imudata.gyr_z};
	int a[] = {imudata.accel_x, imudata.accel_y, imudata.accel_z};
	for(int i = 0;i<3; i++) {
	  offset_acc[i] += a[i];   //加速度受重力影响，这里先不管了
	  offset_gyr[i] += w[i];
	}
    }
  }
#endif
  /*ROS_INFO("count %d mean offset:%d ,%d, %d, %d, %d, %d",count,offset_gyr[0], offset_gyr[1], offset_gyr[2],
	   offset_acc[0],offset_acc[1],offset_acc[2]);*/

  //先验bias
  double bias_acc[] = {1.7, 37.9, -152.7};     //matlab拟合的，重力2049
  double bias_gyr[] = {-27, 17, 3};

  //50t,60度镜头(-28 19 0 -41.5 52 -179)
  //double bias_acc[] = {-41.5, 52, -179};
  //double bias_gyr[] = {-28, 19, 0};
  //double bias_gyr[] = {-27, 19, -11};
  
  int aa = 0;	
  while (nh.ok()) {
    if(cam->readIMU(&imudata)){
      ros::Time timestamp = BASETIME + ros::Duration((double)imudata.imu_timestamp/100000000);
      timestamp = timestamp + ros::Duration(imu_timeoffset); //硬件不同步时，需要手动给imu加time offset
      //printf("imu  :%lf\n",timestamp.toSec());
      lastimu_time = timestamp.toSec();
      sensor_msgs::Imu imu_msg;
      imu_msg.header.stamp = timestamp;
      imu_msg.angular_velocity.x = (double)(imudata.gyr_x - bias_gyr[0]) * gyrS;
      imu_msg.angular_velocity.y = (double)(imudata.gyr_y - bias_gyr[1]) * gyrS;
      imu_msg.angular_velocity.z = (double)(imudata.gyr_z - bias_gyr[2]) * gyrS;
      imu_msg.linear_acceleration.x = (double)(imudata.accel_x - bias_acc[0]) * accS;
      imu_msg.linear_acceleration.y = (double)(imudata.accel_y - bias_acc[1]) * accS;
      imu_msg.linear_acceleration.z = (double)(imudata.accel_z - bias_acc[2]) * accS;

      //raw data
#if 1
      printf("%f, %4d, %4d, %4d, %4d, %4d, %4d\n",timestamp.toSec(),
	     imudata.gyr_x - (short)bias_gyr[0],
	     imudata.gyr_y - (short)bias_gyr[1],
	     imudata.gyr_z - (short)bias_gyr[2],
	     imudata.accel_x - (short)bias_acc[0],
	     imudata.accel_y - (short)bias_acc[1],
	     imudata.accel_z - (short)bias_acc[2]);
#endif

      //转换后的数据
#if 1
      double norm = sqrt(pow(imu_msg.linear_acceleration.x,2)+pow(imu_msg.linear_acceleration.y,2)+pow(imu_msg.linear_acceleration.z,2));
      printf("%-7.3f, %-7.3f, %-7.3f, %-7.3f, %-7.3f, %-7.3f, %-7.3f,    norm:%-7.3f\n",timestamp.toSec(),
	     imu_msg.angular_velocity.x,
	     imu_msg.angular_velocity.y,
	     imu_msg.angular_velocity.z,
	     imu_msg.linear_acceleration.x,
	     imu_msg.linear_acceleration.y,
	     imu_msg.linear_acceleration.z,
	     norm);
#endif

      imu_pub.publish(imu_msg);
    }


    //get frame
    if (cam->QueryFrame(remapLeft, remapRight, imgTimeStamp)) {

      //隔多少帧发送一次
      if (aa++ % interval != 0)
	continue;

      ros::Time timestamp = BASETIME + ros::Duration((double)imgTimeStamp/100000000);
      //printf("image  :%lf\n",timestamp.toSec());
      lastimage_time = timestamp.toSec();
      
      convertScaleAbs(remapLeft, left8u, 255 / 1023.0);
      convertScaleAbs(remapRight, right8u, 255 / 1023.0);

      cv_bridge::CvImage cvi0, cvi1;
      cvi0.header.stamp =  timestamp;
      cvi0.header.frame_id = "image0";
      cvi0.encoding = "mono8";
      cvi0.image = left8u;

      cvi1.header.stamp =  cvi0.header.stamp;
      cvi1.header.frame_id = "image1";
      cvi1.encoding = "mono8";
      cvi1.image = right8u;

      sensor_msgs::Image msg0, msg1;
      cvi0.toImageMsg(msg0);
      cvi1.toImageMsg(msg1);
      
      pub0.publish(msg0);
      pub1.publish(msg1);
    }


    ros::spinOnce();
  }



  cam->close();

  return 0;
}