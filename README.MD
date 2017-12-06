**A Simple Lib for Serial Port on Windows**
===========================================

Usage:
-------

```C++

#include "serialport.h"
#include <iostream>

#pragma comment(lib, "serialport.lib")

using namespace LuHang;

/* When data is comming, Lib calls this function */
void Callback(const char* data, int len)
{
	// to-do with recved data
}

int main()
{

	SerialPort sp(&Callback);
	
	bool ret = sp.open("COM1");

	// to-do with sp
	// sp.write(buf, len);
	
	std::cin.get();

	return 0;
}

```

