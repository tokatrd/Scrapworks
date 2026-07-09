#include "agora_utils.h"
#include "lvs_greenscreen.h"
#include "obs.hpp"
#include "lovense_obs_proc.hpp"

static std::shared_ptr<LvsGreenScreen> singleton = nullptr;
static std::once_flag singletonFlag;

std::shared_ptr<LvsGreenScreen> LvsGreenScreen::getInstance()
{
	std::call_once(singletonFlag,
		[&] { singleton = std::shared_ptr<LvsGreenScreen>(new LvsGreenScreen()); });
	return singleton;
}


void LvsGreenScreen::setHsvThreshold(HsvColor hsv)
{
	this->hsvLow = hsv;
}

void LvsGreenScreen::setXyThreshold(const XyThreshold& xyt)
{
	this->xyThd = xyt;
}

void LvsGreenScreen::setGreenScreenType(int type)
{
	this->greenScreenType = type;
}

void LvsGreenScreen::setbgFrame(InputArray _bgFrame)
{
#ifdef USE_UMAT
	cv::UMat frame;
	_bgFrame.getMat().copyTo(frame);
	std::lock_guard<std::mutex> lk{ bg_frame_mutex_ };
	this->bgFrame = frame;
#else
	std::lock_guard<std::mutex> lk{ bg_frame_mutex_ };
	this->bgFrame = _bgFrame.getMat().clone();
#endif
	
}

bool LvsGreenScreen::isEmpty()
{
	std::lock_guard<std::mutex> lk{ bg_frame_mutex_ };
	return this->bgFrame.empty();
}

#ifdef USE_UMAT
cv::UMat LvsGreenScreen::getGreenScreenFrame(InputArray _frame)
#else
cv::Mat LvsGreenScreen::getGreenScreenFrame(InputArray _frame)
#endif
{
#ifdef USE_UMAT
	cv::UMat frame = _frame.getUMat();
#else
	Mat frame = _frame.getMat();
#endif
	if (!Status || frame.empty())
		return frame;

	{
		std::lock_guard<std::mutex> lk{ bg_frame_mutex_ };
		if (bgFrame.empty())
			return frame;
	}

#ifdef USE_UMAT
	cv::UMat result;
#else
	Mat result;
#endif
	
#ifdef USE_UMAT
	cv::UMat mask = getMask(AgoraUtils::getInstance()->getProcessedVideoUFrame());
#else
	Mat	mask = getMask(AgoraUtils::getInstance()->getProcessedVideoFrame());	
#endif	
	if (!mask.empty()) {

		if (1 == greenScreenType || (!green_area_.valid())) {

			vector<RotatedRect> rectangleVec = findRectangle(mask);
			std::array<int, 4> data;
			findGreenScreenPos(rectangleVec, data);
			green_area_.set_data(data);			
		}

		if ((greenScreenType == 0) && reseting_)
		{
			reseting_ = false;

			this->green_area_.reset();
			this->lask_ok_area_.reset();
			this->xyThd.reset();			
		}
		else
		{
			if (lask_ok_area_.valid() && (!green_area_.valid())) {
				green_area_.set_data(lask_ok_area_.get_data());
			}
		}

		if (!green_area_.valid()) {
			result = frame;
		}
		else {
			auto maxGs = green_area_.get_data();
			auto adjustGs = maxGs;
			int adjustXStart = adjustGs[0] + xyThd.xMin;
			int adjustXEnd = adjustGs[2] - xyThd.xMax;
			int adjustYStart = adjustGs[1] + xyThd.yMin;
			int adjustYEnd = adjustGs[3] - xyThd.yMax;
			int xMid = (adjustGs[2] - adjustGs[0]) / 2 + adjustXStart;
			int yMid = (adjustGs[3] - adjustGs[1]) / 2 + adjustYStart;
			adjustGs[0] = adjustXStart > (xMid - 25) ? xMid - 25 : adjustXStart;
			adjustGs[1] = adjustYStart > (yMid - 25) ? yMid - 25 : adjustYStart;
			adjustGs[2] = adjustXEnd < (xMid + 25) ? xMid + 25 : adjustXEnd;
			adjustGs[3] = adjustYEnd < (yMid + 25) ? yMid + 25 : adjustYEnd;

#ifdef USE_UMAT
			cv::UMat bgPic;
#else
			Mat bgPic;
#endif
			{
				std::lock_guard<std::mutex> lk{ bg_frame_mutex_ };
				resize(bgFrame, bgPic,
					Size(adjustGs[2] - adjustGs[0],
						adjustGs[3] - adjustGs[1]),
					0, 0, INTER_LINEAR);
			}

#ifdef USE_UMAT
			auto last_Ok_pic = replace_and_blend(frame.getMat(cv::AccessFlag::ACCESS_READ).clone(), mask, bgPic, maxGs, adjustGs);
			last_Ok_pic.copyTo(result);
#else
			//last_Ok_pic = result = replace_and_blend(frame, mask, bgPic, maxGs, adjustGs);
			result = replace_and_blend(frame, mask, bgPic, maxGs, adjustGs);
#endif
			lask_ok_area_.set_data(maxGs);
		}
	}
	else {
		result = frame;
	}

	return result;
}


#ifdef USE_UMAT
cv::UMat LvsGreenScreen::getMask(cv::UMat _frame)
{
	cv::UMat mask;
	cv::UMat frame = _frame;
	if (!frame.empty()) {
		cv::UMat hsv, mask_close;
		cvtColor(frame, hsv, COLOR_BGR2HSV);
		{
			//std::lock_guard<std::mutex> lk{ attr_mutex_ };
			inRange(hsv, Scalar(hsvLow.h, hsvLow.s, hsvLow.v), Scalar(hsvHigh.h, hsvHigh.s, hsvHigh.v),
				mask);
		}

		Mat k = getStructuringElement(MORPH_ELLIPSE, Size(1, 1), Point(-1, -1)); //MORPH_ELLIPSE
		morphologyEx(mask, mask_close, MORPH_OPEN, k);                           //MORPH_OPEN
		erode(mask_close, mask_close, k);
		GaussianBlur(mask_close, mask, Size(1, 1), 0, 0, BORDER_DEFAULT); //BORDER_DEFAULT
	}
	return mask;
}

vector<RotatedRect> LvsGreenScreen::findRectangle(cv::UMat _frame) {
	vector<RotatedRect> result;
	if (!_frame.empty()) {
		auto frame = _frame;

		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(frame, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

		//Mat drawing = Mat::zeros(frame.size(), CV_8UC3);

		for (size_t i = 0; i < contours.size(); i++) {
			RotatedRect rectangle = minAreaRect(contours[i]);
			double      areaSize  = contourArea(contours[i]);

			if (rectangle.size.width < 100 || rectangle.size.height < 100)
				continue;

			double val = abs(areaSize / (rectangle.size.width * rectangle.size.height) - 1);
			if (val > 0.3)
				continue;

			result.push_back(rectangle);
		}
	}

	return result;
}

#else

Mat LvsGreenScreen::getMask(InputArray _frame)
{
	Mat mask;
	Mat frame = _frame.getMat();
	if (!frame.empty()) {
		Mat hsv, mask_close;
		cvtColor(frame, hsv, COLOR_BGR2HSV);
		{
			//std::lock_guard<std::mutex> lk{ attr_mutex_ };
			inRange(hsv, Scalar(hsvLow.h, hsvLow.s, hsvLow.v), Scalar(hsvHigh.h, hsvHigh.s, hsvHigh.v),
				mask);
		}


		Mat k = getStructuringElement(MORPH_ELLIPSE, Size(1, 1), Point(-1, -1)); //MORPH_ELLIPSE
		morphologyEx(mask, mask_close, MORPH_OPEN, k);                           //MORPH_OPEN
		erode(mask_close, mask_close, k);
		GaussianBlur(mask_close, mask, Size(1, 1), 0, 0, BORDER_DEFAULT); //BORDER_DEFAULT
	}

	return mask;
}

vector<RotatedRect> LvsGreenScreen::findRectangle(InputArray _frame) {
	vector<RotatedRect> result;
	if (!_frame.empty()) {
		cv::Mat frame = _frame.getMat();

		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(frame, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);

		//Mat drawing = Mat::zeros(frame.size(), CV_8UC3);

		for (size_t i = 0; i < contours.size(); i++) {
			//if (contours[i].size() > 100)
			//	continue;

			RotatedRect rectangle = minAreaRect(contours[i]);
			double areaSize = contourArea(contours[i]);

			if (rectangle.size.width < 100 || rectangle.size.height < 100)
				continue;

			if (abs(areaSize / (rectangle.size.width * rectangle.size.height) - 1) > 0.3)
				continue;

			result.push_back(rectangle);

			//drawContours(drawing, contours, (int)i, Scalar(0, 0, 255), 1, LINE_AA, hierarchy, 0);
		}
	}

	return result;
}

#endif

bool LvsGreenScreen::findGreenScreenPos(const vector<RotatedRect>& rectVec, std::array<int, 4>& result)
{
	//Mat result = (Mat_<int>(1, 4) << -1, -1, -1, -1); //minX,minY,maxX,maxY    x,y position
	result = std::array<int, 4>{-1, -1, -1, -1};

	if (!rectVec.empty()) {
		RotatedRect maxGS = rectVec[0];
		for (size_t i = 1; i < rectVec.size(); i++) {
			if (rectVec[i].size.width < 50 || rectVec[i].size.height < 50)
				continue;

			if (maxGS.size.width * maxGS.size.height < rectVec[i].size.width * rectVec[i].size.height)
				maxGS = rectVec[i];
		}

		if (maxGS.size.width >= 50 && maxGS.size.height >= 50) {
			if (maxGS.angle < 40) {
				result[0] = max(0, int(maxGS.center.x - maxGS.size.width / 2));
				result[1] = max(0, int(maxGS.center.y - maxGS.size.height / 2));
				result[2] = int(maxGS.center.x + maxGS.size.width / 2);
				result[3] = int(maxGS.center.y + maxGS.size.height / 2);
			}
			else if (maxGS.angle > 50) {
				result[0] = max(0, int(maxGS.center.x - maxGS.size.height / 2));
				result[1] = max(0, int(maxGS.center.y - maxGS.size.width / 2));
				result[2] = int(maxGS.center.x + maxGS.size.height / 2);
				result[3] = int(maxGS.center.y + maxGS.size.width / 2);
			}

#if 0
			Point2f vertices[4];
			maxGS.points(vertices);
			for (int i = 0; i < 4; i++)
				line(frame, vertices[i], vertices[(i + 1) % 4], Scalar(0, 255, 0), 2);

			circle(frame, maxGS.center, 5, Scalar(0, 255, 0), -1);

			circle(frame, Point2f(result[0], result[1]), 5, Scalar(0, 255, 0), -1);
			circle(frame, Point2f(result[2], result[1]), 5, Scalar(0, 255, 0), -1);
			circle(frame, Point2f(result[0], result[3]), 5, Scalar(0, 255, 0), -1);
			circle(frame, Point2f(result[2], result[3]), 5, Scalar(0, 255, 0), -1);

			//Point2f vertices_1[4] = { result[0], result[1], result[2], result[3] };

			Rect brect = maxGS.boundingRect();
			rectangle(frame, brect, Scalar(255, 0, 0), 2);

			imshow("rectangles", frame);
			waitKey(0);
#endif
		}
	}

	return result[0] >= 0 && result[1] >= 0 && result[2] > 0 && result[3] > 0;
}

cv::Mat LvsGreenScreen::replace_and_blend(InputArray _frame, const InputArray _mask, const InputArray _bgPic,
	const std::array<int, 4>& gs,
	const std::array<int, 4>& adjGs)
{
	auto result = _frame.getMat();
	auto bgPic  = _bgPic.getMat();
	auto mask   = _mask.getMat();

	if (bgPic.empty() || (!GreenScreenArea::is_valid_gs_pos(gs)) || (!GreenScreenArea::is_valid_gs_pos(adjGs))) {
		return result;
	}

	int m = 0;
	int h = gs[3] - gs[1]; // bgPic.rows;
	int w = gs[2] - gs[0]; // bgPic.cols;

	int xStart = gs[0];
	int yStart = gs[1];

	int xMinAdj = adjGs[0] - gs[0];
	int xMaxAdj = adjGs[2] - gs[0]; //w
	int yMinAdj = adjGs[1] - gs[1];
	//int yMaxAdj = adjGs.at<int>(3);
#if 0
	blog(LOG_INFO, "gs [%d,%d,%d,%d], adjGs[%d,%d,%d,%d], w:%d,h:%d bgPic[%d,%d],mask[%d,%d]"
		, gs[0],gs[1],gs[2],gs[3], adjGs[0], adjGs[1], adjGs[2], adjGs[3], w, h,
		bgPic.rows, bgPic.cols, mask.rows, mask.cols);
#endif

	
	uchar* bgrow = nullptr;
	for (int row = 0; row < (h - 1); row++) {
		if (row >= yMinAdj && row < (bgPic.rows + yMinAdj)) {
			bgrow = bgPic.ptr<uchar>(row - yMinAdj);
		}
		else {
			bgrow = nullptr;
		}

		uchar* maskrow = nullptr;
		uchar* targetrow = nullptr;

		int y = row + yStart;
		if (y < 0 || ((unsigned)y >= (unsigned)mask.rows) || ((unsigned)y >= (unsigned)result.rows)) {
			break;
		}

		maskrow   = mask.ptr<uchar>(y) + xStart;
		targetrow = result.ptr<uchar>(row + yStart) + (xStart * 3);

		for (int col = 0; col < (w - 1); col++) {
			m = *maskrow++;
			if ((col >= xMinAdj) && (col <= xMaxAdj) && (bgrow != nullptr)) {
				if (m == 255) { // background
					memmove(targetrow, bgrow, 3);
				}
				bgrow += 3;
			}

			targetrow += 3;
		}
	}

	return result;
}

void LvsGreenScreen::setStatus(bool status)
{
	if (status != this->Status) {
		this->Status = status;
		reseting_    = true;

		lovense::proc::report_plugin_log("P0400", "status: %d", status);
	}
}

bool LvsGreenScreen::getStatus()
{
	return this->Status;
}

void LvsGreenScreen::reset()
{
	reseting_ = true;
}
