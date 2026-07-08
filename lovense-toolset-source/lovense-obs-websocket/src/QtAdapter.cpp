#include "QtAdapter.h"

#ifdef DISABLE_QT
QString::QString(const char* s)
{
	if (s == nullptr)
	{
		is_null_ = true;
	}
	else
	{
		text_.assign(s);
	}
}

QString::QString(const std::string& s)
{
	text_ = s;
}

void QString::prepend(char ch)
{
	text_.insert(text_.begin(), ch);
	if (!is_null_) {
		is_null_ = true;
	}
}

void QString::prepend(const char* s)
{
	if (!s) return;

	text_ = s + text_;
	if (!is_null_) {
		is_null_ = true;
	}
}

QString QString::fromStdString(const std::string& s)
{
	return QString(s);
}


bool operator==(const QString& one, const QString& other)
{
	return ((one.is_null_ == other.is_null_) && (one.text_ == other.text_));
}

bool operator!=(const QString& one, const QString& other)
{
	return ((one.is_null_ != other.is_null_) || (one.text_ != other.text_));
}

void QString::operator+=(const QString& s)
{
	if (!s.is_null_) {
		text_ += s.text_;
	}
}

QString QString::operator+(const QString& s)
{
	QString str;
	str.is_null_ = this->is_null_ && s.is_null_;
	str.text_ = this->text_ + s.text_;

	return str;
}

bool operator<(const QString& l, const QString& r)
{
	return l.text_ < r.text_;
}
#endif
