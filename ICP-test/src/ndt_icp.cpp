#include <iostream>
#include <pcl/io/ply_io.h>
#include <pcl/io/vtk_lib_io.h>
#include <pcl/point_types.h>
#include <pcl/registration/icp.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/filter_indices.h>
#include <pcl/common/transforms.h>
#include <pcl/registration/gicp.h>
#include <pcl/registration/ndt.h>

#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkCamera.h>


void
loadFile(const char* fileName,
	 pcl::PointCloud<pcl::PointXYZ> &cloud
)
{
  pcl::PolygonMesh mesh;
  
  if ( pcl::io::loadPolygonFile ( fileName, mesh ) == -1 )
  {
    PCL_ERROR ( "loadFile faild." );
    return;
  }
  else
    pcl::fromPCLPointCloud2<pcl::PointXYZ> ( mesh.cloud, cloud );
  
  // remove points having values of nan
  std::vector<int> index;
  pcl::removeNaNFromPointCloud ( cloud, cloud, index );
}


int main ( int argc, char** argv )
{
  
  
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source ( new pcl::PointCloud<pcl::PointXYZ> () );
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_target ( new pcl::PointCloud<pcl::PointXYZ> () );
  
  {
    // load source
    loadFile ( argv[1], *cloud_source );
    // load target
    loadFile ( argv[2], *cloud_target );
  }


  // transformed source ---> target
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_source_trans ( new pcl::PointCloud<pcl::PointXYZ> () );
  Eigen::Matrix4f T;

  { // ICP registration
   // ndt匹配
    pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> icp;
    icp.setTransformationEpsilon(1e-5);
    icp.setStepSize(0.5);          //为More-Thuente线搜索设置最大步长
    icp.setResolution(0.1);        //设置NDT网格结构的分辨率（VoxelGridCovariance）
    icp.setMaximumIterations(100); //设置匹配迭代的最大次数

    //pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
    
    icp.setInputSource ( cloud_source );
    icp.setInputTarget ( cloud_target );
    
    // registration
    icp.align ( *cloud_source_trans );
    T = icp.getFinalTransformation();
    
    if ( icp.hasConverged() )
    {
      std::cout << "Converged. score =" << icp.getFitnessScore() << std::endl;
      
      Eigen::Matrix4f transformation = icp.getFinalTransformation();
      std::cout << transformation << std::endl;
    }
    else
      std::cout << "Not converged." << std::endl;

  }
  
  
  pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_aligned ( new pcl::PointCloud<pcl::PointXYZ> () );
  pcl::transformPointCloud(*cloud_source, *cloud_aligned, T);
  
  { // visualization
    boost::shared_ptr< pcl::visualization::PCLVisualizer > viewer ( new pcl::visualization::PCLVisualizer ("3D Viewer") );
    viewer->setBackgroundColor (0, 0, 0);
    
   
    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> source_color ( cloud_source, 0, 255, 0 );
    viewer->addPointCloud<pcl::PointXYZ> (cloud_source, source_color, "source");
    viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, "source");

    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> target_color ( cloud_target, 255, 255, 255 );
    viewer->addPointCloud<pcl::PointXYZ> ( cloud_target, target_color, "target");
    viewer->setPointCloudRenderingProperties ( pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, "target" );
  
    pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZ> source_trans_color ( cloud_source_trans, 255, 0, 255 );
    viewer->addPointCloud<pcl::PointXYZ> ( cloud_aligned, source_trans_color, "source_trans" );
    viewer->setPointCloudRenderingProperties ( pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 2, "source_trans" );

    viewer->getRenderWindow()->GetRenderers()->GetFirstRenderer()->GetActiveCamera()->SetParallelProjection( 1 );
    viewer->resetCamera();

    viewer->spin();

    
  }
  
  return(0);
}

