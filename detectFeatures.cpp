#include<iostream>
#include "slamBase.h"
using namespace std;

// OpenCV �������ģ��
#include <opencv2/features2d/features2d.hpp>
// #include <opencv2/nonfree/nonfree.hpp> // use this if you want to use SIFT or SURF
#include <opencv2/calib3d/calib3d.hpp>

int main(int argc, char** argv)
{
	// ��������data�ļ������ȡ����rgb�����ͼ
	cv::Mat rgb1 = cv::imread("rgb1.png");
	cv::Mat rgb2 = cv::imread("rgb2.png");
	cv::Mat depth1 = cv::imread("depth1.png", -1);
	cv::Mat depth2 = cv::imread("depth2.png", -1);

	// ����������ȡ������������ȡ��
	cv::Ptr<cv::FeatureDetector> detector;
	cv::Ptr<cv::DescriptorExtractor> descriptor;

	// ������ȡ����Ĭ�����߶�Ϊ ORB

	// ���ʹ�� sift, surf ��֮ǰҪ��ʼ��nonfreeģ��
	// cv::initModule_nonfree();
	// _detector = cv::FeatureDetector::create( "SIFT" );
	// _descriptor = cv::DescriptorExtractor::create( "SIFT" );

	detector = cv::FeatureDetector::create("ORB");
	descriptor = cv::DescriptorExtractor::create("ORB");

	vector< cv::KeyPoint > kp1, kp2; //�ؼ���
	detector->detect(rgb1, kp1);  //��ȡ�ؼ���
	detector->detect(rgb2, kp2);

	cout << "Key points of two images: " << kp1.size() << ", " << kp2.size() << endl;

	// ���ӻ��� ��ʾ�ؼ���
	cv::Mat imgShow;
	cv::drawKeypoints(rgb1, kp1, imgShow, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
	cv::imshow("keypoints", imgShow);
	cv::imwrite("keypoints.png", imgShow);
	cv::waitKey(0); //��ͣ�ȴ�һ������

	// ����������
	cv::Mat desp1, desp2;
	descriptor->compute(rgb1, kp1, desp1);
	descriptor->compute(rgb2, kp2, desp2);

	// ƥ��������
	vector< cv::DMatch > matches;
	cv::BFMatcher matcher;
	matcher.match(desp1, desp2, matches);
	cout << "Find total " << matches.size() << " matches." << endl;

	// ���ӻ�����ʾƥ�������
	cv::Mat imgMatches;
	cv::drawMatches(rgb1, kp1, rgb2, kp2, matches, imgMatches);
	cv::imshow("matches", imgMatches);
	cv::imwrite("matches.png", imgMatches);
	cv::waitKey(0);

	// ɸѡƥ�䣬�Ѿ���̫���ȥ��
	// ����ʹ�õ�׼����ȥ�������ı���С�����ƥ��
	vector< cv::DMatch > goodMatches;
	double minDis = 9999;
	for (size_t i = 0; i<matches.size(); i++)
	{
		if (matches[i].distance < minDis)
			minDis = matches[i].distance;
	}
	cout << "min dis = " << minDis << endl;

	for (size_t i = 0; i<matches.size(); i++)
	{
		if (matches[i].distance < 10 * minDis)
			goodMatches.push_back(matches[i]);
	}

	// ��ʾ good matches
	cout << "good matches=" << goodMatches.size() << endl;
	cv::drawMatches(rgb1, kp1, rgb2, kp2, goodMatches, imgMatches);
	cv::imshow("good matches", imgMatches);
	cv::imwrite("good_matches.png", imgMatches);
	cv::waitKey(0);

	// ����ͼ�����˶���ϵ
	// �ؼ�������cv::solvePnPRansac()
	// Ϊ���ô˺���׼����Ҫ�Ĳ���

	// ��һ��֡����ά��
	vector<cv::Point3f> pts_obj;
	// �ڶ���֡��ͼ���
	vector< cv::Point2f > pts_img;

	// ����ڲ�
	CAMERA_INTRINSIC_PARAMETERS C;
	C.cx = 325.5;
	C.cy = 253.5;
	C.fx = 518.0;
	C.fy = 519.0;
	C.scale = 1000.0;

	for (size_t i = 0; i<goodMatches.size(); i++)
	{
		// query �ǵ�һ��, train �ǵڶ���
		cv::Point2f p = kp1[goodMatches[i].queryIdx].pt;
		// ��ȡd��ҪС�ģ�x�����ҵģ�y�����µģ�����y�����У�x���У�
		ushort d = depth1.ptr<ushort>(int(p.y))[int(p.x)];
		if (d == 0)
			continue;
		pts_img.push_back(cv::Point2f(kp2[goodMatches[i].trainIdx].pt));

		// ��(u,v,d)ת��(x,y,z)
		cv::Point3f pt(p.x, p.y, d);
		cv::Point3f pd = point2dTo3d(pt, C);
		pts_obj.push_back(pd);
	}

	double camera_matrix_data[3][3] = {
			{ C.fx, 0, C.cx },
			{ 0, C.fy, C.cy },
			{ 0, 0, 1 }
	};

	// �����������
	cv::Mat cameraMatrix(3, 3, CV_64F, camera_matrix_data);
	cv::Mat rvec, tvec, inliers;
	// ���pnp
	cv::solvePnPRansac(pts_obj, pts_img, cameraMatrix, cv::Mat(), rvec, tvec, false, 100, 1.0, 100, inliers);

	cout << "inliers: " << inliers.rows << endl;
	cout << "R=" << rvec << endl;
	cout << "t=" << tvec << endl;

	// ����inliersƥ�� 
	vector< cv::DMatch > matchesShow;
	for (size_t i = 0; i<inliers.rows; i++)
	{
		matchesShow.push_back(goodMatches[inliers.ptr<int>(i)[0]]);
	}
	cv::drawMatches(rgb1, kp1, rgb2, kp2, matchesShow, imgMatches);
	cv::imshow("inlier matches", imgMatches);
	cv::imwrite("inliers.png", imgMatches);
	cv::waitKey(0);

	return 0;
}