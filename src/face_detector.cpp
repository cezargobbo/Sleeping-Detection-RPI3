#include "face_detector.h"
#include <iostream>
#include <opencv2/imgproc.hpp>

const double FaceDetector::TICK_FREQUENCY = cv::getTickFrequency();

double FaceDetector::get_time(void) const
{
	struct timeval tv;
	double t;

	if (gettimeofday(&tv, NULL) < 0)
		printf("get_time encountered error in gettimeofday : %s\n", strerror(errno));
	t = tv.tv_sec + tv.tv_usec/1000000.0;
	return t;
}

double euclidean_distance(double x1, double y1, double x2, double y2)
{
    double sqr_ld, sqrt_ld;

    sqr_ld = ((x1 - x2) * (x1 - x2)) + ((y1 - y2) * (y1 - y2));
    sqrt_ld = sqrtl(sqr_ld);

    return sqrt_ld;
}

FaceDetector::FaceDetector()
{
	m_scale = 0;
	m_frontal_face_detector = dlib::get_frontal_face_detector();
}

FaceDetector::FaceDetector(const std::string cascadeFilePath)
{
    setFaceCascade(cascadeFilePath);
}


void FaceDetector::setFaceCascade(const std::string cascadeFilePath)
{
    if (m_faceCascade == NULL) {
        m_faceCascade = new cv::CascadeClassifier(cascadeFilePath);
    }
    else {
        m_faceCascade->load(cascadeFilePath);
    }

    if (m_faceCascade->empty()) {
        std::cerr << "Error creating cascade classifier. Make sure the file" << std::endl
            << cascadeFilePath << " exists." << std::endl;
    }
}

cv::CascadeClassifier *FaceDetector::faceCascade() const
{
    return m_faceCascade;
}

void FaceDetector::setResizedWidth(const int width)
{
    m_resizedWidth = std::max(width, 1);
}

int FaceDetector::resizedWidth() const
{
    return m_resizedWidth;
}

bool FaceDetector::isFaceFound() const
{
	return m_foundFace;
}

void FaceDetector::resetFace()
{
    m_foundFace = false;
}

cv::Rect FaceDetector::face() const
{
    cv::Rect faceRect = m_trackedFace;
    faceRect.x = (int)(faceRect.x / m_scale);
    faceRect.y = (int)(faceRect.y / m_scale);
    faceRect.width = (int)(faceRect.width / m_scale);
    faceRect.height = (int)(faceRect.height / m_scale);
    return faceRect;
}

cv::Point FaceDetector::facePosition() const
{
    cv::Point facePos;
    facePos.x = (int)(m_facePosition.x / m_scale);
    facePos.y = (int)(m_facePosition.y / m_scale);
    return facePos;
}

void FaceDetector::setTemplateMatchingMaxDuration(const double s)
{
    m_templateMatchingMaxDuration = s;
}

double FaceDetector::templateMatchingMaxDuration() const
{
    return m_templateMatchingMaxDuration;
}

FaceDetector::~FaceDetector()
{
    if (m_faceCascade != NULL) {
        delete m_faceCascade;
    }
}

cv::Rect FaceDetector::doubleRectSize(const cv::Rect &inputRect, const cv::Rect &frameSize) const
{
    cv::Rect outputRect;
    // Double rect size
    outputRect.width = inputRect.width * 2;
    outputRect.height = inputRect.height * 2;

    // Center rect around original center
    outputRect.x = inputRect.x - inputRect.width / 2;
    outputRect.y = inputRect.y - inputRect.height / 2;

    // Handle edge cases
    if (outputRect.x < frameSize.x) {
        outputRect.width += outputRect.x;
        outputRect.x = frameSize.x;
    }
    if (outputRect.y < frameSize.y) {
        outputRect.height += outputRect.y;
        outputRect.y = frameSize.y;
    }

    if (outputRect.x + outputRect.width > frameSize.width) {
        outputRect.width = frameSize.width - outputRect.x;
    }
    if (outputRect.y + outputRect.height > frameSize.height) {
        outputRect.height = frameSize.height - outputRect.y;
    }

    return outputRect;
}

cv::Point FaceDetector::centerOfRect(const cv::Rect &rect) const
{
    return cv::Point(rect.x + rect.width / 2, rect.y + rect.height / 2);
}

cv::Rect FaceDetector::biggestFace(std::vector<cv::Rect> &faces) const
{
    assert(!faces.empty());

    cv::Rect biggest = faces[0];
    for (int i = 0; i < faces.size(); i++) {
        if (faces[i].area() > biggest.area())
            biggest = faces[i];
    }
    return biggest;
}

/*
* Face template is small patch in the middle of detected face.
*/
cv::Mat FaceDetector::getFaceTemplate(const cv::Mat &frame, cv::Rect face)
{
    face.x += face.width / 4;
    face.y += face.height / 4;
    face.width /= 2;
    face.height /= 2;

    cv::Mat faceTemplate = frame(face).clone();
    return faceTemplate;
}

void FaceDetector::detectFaceAllSizes(const cv::Mat &frame)
{
    static double detection_start = get_time();
    static bool detection_started = false;

    // Minimum face size is 1/5th of screen height
    // Maximum face size is 2/3rds of screen height
	if(m_faceCascade != NULL)
	{
		m_faceCascade->detectMultiScale(frame, m_allFaces, 1.1, 3, 0,
			cv::Size(frame.rows / 5, frame.rows / 5),
			cv::Size(frame.rows * 2 / 3, frame.rows * 2 / 3));
	}
//	else
//	{
//    as1_log_info("DLIB: %d %lu", m_foundFace, m_allFaces.size());
//		dlib::cv_image<unsigned char> cimg(frame);
//		std::vector<dlib::rectangle> dets = m_frontal_face_detector(cimg);
//
//		for(int i = 0; i < dets.size(); i++)
//			m_allFaces.push_back(cv::Rect(dets[i].left(), dets[i].top(), (dets[i].right() - dets[i].left()), (dets[i].bottom() - dets[i].top())));
//	}

    if (m_allFaces.empty()) {
        detection_start = get_time();
        return;
    }

    // Locate biggest face
    m_trackedFace = biggestFace(m_allFaces);

    // Copy face template
    m_faceTemplate = getFaceTemplate(frame, m_trackedFace);

    // Calculate roi
    m_faceRoi = doubleRectSize(m_trackedFace, cv::Rect(0, 0, frame.cols, frame.rows));

    // Update face position
    m_facePosition = centerOfRect(m_trackedFace);


    double dist_from_previous_face = 0;
    if(m_facePositionPrevious.x != 0 && m_facePositionPrevious.y != 0 )
        dist_from_previous_face = euclidean_distance(m_facePosition.x, m_facePosition.y, m_facePositionPrevious.x, m_facePositionPrevious.y);

    if(dist_from_previous_face > (m_faceRoi.width / 2) ) {
        detection_start = get_time();
        m_facePositionPrevious = m_facePosition;
        return;
    } else {
        double time_diff = get_time() - detection_start;
        if(time_diff < 1.5) {
            return;
        } else {
            m_foundFace = true;
        }
    }

    m_facePositionPrevious = m_facePosition;
}

void FaceDetector::detectFaceAroundRoi(const cv::Mat &frame)
{
    // Detect faces sized +/-20% off biggest face in previous search
	if(m_faceCascade != NULL)
	{
		m_faceCascade->detectMultiScale(frame(m_faceRoi), m_allFaces, 1.3, 3, 0,
			cv::Size(m_trackedFace.width * 7 / 10, m_trackedFace.height * 7 / 10),
			cv::Size(m_trackedFace.width * 13 / 10, m_trackedFace.width * 13 / 10));
	}
	else
	{
		dlib::cv_image<unsigned char> cimg(frame(m_faceRoi));
		std::vector<dlib::rectangle> dets = m_frontal_face_detector(cimg);

		for(int i = 0; i < dets.size(); i++)
			m_allFaces.push_back(cv::Rect(dets[i].left(), dets[i].top(), (dets[i].right() - dets[i].left()), (dets[i].bottom() - dets[i].top())));
	}

    if (m_allFaces.empty())
    {
        // Activate template matching if not already started and start timer
        m_templateMatchingRunning = true;
        if (m_templateMatchingStartTime == 0)
            m_templateMatchingStartTime = cv::getTickCount();
        return;
    }

    // Turn off template matching if running and reset timer
    m_templateMatchingRunning = false;
    m_templateMatchingCurrentTime = m_templateMatchingStartTime = 0;

    // Get detected face
    m_trackedFace = biggestFace(m_allFaces);

    // Add roi offset to face
    m_trackedFace.x += m_faceRoi.x;
    m_trackedFace.y += m_faceRoi.y;

    // Get face template
    m_faceTemplate = getFaceTemplate(frame, m_trackedFace);

    // Calculate roi
    m_faceRoi = doubleRectSize(m_trackedFace, cv::Rect(0, 0, frame.cols, frame.rows));

    // Update face position
    m_facePosition = centerOfRect(m_trackedFace);
}

void FaceDetector::detectFacesTemplateMatching(const cv::Mat &frame)
{
    // Calculate duration of template matching
    m_templateMatchingCurrentTime = cv::getTickCount();
    double duration = (double)(m_templateMatchingCurrentTime - m_templateMatchingStartTime) / TICK_FREQUENCY;

    // If template matching lasts for more than 2 seconds face is possibly lost
    // so disable it and redetect using cascades
    if (duration > m_templateMatchingMaxDuration) {
        m_foundFace = false;
        m_templateMatchingRunning = false;
        m_templateMatchingStartTime = m_templateMatchingCurrentTime = 0;
		m_facePosition.x = m_facePosition.y = 0;
		m_trackedFace.x = m_trackedFace.y = m_trackedFace.width = m_trackedFace.height = 0;
		return;
    }

	// Edge case when face exits frame while 
	if (m_faceTemplate.rows * m_faceTemplate.cols == 0 || m_faceTemplate.rows <= 1 || m_faceTemplate.cols <= 1) {
		m_foundFace = false;
		m_templateMatchingRunning = false;
		m_templateMatchingStartTime = m_templateMatchingCurrentTime = 0;
		m_facePosition.x = m_facePosition.y = 0;
		m_trackedFace.x = m_trackedFace.y = m_trackedFace.width = m_trackedFace.height = 0;
		return;
	}

    // Template matching with last known face 
    //cv::matchTemplate(frame(m_faceRoi), m_faceTemplate, m_matchingResult, CV_TM_CCOEFF);
    cv::matchTemplate(frame(m_faceRoi), m_faceTemplate, m_matchingResult, CV_TM_SQDIFF_NORMED);
    cv::normalize(m_matchingResult, m_matchingResult, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
    double min, max;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(m_matchingResult, &min, &max, &minLoc, &maxLoc);

    // Add roi offset to face position
    minLoc.x += m_faceRoi.x;
    minLoc.y += m_faceRoi.y;

    // Get detected face
    //m_trackedFace = cv::Rect(maxLoc.x, maxLoc.y, m_trackedFace.width, m_trackedFace.height);
    m_trackedFace = cv::Rect(minLoc.x, minLoc.y, m_faceTemplate.cols, m_faceTemplate.rows);
    m_trackedFace = doubleRectSize(m_trackedFace, cv::Rect(0, 0, frame.cols, frame.rows));

    // Get new face template
    m_faceTemplate = getFaceTemplate(frame, m_trackedFace);

    // Calculate face roi
    m_faceRoi = doubleRectSize(m_trackedFace, cv::Rect(0, 0, frame.cols, frame.rows));

    // Update face position
    m_facePosition = centerOfRect(m_trackedFace);
}

cv::Point FaceDetector::Detect(cv::Mat frame)
{
	m_allFaces.clear();

    // Downscale frame to m_resizedWidth width - keep aspect ratio
    m_scale = (double) std::min(m_resizedWidth, frame.cols) / frame.cols;
    cv::Size resizedFrameSize = cv::Size((int)(m_scale*frame.cols), (int)(m_scale*frame.rows));

    cv::Mat resizedFrame;
    cv::resize(frame, resizedFrame, resizedFrameSize);

    //cv::cvtColor(resizedFrame, resizedFrame, CV_BGR2GRAY);

    if (!m_foundFace)
    {
        detectFaceAllSizes(resizedFrame); // Detect using cascades over whole image
    }
    else {
	{
        detectFaceAroundRoi(resizedFrame); // Detect using cascades only in ROI
        if (m_templateMatchingRunning) {
            detectFacesTemplateMatching(resizedFrame); // Detect using template matching
        }
	}
    }

    return m_facePosition;
}
