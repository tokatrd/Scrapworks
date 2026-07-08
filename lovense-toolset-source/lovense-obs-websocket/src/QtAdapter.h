#pragma once

#include <unordered_map>
#include <set>

#ifdef DEF_TOOLSETS
#define DISABLE_QT
#endif

#ifndef DISABLE_QT
#include <QtCore/QString>
#include <QtCore/QFileInfo>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QThread>
#include <QtCore/QByteArray>
#include <QtConcurrent/QtConcurrent>
#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>
#include <QtCore/QVariantHash>
#include <QtCore/QThreadPool>
#include <QtCore/QObject>
#include <QtCore/QCryptographicHash>
#include <QtCore/QTime>
#include <QtGui/QImageWriter>
#include <QtGui/QImage>
#include <QRandomGenerator>
#include <QMainWindow>


#else

#include <map>
#include <string>
#include <filesystem>
#include <thread>
#include <random>

#include "lovense_string_helper.hpp"
#include "lovense_util_helper.hpp"
#include "common/ThreadPool.h"
#include "base64/base64.hpp"

using namespace lovense;

using quint16 = uint16_t;
using uint = uint32_t;

class QString
{
public:
	QString() = default;
	QString(const char* s);
	QString(const std::string& s);

	const char* toUtf8() const
	{
		return text_.c_str();
	}

	const std::string& str() const
	{
		return text_;
	}

	const char* c_str() const
	{
		return text_.c_str();
	}

	bool isEmpty() const
	{
		return text_.empty();
	}

	bool isNull() const
	{
		return is_null_;
	}

	template<class T>
	static QString number(T val)
	{
		return std::to_string(val);
	}

	int toInt() const
	{
		return atoi(text_.c_str());
	}

	void prepend(char ch);
	void prepend(const char* s);

	static QString fromStdString(const std::string& s);

	void operator+=(const QString& s);
	QString operator+(const QString& s);

	friend bool operator==(const QString& one, const QString& other);
	friend bool operator!=(const QString& one, const QString& other);
	friend bool operator<(const QString& l, const QString& r);
private:
	std::string text_;
	bool        is_null_{ false };
};

//bool operator!=(const QString& one, const QString& other);

#endif
