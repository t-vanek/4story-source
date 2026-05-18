#include "stdafx.h"
#include "HwidManagerSvr.h"


BOOL HwidManagerSvr::GetSegmentChecksum(DWORD hwidParams, CString& hwidChecksumResult)
{
	if (hwidParams != NULL)
	{
		if ((hwidParams & HWID_WINDEVICE_SEGMENT) == HWID_WINDEVICE_SEGMENT)
		{
			if (this->WinDeviceSegment)
			{
				hwidChecksumResult += this->WinDeviceSegment;
			}
			else
			{
				return FALSE;
			}
		}

		if ((hwidParams & HWID_WINSERIAL_SEGMENT) == HWID_WINSERIAL_SEGMENT)
		{
			if (this->WinSerialSegment)
			{
				hwidChecksumResult += this->WinSerialSegment;
			}
			else
			{
				return FALSE;
			}
		}

		if ((hwidParams & HWID_WINUSER_SEGMENT) == HWID_WINUSER_SEGMENT)
		{
			if (this->WinUserSegment)
			{
				hwidChecksumResult += this->WinUserSegment;
			}
			else
			{
				return FALSE;
			}
		}

		if ((hwidParams & HWID_MOBO_SEGMENT) == HWID_MOBO_SEGMENT)
		{
			if (this->MoboSegment)
			{
				hwidChecksumResult += this->MoboSegment;
			}
			else
			{
				return FALSE;
			}
		}

		if ((hwidParams & HWID_CPU_SEGMENT) == HWID_CPU_SEGMENT)
		{
			if (this->CpuSegment)
			{
				hwidChecksumResult += this->CpuSegment;
			}
			else
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	return FALSE;
}
