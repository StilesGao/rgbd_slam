#ifndef SLAM_BASE
#define SLAM_BASE
#endif
#ifndef EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
#define EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
#endif

// C++��׼��
#include <fstream>
#include <vector>
#include <string>
#include <map>
using namespace std;

// OpenCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/nonfree/nonfree.hpp>
#include <opencv2/calib3d/calib3d.hpp>


//PCL
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/common/transforms.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/filters/voxel_grid.h> 

#include <pcl/io/io.h>  

// Eigen !
#include "Eigen/Core"
#include "opencv2/core/eigen.hpp"
#include "Eigen/Geometry"


// ���Ͷ���
typedef pcl::PointXYZRGBA PointT;
typedef pcl::PointCloud<PointT> PointCloud;

// ����ڲνṹ
struct CAMERA_INTRINSIC_PARAMETERS
{
	double cx, cy, fx, fy, scale;
};

//������ȡ��
class ParameterReader
{
public:
	ParameterReader(string filename = "parameters.txt")
	{
		ifstream fin(filename.c_str());
		if (!fin)
		{
			cerr << "parameters file dose not exist" << endl;
			return;
		}
		while (!fin.eof())
		{
			string str;
			getline(fin, str);
			if (str[0] == '#')
			{
				//#��ͷΪע��
				continue;
			}

			int pos = str.find("=");
			if (pos == -1)
				continue;

			string key = str.substr(0, pos);
			string value = str.substr(pos + 1, str.length());
			data[key] = value;

			if (!fin.good())
				break;
		}
	}
	string getData(string key)
	{
		map<string, string>::iterator iter = data.find(key);
		if (iter == data.end())
		{
			cerr << "Parameters name " << key << "  no find" << endl;
			return string("not found");
		}
		return iter->second;
	}
public:
	map<string, string> data;
};

//֡�ṹ
struct FRAME
{
	int frameID;
	cv::Mat rgb, depth;
	cv::Mat desp;
	vector<cv::KeyPoint> kp;
};
//PnP���
struct RESULT_OF_PNP
{
	cv::Mat rvec, tvec;
	int inliers;
};

// �����ӿ�
// image2PonitCloud ��rgbͼת��Ϊ����
PointCloud::Ptr image2PointCloud(cv::Mat& rgb, cv::Mat& depth, CAMERA_INTRINSIC_PARAMETERS& camera);

// point2dTo3d ���������ͼ������ת��Ϊ�ռ�����
// input: 3ά��Point3f (u,v,d)
cv::Point3f point2dTo3d(cv::Point3f& point, CAMERA_INTRINSIC_PARAMETERS& camera);
//computeKeyPointsAndDesp ͬʱ��ȡ�ؼ���������������
void computeKeyPointsAndDesp(FRAME& frame, string detector, string descriptor);
// estimateMotion ��������֮֡����˶�
// ���룺֡1��֡2, ����ڲ�
RESULT_OF_PNP estimateMotion(FRAME& frame1, FRAME& frame2, CAMERA_INTRINSIC_PARAMETERS& camera);

inline static CAMERA_INTRINSIC_PARAMETERS getDefaultCamera()
{
	ParameterReader pd;
	CAMERA_INTRINSIC_PARAMETERS camera;
	camera.fx = atof(pd.getData("camera.fx").c_str());
	camera.fy = atof(pd.getData("camera.fy").c_str());
	camera.cx = atof(pd.getData("camera.cx").c_str());
	camera.cy = atof(pd.getData("camera.cy").c_str());
	camera.scale = atof(pd.getData("camera.scale").c_str());
	return camera;
}


//����

PointCloud::Ptr image2PointCloud(cv::Mat& rgb, cv::Mat& depth, CAMERA_INTRINSIC_PARAMETERS& camera)
{
	PointCloud::Ptr cloud(new PointCloud);

	for (int m = 0; m < depth.rows; m += 2)
		for (int n = 0; n < depth.cols; n += 2)
		{
		// ��ȡ���ͼ��(m,n)����ֵ
		ushort d = depth.ptr<ushort>(m)[n];
		// d ����û��ֵ������ˣ������˵�
		if (d == 0)
			continue;
		// d ����ֵ�������������һ����
		PointT p;

		// ���������Ŀռ�����
		p.z = double(d) / camera.scale;
		p.x = (n - camera.cx) * p.z / camera.fx;
		p.y = (m - camera.cy) * p.z / camera.fy;

		// ��rgbͼ���л�ȡ������ɫ
		// rgb����ͨ����BGR��ʽͼ�����԰������˳���ȡ��ɫ
		p.b = rgb.ptr<uchar>(m)[n * 3];
		p.g = rgb.ptr<uchar>(m)[n * 3 + 1];
		p.r = rgb.ptr<uchar>(m)[n * 3 + 2];

		// ��p���뵽������
		cloud->points.push_back(p);
		}
	// ���ò��������
	cloud->height = 1;
	cloud->width = cloud->points.size();
	cloud->is_dense = false;

	return cloud;
}

cv::Point3f point2dTo3d(cv::Point3f& point, CAMERA_INTRINSIC_PARAMETERS& camera)
{
	cv::Point3f p; // 3D ��
	p.z = double(point.z) / camera.scale;
	p.x = (point.x - camera.cx) * p.z / camera.fx;
	p.y = (point.y - camera.cy) * p.z / camera.fy;
	return p;
}

// computeKeyPointsAndDesp ͬʱ��ȡ�ؼ���������������
void computeKeyPointsAndDesp(FRAME& frame, string detector, string descriptor)
{
	cv::Ptr<cv::FeatureDetector> _detector;
	cv::Ptr<cv::DescriptorExtractor> _descriptor;

	_detector = cv::FeatureDetector::create(detector.c_str());
	_descriptor = cv::DescriptorExtractor::create(descriptor.c_str());

	if (!_detector || !_descriptor)
	{
		cerr << "Unknown detector or discriptor type !" << detector << "," << descriptor << endl;
		return;
	}

	_detector->detect(frame.rgb, frame.kp);
	_descriptor->compute(frame.rgb, frame.kp, frame.desp);

	return;
}

// estimateMotion ��������֮֡����˶�
// ���룺֡1��֡2
// �����rvec �� tvec
RESULT_OF_PNP estimateMotion(FRAME& frame1, FRAME& frame2, CAMERA_INTRINSIC_PARAMETERS& camera)
{
	static ParameterReader pd;
	vector< cv::DMatch > matches;
	cv::BFMatcher matcher;
	matcher.match(frame1.desp, frame2.desp, matches);

	RESULT_OF_PNP result;
	vector< cv::DMatch > goodMatches;
	double minDis = 9999;
	double good_match_threshold = atof(pd.getData("good_match_threshold").c_str());
	for (size_t i = 0; i<matches.size(); i++)
	{
		if (matches[i].distance < minDis)
			minDis = matches[i].distance;
	}

	cout << "min dis = " << minDis << endl;
	if (minDis < 10)
		minDis = 10;

	for (size_t i = 0; i<matches.size(); i++)
	{
		if (matches[i].distance < good_match_threshold*minDis)
			goodMatches.push_back(matches[i]);
	}

	cout << "good matches: " << goodMatches.size() << endl;

	if (goodMatches.size() <= 5)
	{
		result.inliers = -1;
		return result;
	}
	// ��һ��֡����ά��
	vector<cv::Point3f> pts_obj;
	// �ڶ���֡��ͼ���
	vector< cv::Point2f > pts_img;

	// ����ڲ�
	for (size_t i = 0; i<goodMatches.size(); i++)
	{
		// query �ǵ�һ��, train �ǵڶ���
		cv::Point2f p = frame1.kp[goodMatches[i].queryIdx].pt;
		// ��ȡd��ҪС�ģ�x�����ҵģ�y�����µģ�����y�����У�x���У�
		ushort d = frame1.depth.ptr<ushort>(int(p.y))[int(p.x)];
		if (d == 0)
			continue;
		pts_img.push_back(cv::Point2f(frame2.kp[goodMatches[i].trainIdx].pt));

		// ��(u,v,d)ת��(x,y,z)
		cv::Point3f pt(p.x, p.y, d);
		cv::Point3f pd = point2dTo3d(pt, camera);
		pts_obj.push_back(pd);
	}

	if (pts_obj.size() == 0 || pts_img.size() == 0)
	{
		result.inliers = -1;
		return result;
	}

	double camera_matrix_data[3][3] = {
			{ camera.fx, 0, camera.cx },
			{ 0, camera.fy, camera.cy },
			{ 0, 0, 1 }
	};

	// �����������
	cv::Mat cameraMatrix(3, 3, CV_64F, camera_matrix_data);
	cv::Mat rvec, tvec, inliers;
	// ���pnp
	cv::solvePnPRansac(pts_obj, pts_img, cameraMatrix, cv::Mat(), rvec, tvec, false, 100, 1.0, 100, inliers);

	result.rvec = rvec;
	result.tvec = tvec;
	result.inliers = inliers.rows;

	return result;
}

// cvMat2Eigen
Eigen::Isometry3d cvMat2Eigen(cv::Mat& rvec, cv::Mat& tvec)
{
	cv::Mat R;
	cv::Rodrigues(rvec, R);
	Eigen::Matrix3d r;//3*3 double
	for (int i = 0; i<3; i++)
		for (int j = 0; j<3; j++)
			r(i, j) = R.at<double>(i, j);

	// ��ƽ����������ת����ת���ɱ任����
	Eigen::Isometry3d T = Eigen::Isometry3d::Identity();

	Eigen::AngleAxisd angle(r);
	T = angle;
	T(0, 3) = tvec.at<double>(0, 0);
	T(1, 3) = tvec.at<double>(1, 0);
	T(2, 3) = tvec.at<double>(2, 0);
	return T;
}

// joinPointCloud 
// ���룺ԭʼ���ƣ�������֡�Լ�����λ��
// �����������֡�ӵ�ԭʼ֡���ͼ��
PointCloud::Ptr joinPointCloud(PointCloud::Ptr original, FRAME& newFrame, Eigen::Isometry3d& T, CAMERA_INTRINSIC_PARAMETERS& camera)
{
	PointCloud::Ptr newCloud = image2PointCloud(newFrame.rgb, newFrame.depth, camera);
	
	// �ϲ�����
	PointCloud::Ptr output(new PointCloud());
	pcl::transformPointCloud(*original, *output, T.matrix().cast<float>());
	*newCloud += *output;
	
	// Voxel grid �˲�������
	PointCloud::Ptr tmp(new PointCloud());
	static pcl::VoxelGrid<PointT> voxel;
	static ParameterReader pd;
	
	float gridsize = atof(pd.getData("voxel_grid").c_str());
	voxel.setLeafSize(gridsize, gridsize, gridsize);
	voxel.setInputCloud(newCloud);
	voxel.filter( *tmp );
	
	return newCloud;
}
