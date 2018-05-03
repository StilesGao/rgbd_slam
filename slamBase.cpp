#include "slamBase.h"

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


