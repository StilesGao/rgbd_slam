#include <iostream>
#include <fstream>
#include <sstream>
using namespace std;

#include "slamBase.h"

#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/passthrough.h>

#include <g2o/types/slam3d/types_slam3d.h>
#include <g2o/core/sparse_optimizer.h>
#include <g2o/core/block_solver.h>
#include <g2o/core/factory.h>
#include <g2o/core/optimization_algorithm_factory.h>
#include <g2o/core/optimization_algorithm_gauss_newton.h>
#include <g2o/solvers/eigen/linear_solver_eigen.h>
#include <g2o/core/robust_kernel.h>
#include <g2o/core/robust_kernel_impl.h>
#include <g2o/core/optimization_algorithm_levenberg.h>

// ��g2o�Ķ���ŵ�ǰ��
typedef g2o::BlockSolver_6_3 SlamBlockSolver;
typedef g2o::LinearSolverEigen< SlamBlockSolver::PoseMatrixType > SlamLinearSolver;

// ����index����ȡһ֡����
FRAME readFrame(int index, ParameterReader& pd);
// ����һ���˶��Ĵ�С
double normofTransform(cv::Mat rvec, cv::Mat tvec);

// �������֡���������
enum CHECK_RESULT { NOT_MATCHED = 0, TOO_FAR_AWAY, TOO_CLOSE, KEYFRAME };
// ��������
CHECK_RESULT checkKeyframes(FRAME& f1, FRAME& f2, g2o::SparseOptimizer& opti, bool is_loops = false);
// ��������Ļػ�
void checkNearbyLoops(vector<FRAME>& frames, FRAME& currFrame, g2o::SparseOptimizer& opti);
// ������ػ�
void checkRandomLoops(vector<FRAME>& frames, FRAME& currFrame, g2o::SparseOptimizer& opti);

int main(int argc, char** argv)
{
	// ǰ�沿�ֺ�vo��һ����
	ParameterReader pd;
	int startIndex = atoi(pd.getData("start_index").c_str());
	int endIndex = atoi(pd.getData("end_index").c_str());

	// ���еĹؼ�֡������������
	vector< FRAME > keyframes;
	// initialize
	cout << "Initializing ..." << endl;
	int currIndex = startIndex; // ��ǰ����ΪcurrIndex
	FRAME currFrame = readFrame(currIndex, pd); // ��һ֡����

	string detector = pd.getData("detector");
	string descriptor = pd.getData("descriptor");
	CAMERA_INTRINSIC_PARAMETERS camera = getDefaultCamera();
	computeKeyPointsAndDesp(currFrame, detector, descriptor);
	PointCloud::Ptr cloud = image2PointCloud(currFrame.rgb, currFrame.depth, camera);

	pcl::visualization::CloudViewer viewer("viewer");

	// �Ƿ���ʾ����
	bool visualize = pd.getData("visualize_pointcloud") == string("yes");

	/*******************************
	// ����:�й�g2o�ĳ�ʼ��
	*******************************/
	// ��ʼ�������
	SlamLinearSolver* linearSolver = new SlamLinearSolver();
	linearSolver->setBlockOrdering(false);
	SlamBlockSolver* blockSolver = new SlamBlockSolver(linearSolver);
	g2o::OptimizationAlgorithmLevenberg* solver = new g2o::OptimizationAlgorithmLevenberg(blockSolver);

	g2o::SparseOptimizer globalOptimizer;  // ����õľ����������
	globalOptimizer.setAlgorithm(solver);
	// ��Ҫ���������Ϣ
	globalOptimizer.setVerbose(false);

	// ��globalOptimizer���ӵ�һ������
	g2o::VertexSE3* v = new g2o::VertexSE3();
	v->setId(currIndex);
	v->setEstimate(Eigen::Isometry3d::Identity()); //����Ϊ��λ����
	v->setFixed(true); //��һ������̶��������Ż�
	globalOptimizer.addVertex(v);

	keyframes.push_back(currFrame);

	double keyframe_threshold = atof(pd.getData("keyframe_threshold").c_str());
	bool check_loop_closure = pd.getData("check_loop_closure") == string("yes");

	for (currIndex = startIndex + 1; currIndex<endIndex; currIndex++)
	{
		cout << "Reading files " << currIndex << endl;
		FRAME currFrame = readFrame(currIndex, pd); // ��ȡcurrFrame
		computeKeyPointsAndDesp(currFrame, detector, descriptor); //��ȡ����
		CHECK_RESULT result = checkKeyframes(keyframes.back(), currFrame, globalOptimizer); //ƥ���֡��keyframes�����һ֡
		switch (result) // ����ƥ������ͬ��ȡ��ͬ����
		{
		case NOT_MATCHED:
			//ûƥ���ϣ�ֱ������
			cout << "Not enough inliers." << endl;
			break;
		case TOO_FAR_AWAY:
			// ̫���ˣ�Ҳֱ����
			cout << "Too far away, may be an error." << endl;
			break;
		case TOO_CLOSE:
			// ̫Զ�ˣ����ܳ�����
			cout << "Too close, not a keyframe" << endl;
			break;
		case KEYFRAME:
			cout << "This is a new keyframe" << endl;
			// ��Զ�������պ�
			/**
			* This is important!!
			* This is important!!
			* This is important!!
			* (very important so I've said three times!)
			*/
			// ���ػ�
			
			if (check_loop_closure)
			{
				checkNearbyLoops(keyframes, currFrame, globalOptimizer);
				checkRandomLoops(keyframes, currFrame, globalOptimizer);
			}
			keyframes.push_back(currFrame);
			
			break;
		default:
			break;
		}
	}
	
	// �Ż�
	cout <<"optimizing pose graph, vertices: " << globalOptimizer.vertices().size() << endl;
	globalOptimizer.save("result_before.g2o");
	globalOptimizer.initializeOptimization();
	globalOptimizer.optimize(100); //����ָ���Ż�����
	globalOptimizer.save("result_after.g2o");
	cout << "Optimization done." << endl;
	
	// ƴ�ӵ��Ƶ�ͼ
	cout << "saving the point cloud map..." << endl;
	PointCloud::Ptr output(new PointCloud()); //ȫ�ֵ�ͼ
	PointCloud::Ptr tmp(new PointCloud());
	/*
	pcl::VoxelGrid<PointT> voxel; // �����˲�����������ͼ�ֱ���
	
	pcl::PassThrough<PointT> pass; // z���������˲���������rgbd�������Ч����������ޣ���̫Զ��ȥ��
	
	pass.setFilterFieldName("z");
	pass.setFilterLimits(0.0, 4.0); //4m���ϾͲ�Ҫ��
	
	double gridsize = atof(pd.getData("voxel_grid").c_str()); //�ֱ�ͼ������parameters.txt���
	voxel.setLeafSize(gridsize, gridsize, gridsize);
	*/
	for (size_t i = 0; i<keyframes.size(); i++)
	{
		// ��g2o��ȡ��һ֡
		g2o::VertexSE3* vertex = dynamic_cast<g2o::VertexSE3*>(globalOptimizer.vertex(keyframes[i].frameID));
		Eigen::Isometry3d pose = vertex->estimate(); //��֡�Ż����λ��
		PointCloud::Ptr newCloud = image2PointCloud(keyframes[i].rgb, keyframes[i].depth, camera); //ת�ɵ���
		// �������˲�
		/*
		voxel.setInputCloud(newCloud);
		voxel.filter(*tmp);
		pass.setInputCloud(tmp);
		pass.filter(*newCloud);
		*/
		// �ѵ��Ʊ任�����ȫ�ֵ�ͼ��
		pcl::transformPointCloud(*newCloud, *tmp, pose.matrix());
		*output += *tmp;
		
		if (visualize == true)
		{
			viewer.showCloud(output);
		}
		
		tmp->clear();
		newCloud->clear();
		
	}
	
	/*
	voxel.setInputCloud(output);
	voxel.filter(*tmp);
	*/
	//�洢
	//pcl::io::savePCDFile("result.pcd", *output);
	
	cout << "Final map is saved." << endl;
	system("pause");
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
	f.frameID = index;
	return f;
}

double normofTransform(cv::Mat rvec, cv::Mat tvec)
{
	return fabs(min(cv::norm(rvec), 2 * M_PI - cv::norm(rvec))) + fabs(cv::norm(tvec));
}

CHECK_RESULT checkKeyframes(FRAME& f1, FRAME& f2, g2o::SparseOptimizer& opti, bool is_loops)
{
	static ParameterReader pd;
	static int min_inliers = atoi(pd.getData("min_inliers").c_str());
	static double max_norm = atof(pd.getData("max_norm").c_str());
	static double keyframe_threshold = atof(pd.getData("keyframe_threshold").c_str());
	static double max_norm_lp = atof(pd.getData("max_norm_lp").c_str());
	static CAMERA_INTRINSIC_PARAMETERS camera = getDefaultCamera();
	// �Ƚ�f1 �� f2
	RESULT_OF_PNP result = estimateMotion(f1, f2, camera);
	if (result.inliers < min_inliers) //inliers������������֡
		return NOT_MATCHED;
	// �����˶���Χ�Ƿ�̫��
	double norm = normofTransform(result.rvec, result.tvec);
	if (is_loops == false)
	{
		if (norm >= max_norm)
			return TOO_FAR_AWAY;   // too far away, may be error
	}
	else
	{
		if (norm >= max_norm_lp)
			return TOO_FAR_AWAY;
	}

	if (norm <= keyframe_threshold)
		return TOO_CLOSE;   // too adjacent frame
	// ��g2o�����������������һ֡��ϵ�ı�
	// ���㲿��
	// ����ֻ���趨id����
	if (is_loops == false)
	{
		g2o::VertexSE3 *v = new g2o::VertexSE3();
		v->setId(f2.frameID);
		v->setEstimate(Eigen::Isometry3d::Identity());
		opti.addVertex(v);
	}
	// �߲���
	g2o::EdgeSE3* edge = new g2o::EdgeSE3();
	// ���Ӵ˱ߵ���������id
	edge->setVertex(0, opti.vertex(f1.frameID));
	edge->setVertex(1, opti.vertex(f2.frameID));
	edge->setRobustKernel(new g2o::RobustKernelHuber());
	// ��Ϣ����
	Eigen::Matrix<double, 6, 6> information = Eigen::Matrix< double, 6, 6 >::Identity();
	// ��Ϣ������Э���������棬��ʾ���ǶԱߵľ��ȵ�Ԥ�ȹ���
	// ��ΪposeΪ6D�ģ���Ϣ������6*6���󣬼���λ�úͽǶȵĹ��ƾ��Ⱦ�Ϊ0.1�һ������
	// ��ôЭ������Ϊ�Խ�Ϊ0.01�ľ�����Ϣ����Ϊ100�ľ���
	information(0, 0) = information(1, 1) = information(2, 2) = 100;
	information(3, 3) = information(4, 4) = information(5, 5) = 100;
	// Ҳ���Խ��Ƕ����һЩ����ʾ�ԽǶȵĹ��Ƹ���׼ȷ
	edge->setInformation(information);
	// �ߵĹ��Ƽ���pnp���֮���
	Eigen::Isometry3d T = cvMat2Eigen(result.rvec, result.tvec);
	// edge->setMeasurement( T );
	edge->setMeasurement(T.inverse());
	// ���˱߼���ͼ��
	opti.addEdge(edge);
	return KEYFRAME;
}

void checkNearbyLoops(vector<FRAME>& frames, FRAME& currFrame, g2o::SparseOptimizer& opti)
{
	static ParameterReader pd;
	static int nearby_loops = atoi(pd.getData("nearby_loops").c_str());

	// ���ǰ�currFrame�� frames��ĩβ������һ��
	if (frames.size() <= nearby_loops)
	{
		// no enough keyframes, check everyone
		for (size_t i = 0; i<frames.size(); i++)
		{
			checkKeyframes(frames[i], currFrame, opti, true);
		}
	}
	else
	{
		// check the nearest ones
		for (size_t i = frames.size() - nearby_loops; i<frames.size(); i++)
		{
			checkKeyframes(frames[i], currFrame, opti, true);
		}
	}
}

void checkRandomLoops(vector<FRAME>& frames, FRAME& currFrame, g2o::SparseOptimizer& opti)
{
	static ParameterReader pd;
	static int random_loops = atoi(pd.getData("random_loops").c_str());
	srand((unsigned int)time(NULL));
	// ���ȡһЩ֡���м��

	if (frames.size() <= random_loops)
	{
		// no enough keyframes, check everyone
		for (size_t i = 0; i<frames.size(); i++)
		{
			checkKeyframes(frames[i], currFrame, opti, true);
		}
	}
	else
	{
		// randomly check loops
		for (int i = 0; i<random_loops; i++)
		{
			int index = rand() % frames.size();
			checkKeyframes(frames[index], currFrame, opti, true);
		}
	}
}
