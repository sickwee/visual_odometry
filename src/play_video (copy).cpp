#include <ros/ros.h>
#include <sensor_msgs/CompressedImage.h>
#include <sstream>
#include <iostream>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Header.h>
#include <sys/time.h>

#include <opencv2/video/tracking.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>

#include <ctype.h>
//---------------------------------------------

int MAX_FEATURE_COUNT = 200;
#define PRINT_ALL 1
int PRINT_FEATURES_LOCATION = 0;
int SHOW_VIDEO_FEED = 0;
int SHOW_FLOW = 0;
int SAVE_FRAMES = 0;

//---------------------------------------------

using namespace cv;
using namespace std;

//--------------------------------------------- Global variables

int needtoInit = 1;
Mat mask;
cv::Mat image, previmage;

cv::Mat imagef, previmagef;
cv::Mat imagee, previmagee;
cv::Mat imaged, previmaged;

vector<Point2f> corners, prevcorners;

std::string rosbagstring = "/camera_meio/led/compressed"; 
std::string nodename = "/camera_meio"; 

vector<Point2f> cornersf, prevcornersf;
vector<Point2f> cornerse, prevcornerse;
vector<Point2f> cornersd, prevcornersd;

int ofcnt = 0;

//----------------------- EXTRA FUNCTIONS HERE
/*
void epipolarlines(Mat leftImage, Mat rightImage, vector<Point2f> imagePointsLeftCamera, vector<Point2f> imagePointsRightCamera){
	cv::Mat fundamentalMatrix= cv::findFundamentalMat(imagePointsLeftCamera, imagePointsRightCamera, FM_8POINT);
	std::vector<cv::Vec3d> leftLines, rightLines;
	cv::computeCorrespondEpilines(imagePointsLeftCamera, 1, fundamentalMatrix, rightLines);
    cv::computeCorrespondEpilines(imagePointsRightCamera, 2, fundamentalMatrix, leftLines);
	
    cv::Mat leftImageRGB(leftImage.size(), CV_8UC3);
    cv::cvtColor(leftImage, leftImageRGB, CV_GRAY2RGB);
	
    cv::Mat rightImageRGB(rightImage.size(), CV_8UC3);
    cv::cvtColor(rightImage, rightImageRGB, CV_GRAY2RGB);
 
    cv::Mat imagePointLeftCameraMatrix=cv::Mat_<double>(3,1);
 
    for(std::size_t i=0;i<rightLines.size();i=i+1)
    {
        cv::Vec3d l=rightLines.at(i);
	double a=l.val[0];
        double b=l.val[1];
        double c=l.val[2];
        std::cout<<"------------------------a,b,c Using OpenCV (ax+by+c=0)------------------------------"<<std::endl;
        std::cout<< a <<", "<<b <<", "<<c <<std::endl;
        std::cout<<"------------------------calculating a,b,c (ax+by+c=0) ------------------------------"<<std::endl;
        
        imagePointLeftCameraMatrix.at<double>(0,0)=imagePointsLeftCamera[i].x;
        imagePointLeftCameraMatrix.at<double>(1,0)=imagePointsLeftCamera[i].y;
        imagePointLeftCameraMatrix.at<double>(2,0)=1;
        cv::Mat rightLineMatrix=fundamentalMatrix*imagePointLeftCameraMatrix;
        
        std::cout<< rightLineMatrix.at<double>(0,0) <<", "<<rightLineMatrix.at<double>(0,1) <<", "<<rightLineMatrix.at<double>(0,2) <<std::endl;
 
        //drawing the line on the image
        //ax+by+c=0
        double x0,y0,x1,y1;
        x0=0;
        y0=(-c-a*x0)/b;
	x1=rightImageRGB.cols;
        y1=(-c-a*x1)/b;
 
	std::cout<<"error: "<< a*imagePointsRightCamera.at(i).x+ b*imagePointsRightCamera.at(i).y +c<<std::endl;
	cv::line(rightImageRGB, cvPoint(x0,y0), cvPoint(x1,y1), cvScalar(0,255,0), 1);
    }
}*/

namespace patch //std::to_string patch: there's a known bug with to_string not being a member of std when compiling
{
    template < typename T > std::string to_string( const T& n )
    {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
    }
}

Mat correctGamma( Mat& img, double gamma ) {
 double inverse_gamma = 1.0 / gamma;
 
 Mat lut_matrix(1, 256, CV_8UC1 );
 uchar * ptr = lut_matrix.ptr();
 for( int i = 0; i < 256; i++ )
   ptr[i] = (int)( pow( (double) i / 255.0, inverse_gamma ) * 255.0 );
 
 Mat result;
 LUT( img, lut_matrix, result );
 
 return result;
}

int cntr = 0;
//----------------------- Image Callback - runs when a new image is available
void imageCallback(const sensor_msgs::CompressedImageConstPtr& msg)
{
  try
  { 
	const char *cstr = rosbagstring.c_str();
	if(strcmp(cstr,"/camera_meio/led/compressed")==0){
		std::swap(corners, cornersf);
		std::swap(prevcorners, prevcornersf);
		cv::swap(image, imagef);
		cv::swap(previmage, previmagef);
	}
	if(strcmp(cstr,"/camera_direita/led/compressed")==0){
		std::swap(corners, cornersd);
		std::swap(prevcorners, prevcornersd);
		cv::swap(image, imaged);
		cv::swap(previmage, previmaged);
	}
	if(strcmp(cstr,"/camera_esquerda/led/compressed")==0){
		std::swap(corners, cornerse);
		std::swap(prevcorners, prevcornerse);
		cv::swap(image, imagee);
		cv::swap(previmage, previmagee);
	}
//----------------------- Start

	image = cv::imdecode(cv::Mat(msg->data),1);//convert compressed image data to cv::Mat

	cvtColor( image, image, CV_BGR2GRAY );

	if(SAVE_FRAMES == 1){
		std::string savingName = "../imgcamcalib/"+patch::to_string(++cntr)+".jpg";
		cv::imwrite(savingName, image);
		//printf(savingName);
		printf("\nImage Saved\n");
	}

	//image = 0.25* (image + 100);
	image = correctGamma(image, 1.2);	

	Mat img;
	cv::resize(image, img, cv::Size(), 0.25, 0.25);
	//if(SHOW_VIDEO_FEED == 1)cv::imshow("view" , img);
	cv::waitKey(10);
	
//----------------------- Detector parameters (Shi-Tomasi corner detector)

	double qualityLevel = 0.03;
	double minDistance = 15;
	int blockSize = 15;
	bool useHarrisDetector = false;
	double k = 0.04;

	TermCriteria termcrit(TermCriteria::COUNT|TermCriteria::EPS,20,0.03);
	Size subPixWinSize(10,10), winSize(31,31);

//----------------------- Detecting corners (Shi-Tomasi corner detector)

	/// Apply corner detection
	goodFeaturesToTrack( image,
		corners,
		MAX_FEATURE_COUNT,
		qualityLevel,
		minDistance,
		Mat(),
		blockSize,
		useHarrisDetector,
		k );
	cornerSubPix(image, corners, winSize, Size(-1,-1), termcrit);
	cv::Mat image_copy = image;

//----------------------- Need to Init
	int ndetected = 0;
	if(ndetected < 50){
		goodFeaturesToTrack( previmage,
		prevcorners,
		MAX_FEATURE_COUNT,
		qualityLevel,
		minDistance,
		Mat(),
		blockSize,
		useHarrisDetector,
		k );
	}

	if (needtoInit == 1){
		prevcorners = corners;
		previmage = image;
		needtoInit = 0;
		mask = Mat::zeros(image.size(), image.type());
	}

//----------------------- LK
	
	std::vector<uchar> status;
	std::vector<float> err;

	calcOpticalFlowPyrLK(previmage, image, prevcorners, corners, status, err, winSize, 3, termcrit, 0, 0.001);

//----------------------- Draw features

ndetected = 0;
	for( int i = 0; i < corners.size() ; i++ ){ 
	if(status[i] == 1){
		circle( image_copy, Point( corners[i].x, corners[i].y ), 5,  Scalar(0), 2, 8, 0 );
		if(PRINT_FEATURES_LOCATION == 1){
			printf("Features %d: ( %.2f , %.2f ) status: %d err: %f \n",ndetected+1,corners[i].x,corners[i].y, status[i],err[i]);
			//printf("Features (prev): ( %.2f , %.2f )\n",prevcorners[i].x,prevcorners[i].y);
		}
		ndetected = ndetected + 1;

	}
	}	
	cv::resize(image_copy, img, cv::Size(), 0.50, 0.50);
	if(SHOW_VIDEO_FEED == 1)cv::imshow("corners" , img);

//-------

 	Mat frame = image;
	//Mat mask = Mat::zeros(image.size(), image.type());
	vector<Scalar> colors;
	RNG rng;
	for(int i = 0; i < 100; i++)
	{
		int r = rng.uniform(0, 256);
		int g = rng.uniform(0, 256);
		int b = rng.uniform(0, 256);
		colors.push_back(Scalar(r,g,b));
	}
        vector<Point2f> good_new;
       for(int i = 0; i < corners.size(); i = i + 5){

            // Select good points
            if(err[i] <= 100){
                good_new.push_back(corners[i]);
                // draw the tracks
                line(mask,corners[i], prevcorners[i], colors[i], 1);
                circle(frame, corners[i], 5, colors[i], -1);
            }

        }
/*
	Mat imgof;
        cv::add(frame, mask, imgof);
	cv::resize(image_copy, imgof, cv::Size(), 0.50, 0.50);
        if(SHOW_VIDEO_FEED == 1)imshow("Optical Flow", mask);
*/
	

	if (ofcnt == 100){
		mask = Mat::zeros(image.size(), image.type());
		ofcnt = 0;
	}

	ofcnt = ofcnt + 1;

	Mat imgof;
	cv::resize(mask, imgof, cv::Size(), 0.50, 0.50);
        if(SHOW_VIDEO_FEED == 1)imshow("Optical Flow", imgof);
	if(SHOW_VIDEO_FEED == 1)imshow("view" , img);

	if(SHOW_VIDEO_FEED == 1 && SHOW_FLOW != 0){
		Mat hstack;
		hconcat(img, imgof, hstack);
		cv::resize(hstack, hstack, cv::Size(), 0.90, 0.90);
		imshow("view", hstack);
	}

	if(SHOW_FLOW == 1){
		cv::add(imgof, img, imgof);
		imshow("Optical Flow", imgof);
	}
	
//----------------------- Epipolar Check

	Mat rigidtransform = estimateRigidTransform(prevcorners, corners, 0);
	//epipolarlines(previmage, image, prevcorners, corners);
	cv::Mat fundM = cv::findFundamentalMat(prevcorners, corners, FM_8POINT);

	std::vector<cv::Vec3d> prevlines, lines;

/*	computeCorrespondEpilines(prevcorners, 1, fundM, lines);
    	cv::computeCorrespondEpilines(corners, 2, fundM, prevlines);

	cv::Mat imagePointLeftCameraMatrix=cv::Mat_<double>(3,1);

for(std::size_t i=0;i<lines.size();i=i+1)
    {
        cv::Vec3d l=lines.at(i);
	double a=l.val[0];
        double b=l.val[1];
        double c=l.val[2];
       
        imagePointLeftCameraMatrix.at<double>(0,0)=prevcorners[i].x;
        imagePointLeftCameraMatrix.at<double>(1,0)=prevcorners[i].y;
        imagePointLeftCameraMatrix.at<double>(2,0)=1;
        cv::Mat rightLineMatrix=fundM*imagePointLeftCameraMatrix;
       
        //drawing the line on the image
        //ax+by+c=0

        double x0,y0,x1,y1;
        x0=0;
        y0=(-c-a*x0)/b;
	x1=image.cols;
        y1=(-c-a*x1)/b;
 
	cv::line(image, cvPoint(x0,y0), cvPoint(x1,y1), cvScalar(0,255,0), 1);
    }*/


//----------------------- Prepare new cycle
/*
	if(PRINT_ALL == 1){
		printf("|-------------------------------------------------------|\n\n");
		printf("[%d detected] \n\n",ndetected);
		cout << "Fundamental Matrix: \n";
		cout << fundM;
		cout << "\n\n";	
		
	}
*/
	std::swap(corners, prevcorners);
	cv::swap(previmage, image);
	destroyAllWindows;

//----------------------- PUBLISH DATA
/*
	std_msgs::Float32MultiArray dat;

	dat.layout.dim.push_back(std_msgs::MultiArrayDimension());
	dat.layout.dim.push_back(std_msgs::MultiArrayDimension());
	dat.layout.dim[0].label = "height";
	dat.layout.dim[1].label = "width";
	int H = prevcorners.size();
	dat.layout.dim[0].size = H;
	int W = 2;
	dat.layout.dim[1].size = W;
	dat.layout.dim[0].stride = H*W;
	dat.layout.dim[1].stride = W;
	dat.layout.data_offset = 0;

	ros::NodeHandle nh;
	ros::Publisher pub = nh.advertise<std_msgs::Float32MultiArray>("uwv_2d_data", 1000);

		dat.data.push_back(ndetected);
	for (int i=0; i<H; i++){
		dat.data.push_back(prevcorners[i].x);
		dat.data.push_back(prevcorners[i].y);
	}

	pub.publish(dat);
	dat.data.clear();


	//printf("\n\n\n\n %d \n\n\n\n ",ndetected); //debug
*/
//----------------------- GET POSE

	//FocalLength: [1.1055e+03 1.1051e+03]
	//PrincipalPoint: [1.0376e+03 834.9490]

	double focal = 1.1055e+03; // focal = camera focal length
	cv::Point2d pp(1.0376e+03, 834.9490); // pp = principle point;
	Mat E, R, t, mask;

	// Mat E = K.t() * F * K; // K = camera intrinsic matrix & F = Fundamental Matrix
	E = findEssentialMat(prevcorners, corners, focal, pp, RANSAC, 0.999, 1.0, mask);
	recoverPose(E, corners, prevcorners, R, t, focal, pp, mask);

//-------------------------------------------------------------------------- PRINT FOR TXT LOGS
//-------------------------------------------------------------------------- PRINT FOR TXT LOGS
//-------------------------------------------------------------------------- PRINT FOR TXT LOGS

	struct timeval tp;
	gettimeofday(&tp, NULL);
	long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;

	cout << ms << " ";

	double* mtemp1 = &R.at<double>(0);
	for(int i=0;i<9;i++)cout << mtemp1[i] << " ";

	double* mtemp2 = &t.at<double>(0);
	for(int i=0;i<3;i++)cout << mtemp2[i] << " ";

	cout << endl;
/*
	cout << ms << endl;
	cout << R << endl;
	cout << t << endl;
*/

//----------------------- PUBLISH R&t DATA

	std_msgs::Float32MultiArray dat;

	ros::NodeHandle n;
	ros::Publisher pub = n.advertise<std_msgs::Float32MultiArray>("uwv_rt_data", 1000);

	for(int i = 0; i<3; i++){
		dat.data.push_back(t.at<double>(i));
	}
	for(int i = 0; i<9; i++){
		dat.data.push_back(R.at<double>(i));
	}

	pub.publish(dat);
	dat.data.clear();

//----------------------- End

	if(strcmp(cstr,"/camera_meio/led/compressed")==0){
		std::swap(cornersf, corners);
		std::swap(prevcornersf, prevcorners);
		cv::swap(imagef, image);
		cv::swap(previmagef, previmage);
	}
	if(strcmp(cstr,"/camera_direita/led/compressed")==0){
		std::swap(cornersd, corners);
		std::swap(prevcornersd, prevcorners);
		cv::swap(imaged, image);
		cv::swap(previmaged, previmage);
	}
	if(strcmp(cstr,"/camera_esquerda/led/compressed")==0){
		std::swap(cornerse, corners);
		std::swap(prevcornerse, prevcorners);
		cv::swap(imagee, image);
		cv::swap(previmagee, previmage);
	}

  }
  catch (cv_bridge::Exception& e)
  {
    ROS_ERROR("Could not convert image!");
  }
}

//----------------------- HELP PAGE

void fhelp(void){
	printf("\n---------------- [HELP PAGE] ----------------");
	printf("\n[Arguments]");
	printf("\n-bag [topic name] : used to subscribe to a specific topic (video feed must be compressed)");
	printf("\n-maxfcnt [number] : max features count allowed");
	printf("\n-pfloc            : print feature location in video feed");
	printf("\n-vfeed            : enable video feed");
	printf("\n-oflow            : enable video feed with optical flow");
	printf("\n-save             : save frames in a folder");	
	printf("\n---------------------------------------------\n\n");
	return;
}


//----------------------- MAIN

int main(int argc, char **argv)
{
 
  if(argc > 0){
	for(int i=0; i < argc; i++){
		if (strcmp(argv[i], "-bag") == 0){
			rosbagstring = argv[i+1];			
			cout << "STRING:" << endl << rosbagstring << endl << endl;
		}
		if (strcmp(argv[i], "-maxfcnt") == 0)MAX_FEATURE_COUNT = atoi(argv[i+1]);
		if (strcmp(argv[i], "-pfloc") == 0)PRINT_FEATURES_LOCATION = 1;
		if (strcmp(argv[i], "-vfeed") == 0)SHOW_VIDEO_FEED = 1;
		if (strcmp(argv[i], "-oflow") == 0)SHOW_FLOW = 1;
		if (strcmp(argv[i], "-save") == 0)SAVE_FRAMES = 1;
		if (strcmp(argv[i], "-help") == 0){fhelp();return 0;}
	}
  }

/*
if((rosbagstring.empty()) && (helppage = 0)){
	printf("\n\nERROR: add '-bag [topic name]' as arguments to subscribe to a video feed topic. \nVideo must be compressed\n\n");
	return 0;
}
*/

	const char *cstr = rosbagstring.c_str();
	if(strcmp(cstr,"/camera_meio/led/compressed")==0)nodename = "camera_meio"; 
	if(strcmp(cstr,"/camera_direita/led/compressed")==0)nodename = "camera_direita"; 
	if(strcmp(cstr,"/camera_esquerda/led/compressed")==0)nodename = "camera_esquerda"; 

  ros::init(argc, argv, nodename);
  ros::NodeHandle nh;
  //cv::namedWindow("view");
  cv::startWindowThread();//"/camera_meio/led/compressed"
  ros::Subscriber sub = nh.subscribe(rosbagstring, 1, imageCallback);
  ros::Publisher pub = nh.advertise<std_msgs::Float32MultiArray>("uwv_rt_data", 1000);
  //ros::Publisher pub = nh.advertise<std_msgs::Float32MultiArray>("uwv_2d_data", 1000);
  

  printf("\n\nWaiting for video input with '/camera_meio/led/compressed' topic...\n");
  ros::spin();
  //cv::destroyWindow("view");
}


