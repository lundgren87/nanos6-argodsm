/*
	This file is part of Nanos6 and is licensed under the terms contained in the COPYING file.

	Copyright (C) 2015-2020 Barcelona Supercomputing Center (BSC)
*/

#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <cassert>
#include <vector>

#include <nanos6.h>

class DeviceComputePlace;

class Device {

	std::vector<DeviceComputePlace*> _places;
	nanos6_device_t _devType;
	int _subType;

public:

	Device(nanos6_device_t type, int subType);
	~Device();

	void addComputePlace(DeviceComputePlace *computePlace);

	DeviceComputePlace *getComputePlace(int idx)
	{
		return _places[idx];
	}

	std::vector<DeviceComputePlace *> &getComputePlaces()
	{
		return _places;
	}

	inline int getNumDevices()
	{
		return _places.size();
	}

	inline nanos6_device_t getDeviceType()
	{
		return _devType;
	}

	inline int getDeviceSubType()
	{
		return _subType;
	}
};

#endif // DEVICE_HPP