#include <limits>
#include <type_traits>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/features/normal_3d.h>
#include <pcl/features/fpfh.h>
#include <pcl/registration/registration.h>
#include <pcl/registration/sample_consensus_prerejective.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/voxel_grid.h>

#include <pcl/keypoints/harris_3d.h>

#include <pcl/kdtree/impl/kdtree_flann.hpp>

// #include <boost/timer.hpp>
#include <yaml-cpp/yaml.h>

pcl::StopWatch timer;
double duration;

//convenient typedefs
typedef pcl::PointXYZ PointT;
// typedef pcl::PointXYZI KeyPointT;
typedef PointT KeyPointT;
typedef pcl::PointCloud<PointT> PointCloud;

//our visualizer
pcl::visualization::PCLVisualizer *p_viz;
//its left and right viewports
int vp_1, vp_2;

void showCloudsLeft(const PointCloud::Ptr cloud_target, const PointCloud::Ptr cloud_source)
{
	// p_viz->removePointCloud("vp1_target");
	// p_viz->removePointCloud("vp1_source");

	if (std::is_same<PointT, pcl::PointXYZ>::value) {
		 pcl::visualization::PointCloudColorHandlerCustom<PointT> tgt_h(cloud_target, 0, 255, 0);
		 pcl::visualization::PointCloudColorHandlerCustom<PointT> src_h(cloud_source, 255, 0, 0);
		 p_viz->addPointCloud(cloud_target, tgt_h, "vp1_target", vp_1);
		 p_viz->addPointCloud(cloud_source, src_h, "vp1_source", vp_1);
	}
	if (std::is_same<PointT, pcl::PointXYZRGB>::value) {
		pcl::visualization::PointCloudColorHandlerRGBField<PointT> tgt_h(cloud_target);
		pcl::visualization::PointCloudColorHandlerRGBField<PointT> src_h(cloud_source);
		p_viz->addPointCloud(cloud_target, tgt_h, "vp1_target", vp_1);
		p_viz->addPointCloud(cloud_source, src_h, "vp1_source", vp_1);
	}
	
	//PCL_INFO("Press q to continue.\n");
	//p_viz->spin();

	//p_viz->addCoordinateSystem(0.1);
	p_viz->initCameraParameters();

	PCL_INFO("Press q to begin the registration.\n");
	p_viz->spin();
}

void showCloudsRight(const PointCloud::Ptr cloud_target, const PointCloud::Ptr cloud_source)
{
	if (std::is_same<PointT, pcl::PointXYZ>::value) {
		 pcl::visualization::PointCloudColorHandlerCustom<PointT> tgt_h(cloud_target, 0, 255, 0);
		 pcl::visualization::PointCloudColorHandlerCustom<PointT> src_h(cloud_source, 255, 0, 0);
		 p_viz->addPointCloud(cloud_target, tgt_h, "vp2_target", vp_2);
		 p_viz->addPointCloud(cloud_source, src_h, "vp2_source", vp_2);
	}

	if (std::is_same<PointT, pcl::PointXYZRGB>::value) {
		pcl::visualization::PointCloudColorHandlerRGBField<PointT> tgt_h(cloud_target);
		pcl::visualization::PointCloudColorHandlerRGBField<PointT> src_h(cloud_source);
		p_viz->addPointCloud(cloud_target, tgt_h, "vp2_target", vp_2);
		p_viz->addPointCloud(cloud_source, src_h, "vp2_source", vp_2);
	}

	//p_viz->addCoordinateSystem(0.1);

	PCL_INFO("Press q to continue.\n");
	p_viz->spin();

	p_viz->removePointCloud("vp2_target");
	p_viz->removePointCloud("vp2_source");
}

int main(int argc, char **argv)
{
	pcl::console::setVerbosityLevel(pcl::console::L_DEBUG); // show debug messages
	// Load config file
	YAML::Node lconf = YAML::LoadFile("config/scp_config.yaml");
	// set parameters
	float norm_est_RadiusSearch = lconf["norm_est_RadiusSearch"].as<float>();
	float fpfh_est_RadiusSearch = lconf["fpfh_est_RadiusSearch"].as<float>();

	float scp_max_corr_distance = lconf["scp_max_corr_distance"].as<float>();
	int scp_max_iter = lconf["scp_max_iter"].as<int>();
	float scp_sim_threshold = lconf["scp_sim_threshold"].as<float>();
	int scp_corr_randomness = lconf["scp_corr_randomness"].as<int>();
	float scp_inlier_fraction = lconf["scp_inlier_fraction"].as<float>();
	int scp_number_of_samples = lconf["scp_number_of_samples"].as<int>();

	float grid_filter_leafsize = lconf["grid_filter_leafsize"].as<float>();

	PointCloud cloud_source, cloud_target, cloud_reg;

	// load PCD file
	std::string src_fname = lconf["src_pcd"].as<std::string>();
	std::cout << "source: " << src_fname << std::endl;
	pcl::io::loadPCDFile(src_fname, cloud_source);

	std::string target_fname = lconf["target_pcd"].as<std::string>();
	std::cout << "target: " << target_fname << std::endl;
	pcl::io::loadPCDFile(target_fname, cloud_target);

	// Create a PCLVisualizer object
	p_viz = new pcl::visualization::PCLVisualizer(argc, argv, "SCP Test");
	p_viz->createViewPort(0.0, 0, 0.5, 1.0, vp_1);
	p_viz->createViewPort(0.5, 0, 1.0, 1.0, vp_2);

	//remove NAN points from the cloud
	std::vector<int> indices_source, indices_target;
	pcl::removeNaNFromPointCloud(cloud_source, cloud_source, indices_source);
	pcl::removeNaNFromPointCloud(cloud_target, cloud_target, indices_target);

	// Transform the source cloud by a large amount
	 //Eigen::Vector3f initial_offset(0.05, 0.02, 0);
	 //float angle = 0.0*static_cast<float>(M_PI) / 4.0f;
	 //Eigen::Quaternionf initial_rotation(cos(angle / 2), 0, 0, sin(angle / 2));
	 //PointCloud cloud_source_transformed;
	 //transformPointCloud(cloud_source, cloud_target, initial_offset, initial_rotation);

	// cloud_target = cloud_source;
	// Create shared pointers
	PointCloud::Ptr cloud_source_ptr, cloud_target_ptr, cloud_reg_ptr;
	cloud_source_ptr = cloud_source.makeShared();
	cloud_target_ptr = cloud_target.makeShared();
	cloud_reg_ptr = cloud_reg.makeShared();

	pcl::PointCloud<KeyPointT>::Ptr source_keypoints_ptr(new pcl::PointCloud<KeyPointT>);
	pcl::PointCloud<KeyPointT>::Ptr target_keypoints_ptr(new pcl::PointCloud<KeyPointT>);
	pcl::PointCloud<KeyPointT>::Ptr reg_keypoints_ptr(new pcl::PointCloud<KeyPointT>);

	// Apply grid filter, downsampling
	pcl::VoxelGrid<PointT> grid;
	grid.setLeafSize(grid_filter_leafsize, grid_filter_leafsize, grid_filter_leafsize);
	grid.setInputCloud(cloud_source_ptr);
	grid.filter(*source_keypoints_ptr);
	grid.setInputCloud(cloud_target_ptr);
	grid.filter(*target_keypoints_ptr);

	std::cout << "source size: " << (*cloud_source_ptr).size() << std::endl;
	std::cout << "source keypoints size: " << (*source_keypoints_ptr).size() << std::endl;
	std::cout << "target size: " << (*cloud_target_ptr).size() << std::endl;
	std::cout << "target keypoints size: " << (*target_keypoints_ptr).size() << std::endl;

	showCloudsLeft(cloud_source_ptr, cloud_target_ptr);

	// Initialize estimators for surface normals and FPFH features
	pcl::search::KdTree<KeyPointT>::Ptr tree(new pcl::search::KdTree<KeyPointT>);

	// Normal estimator
	pcl::NormalEstimation<KeyPointT, pcl::Normal> norm_est;
	norm_est.setSearchMethod(tree);
	norm_est.setRadiusSearch(norm_est_RadiusSearch);
	pcl::PointCloud<pcl::Normal> normals_source, normals_target;

	// FPFH estimator
	pcl::FPFHEstimation<KeyPointT, pcl::Normal, pcl::FPFHSignature33> fpfh_est;
	fpfh_est.setSearchMethod(tree);
	fpfh_est.setRadiusSearch(fpfh_est_RadiusSearch);
	pcl::PointCloud<pcl::FPFHSignature33> features_source, features_target;

	// Estimate the normals and the FPFH features for the source cloud
	timer.reset();
	norm_est.setInputCloud(source_keypoints_ptr);
	norm_est.compute(normals_source);
	duration = timer.getTimeSeconds();
	std::cout << "Normal estimator: " << duration << "s" << std::endl;

	timer.reset();
	fpfh_est.setInputCloud(source_keypoints_ptr);
	fpfh_est.setInputNormals(normals_source.makeShared());
	fpfh_est.compute(features_source);
	duration = timer.getTimeSeconds();
	std::cout << "FPFH estimator: " << duration << "s" << std::endl;

	// Estimate the normals and the FPFH features for the target cloud
	timer.reset();
	norm_est.setInputCloud(target_keypoints_ptr);
	norm_est.compute(normals_target);
	duration = timer.getTimeSeconds();
	std::cout << "Normal estimator: " << duration << "s" << std::endl;

	timer.reset();
	fpfh_est.setInputCloud(target_keypoints_ptr);
	fpfh_est.setInputNormals(normals_target.makeShared());
	fpfh_est.compute(features_target);
	duration = timer.getTimeSeconds();
	std::cout << "FPFH estimator: " << duration << "s" << std::endl;

	// Initialize Sample Consensus Prerejective with 5x the number of iterations and 1/5 feature kNNs as SAC-IA
	timer.reset();
	pcl::SampleConsensusPrerejective<KeyPointT, KeyPointT, pcl::FPFHSignature33> reg;
	reg.setMaxCorrespondenceDistance(scp_max_corr_distance);
	reg.setMaximumIterations(scp_max_iter);
	reg.setSimilarityThreshold(scp_sim_threshold);
	reg.setCorrespondenceRandomness(scp_corr_randomness);
	reg.setInlierFraction(scp_inlier_fraction);
	reg.setNumberOfSamples(scp_number_of_samples);

	// Set source and target cloud/features
	reg.setInputSource(source_keypoints_ptr);
	reg.setInputTarget(target_keypoints_ptr);
	reg.setSourceFeatures(features_source.makeShared());
	reg.setTargetFeatures(features_target.makeShared());

	// Register
	reg.align(*reg_keypoints_ptr);

	duration = timer.getTimeSeconds();
	std::cout << "SampleConsensusPrerejective: " << duration << "s" << std::endl;

	// Show result
	if (reg.hasConverged())
	{
		pcl::transformPointCloud(*cloud_source_ptr, *cloud_reg_ptr, reg.getFinalTransformation());
		Eigen::Matrix4f T = reg.getFinalTransformation();
		pcl::console::print_info("=======================================================\n");
		pcl::console::print_info("    | %6.3f %6.3f %6.3f | \n", T(0, 0), T(0, 1), T(0, 2));
		pcl::console::print_info("R = | %6.3f %6.3f %6.3f | \n", T(1, 0), T(1, 1), T(1, 2));
		pcl::console::print_info("    | %6.3f %6.3f %6.3f | \n", T(2, 0), T(2, 1), T(2, 2));
		pcl::console::print_info("\n");
		pcl::console::print_info("t = [ %0.3f, %0.3f, %0.3f ]\n", T(0, 3), T(1, 3), T(2, 3));
		pcl::console::print_info("\n");
		pcl::console::print_info("Inlier-fraction: (%i/%i) = %.2f%%\n",
			reg.getInliers().size(), source_keypoints_ptr->size(),
			100 * (float)reg.getInliers().size() / source_keypoints_ptr->size());
		pcl::console::print_info("\n");
		pcl::console::print_info("Mean error: %.6f[m]\n", reg.getFitnessScore(std::numeric_limits<double>::max()));
		pcl::console::print_info("=======================================================\n");
		showCloudsRight(cloud_reg_ptr, cloud_target_ptr);
	}
	else
	{
		pcl::console::print_error("Alignment failed!\n");
	}

	while (!p_viz->wasStopped())
	{
	}

	return 0;
}