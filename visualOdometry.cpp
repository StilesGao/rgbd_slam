#include <iostream>
#include <fstream>
#include <sstream>
#include "slamBase.h"
#include <math.h>

using namespace std;

// ����index����ȡһ֡����
FRAME readFrame(int index, ParameterReader& pd);
// �����˶��Ĵ�С
double normofTransform(cv::Mat rvec, cv::Mat tvec);

int main(int argc, char** argv)
{
	
	ParameterReader pd;
	int startIndex = atoi(pd.getData("start_index").c_str());
	int endIndex = atoi(pd.getData("end_index").c_str());
	
	// initialize
	cout << "Initializing ..." << endl;
	int currIndex = startIndex; // ��ǰ����ΪcurrIndex
	FRAME lastFrame = readFrame(currIndex, pd); // ��һ֡����
	// ���������ڱȽ�currFrame��lastFrame
	string detector = pd.getData("detector");
	string descriptor = pd.getData("descriptor");
	CAMERA_INTRINSIC_PARAMETERS camera = getDefaultCamera();
	computeKeyPointsAndDesp(lastFrame, detector, descriptor);
	PointCloud::Ptr cloud = image2PointCloud(lastFrame.rgb, lastFrame.depth, camera);

	pcl::visualization::CloudViewer viewer("viewer");

	// �Ƿ���ʾ����
	bool visualize = pd.getData("visualize_pointcloud") == string("yes");

	int min_inliers = atoi(pd.getData("min_inliers").c_str());
	double max_norm = atof(pd.getData("max_norm").c_str());

	for (currIndex = startIndex + 1; currIndex < endIndex; currIndex++)
	{
		cout << "Reading files " << currIndex << endl;
		FRAME currFrame = readFrame(currIndex, pd); // ��ȡcurrFrame
		computeKeyPointsAndDesp(currFrame, detector, descriptor);
		// �Ƚ�currFrame �� lastFrame
		RESULT_OF_PNP result = estimateMotion(lastFrame, currFrame, camera);
		if (result.inliers < min_inliers) //inliers������������֡
			continue;
		// �����˶���Χ�Ƿ�̫��
		double norm = normofTransform(result.rvec, result.tvec);
		cout << "norm = " << norm << endl;
		if (norm >= max_norm)
			continue;
		Eigen::Isometry3d T = cvMat2Eigen(result.rvec, result.tvec);
		cout << "T=" << T.matrix() << endl;

		cloud = joinPointCloud(cloud, currFrame, T, camera);

		if (visualize == true)
			viewer.showCloud(cloud);

		lastFrame = currFrame;
	}

	pcl::io::savePCDFile("result.pcd", *cloud);
	return 0;
	

}

FRAME readFrame(int index, ParameterReader& pd)
{
	FRAME f;
	string rgbDir = pd.getData("rgb_dir");
	string depthDir = pd.getData("depth_dir");

	string rgbExt = pd.getData("rgb_extension");
	string depthExt = pd.getData("depth_extension");

	stringstream ss;
	ss << rgbDir << index << rgbExt;
	string filename;
	ss >> filename;
	f.rgb = cv::imread(filename);

	ss.clear();
	filename.clear();
	ss << depthDir << index << depthExt;
	ss >> filename;

	f.depth = cv::imread(filename, -1);
	return f;
	
}

double normofTransform(cv::Mat rvec, cv::Mat tvec)
{
	return fabs(min(cv::norm(rvec), 2 * M_PI - cv::norm(rvec))) + fabs(cv::norm(tvec));
}