#ifndef _LVS_GREENSCREEN_H_
#define _LVS_GREENSCREEN_H_

#include <iostream>
#include <mutex>
#include <array>


using namespace cv;
using namespace std;

struct HsvColor {
  int h;
  int s;
  int v;
  HsvColor(int h, int s, int v)
  {
    this->h = h;
    this->s = s;
    this->v = v;
  }
};

struct XyThreshold {
	int xMin{ 0 };
	int yMin{ 0 };
	int xMax{ 0 };
	int yMax{ 0 };

	XyThreshold() = default;

	XyThreshold(int xMin, int yMin, int xMax, int yMax)
	{
		this->xMin = xMin;
		this->yMin = yMin;
		this->xMax = xMax;
		this->yMax = yMax;
	}

	void reset()
	{
		this->xMin = 0;
		this->yMin = 0;
		this->xMax = 0;
		this->yMax = 0;
	}
};

class GreenScreenArea
{
public:
	inline static bool is_valid_gs_pos(const std::array<int, 4>& result)
	{
		//return !((result[0] == -1) && (result[1] == -1) && (result[2] == -1) && (result[3] == -1));
		return ((result[0] >= 0) && (result[1] >= 0));
	}

	void reset()
	{
		//std::lock_guard<std::mutex> lk(mutex_);
		data_ = { -1, -1, -1, -1 };
		valid_ = false;
	}

	bool valid() const
	{
		return valid_;
	}

	int get_h() const
	{
		return data_[3] - data_[1];
	}

	int get_w() const
	{
		return data_[2] - data_[0];
	}

	void set_data(std::array<int, 4> data)
	{
		valid_ = is_valid_gs_pos(data);
		//std::lock_guard<std::mutex> lk(mutex_);
		data_ = data;
	}

	std::array<int, 4> get_data()
	{
		//std::lock_guard<std::mutex> lk(mutex_);
		return data_;
	}
private:
	std::array<int, 4> data_{ -1,-1,-1,-1 };
	std::atomic_bool   valid_{ false };
	//std::mutex         mutex_;
};

//struct 

class LvsGreenScreen {
  public:
  static shared_ptr<LvsGreenScreen> getInstance();
    ~LvsGreenScreen(){};

    void setHsvThreshold(HsvColor hsv);
    void setXyThreshold(const XyThreshold& xyt);
    void setGreenScreenType(int type = 0);//0:Static  1:Dynamic
    void setbgFrame(InputArray _bgFrame);
	bool isEmpty();
	
    void setStatus(bool status);//on  , off
    bool getStatus();
	void reset();
#ifdef USE_UMAT
	cv::UMat getGreenScreenFrame(InputArray _frame);
#else
	cv::Mat getGreenScreenFrame(InputArray _frame);
#endif

  private:
    LvsGreenScreen(){};

#ifdef USE_UMAT	
	cv::UMat getMask(cv::UMat _frame);
	vector<RotatedRect> findRectangle(cv::UMat _frame);
#else	
    Mat getMask(InputArray _frame);
	vector<RotatedRect> findRectangle(InputArray _frame);
#endif
	
	bool findGreenScreenPos(const vector<RotatedRect>& rectVec, std::array<int, 4>& result);
    Mat replace_and_blend(InputArray _frame, const InputArray _mask, const InputArray _bgPic,
		const std::array<int, 4>& _gs,
		const std::array<int, 4>& _adjGs);

    HsvColor    hsvLow = HsvColor(50, 94, 49);
    HsvColor    hsvHigh = HsvColor(95, 255, 255);
	XyThreshold xyThd;

	std::mutex bg_frame_mutex_;
#ifdef USE_UMAT
	cv::UMat bgFrame;
#else
    Mat bgFrame;
#endif

	GreenScreenArea  green_area_;
	GreenScreenArea  lask_ok_area_;
    
    //Mat last_Ok_pic;
	std::atomic_int  greenScreenType = 0;
    std::atomic_bool Status    = false;
	std::atomic_bool reseting_ = false;

};

#endif // !_LVS_GREENSCREEN_H_
