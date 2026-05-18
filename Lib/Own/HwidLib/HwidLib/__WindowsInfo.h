#pragma once
#ifndef __WINDOWSINFO_H
#define __WINDOWSINFO_H

#include <vector>
#include <string>

#include "__SysCommand.h"

class __WindowsInfo
{
public:
	__WindowsInfo::__WindowsInfo(const std::vector<std::string> &rawData, int OSNumber);

	std::string name() const;
	std::string manufacturer() const;
	std::string architecture() const;
	std::string caption() const;
	std::string version() const;
	std::string currentUser() const;
	std::string installDate() const;
	std::string bootDevice() const;
	std::string serialNumber() const;
	std::string totalVisibleMemory() const;
	int osNumber() const;

private:
	std::string _name;
	std::string _manufacturer;
	std::string _architecture;
	std::string _caption;
	std::string _version;
	std::string _currentUser;
	std::string _installDate;
	std::string _bootDevice;
	std::string _serialNumber;
	std::string _totalVisibleMemory;

	int _osNumber;

	template <typename T>
	std::string toString(const T &convert) const
	{
		std::stringstream transfer;
		std::string returnString;
		transfer << convert;
		transfer >> returnString;
		return returnString;
	}

	static const std::string NAME_IDENTIFIER_STRING;
	static const std::string MANUFACTURER_IDENTIFIER_STRING;
	static const std::string ARCHITECTURE_IDENTIFIER_STRING;
	static const std::string CAPTION_INDENTIFIER_STRING;
	static const std::string VERSION_IDENTIFIER_STRING;
	static const std::string CURRENT_USER_IDENTIFIER_STRING;
	static const std::string INSTALL_DATE_IDENTIFIER_STRING;
	static const std::string BOOT_DEVICE_IDENTIFIER_STRING;
	static const std::string TOTAL_VISIBLE_MEMORY_SIZE_IDENTIFIER_STRING;
	static const std::string SERIAL_NUMBER_IDENTIFIER_STRING;

	static const int KILOBYTES_PER_MEGABYTE;
};
#endif

#ifndef OSInfoDelegate_H
#define OSInfoDelegate_H
#include "__SysCommand.h"

class __WindowsInfoDelegate
{
public:
	__WindowsInfoDelegate();
	std::vector<__WindowsInfo> winInfoVector() const;
	int numberOfOSInfoItems() const;
private:
	std::vector<__WindowsInfo> _winInfoVector;
	int _numberOfOSInfoItems;

	void determineNumberOfOSInfoItems(const std::vector<std::string> &data);
	static const std::string OS_INFO_QUERY_STRING;
	static const std::string OS_INSTANCE_QUERY_STRING;
	static const std::string OS_INFO_END_IDENTIFIER_STRING;
};
#endif