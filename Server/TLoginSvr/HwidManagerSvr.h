#pragma once
#ifndef HWID_MANAGER_SVR_H
#define HWID_MANAGER_SVR_H

#include <atlstr.h>

//Supress "Consider XXX_s Method" Warnings ...
#define _CRT_SECURE_NO_WARNINGS


#define HWID_WINDEVICE_SEGMENT 0
#define HWID_WINSERIAL_SEGMENT 1
#define HWID_WINUSER_SEGMENT 2
#define HWID_MOBO_SEGMENT 4
#define HWID_CPU_SEGMENT 8

class HwidManagerSvr
{
public:
	CString WinDeviceSegment;
	CString WinSerialSegment;
	CString WinUserSegment;
	CString MoboSegment;
	CString CpuSegment;

public:
	BOOL GetSegmentChecksum(DWORD hwidParams, CString& hwidChecksumResult);
};
#endif