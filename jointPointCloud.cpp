
#include<iostream>
#include<string>

using namespace std;

#include "slamBase.h"

#include <opencv2/core/eigen.hpp>

#include <pcl/common/transforms.h>
#include <pcl/visualization/cloud_viewer.h>

// Eigen !
#include <Eigen/Core>
#include <Eigen/Geometry>

int main(int argc, char** argv)
{
	//����Ҫƴ�ϵ�data�е�����ͼ��
	ParameterReader pd;
	//��������֡
	FRAME frame1, frame2;

	//��ͼ��
	frame1.rgb = cv::imread("21.jpg");
	frame1.depth  = cv::imread("22,jpg",-1);
	frame2.rgb = cv::imread("21.jpg");
	frame2.depth = cv::imread("22.jpg",-1);

	// ��ȡ����������������
	cout << "extracting features" << endl;
	string detecter = pd.getData("detector");
	string descriptor = pd.getData("descriptor");
	
	computeKeyPointsAndDesp(frame1, detecter, descriptor);
	computeKeyPointsAndDesp(frame2, detecter, descriptor);
	
	// ����ڲ�
	CAMERA_INTRINSIC_PARAMETERS camera;
	camera.fx = atof(pd.getData("camera.fx").c_str());
	camera.fy = atof(pd.getData("camera.fy").c_str());
	camera.cx = atof(pd.getData("camera.cx").c_str());
	camera.cy = atof(pd.getData("camera.cy").c_str());
	camera.scale = atof(pd.getData("camera.scale").c_str());

	cout << "solving pnp" << endl;
	// ���pnp
	RESULT_OF_PNP result = estimateMotion(frame1, frame2, camera);

	cout << result.rvec << endl << result.tvec << endl;
	
	// ����result
	// ����ת����ת��Ϊ��ת����
	cv::Mat R;
	cv::Rodrigues(result.rvec, R);
	Eigen::Matrix3d r;
	cv::cv2eigen(R, r);

	// ��ƽ����������ת����ת���ɱ任����
	Eigen::Isometry3d T = Eigen::Isometry3d::Identity();//Isometry3d�ǵȾ�任

	Eigen::AngleAxisd angle(r);//AngleAxisd������Ƕ�
	cout << "translation" << endl;
	
	T = angle;
	T(0, 3) = result.tvec.at<double>(0, 0);
	T(1, 3) = result.tvec.at<double>(1, 0);
	T(2, 3) = result.tvec.at<double>(2, 0);
	
	// ת������
	cout << "converting image to clouds" << endl;
	PointCloud::Ptr cloud1 = image2PointCloud(frame1.rgb, frame1.depth, camera);
	PointCloud::Ptr cloud2 = image2PointCloud(frame2.rgb, frame2.depth, camera);

	// �ϲ�����
	cout << "combining clouds" << endl;
	PointCloud::Ptr output(new PointCloud());
	pcl::transformPointCloud(*cloud1, *output, T.matrix().cast<float>());
	*output += *cloud2;
	pcl::io::savePCDFile("result.pcd", *output);
	cout << "Final result saved." << endl;

	pcl::visualization::CloudViewer viewer("viewer");
	viewer.showCloud(output);
	while (!viewer.wasStopped())
	{

	}
	return 0;
	
}