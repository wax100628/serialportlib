
/********************************************************
*
* Description:串口通用操作类
* 
* 
* @author ZhangJie
* @email  wax628@gmail.com
* 
* @version 1.0 2017-04-06 (init)
* @version 1.1 2017-04-25 (function read() improved)
* @version 1.2 2017-11-22 (add read callback function)
* @version 1.3 2017-12-06 (read(...) len is always 4 bytes - fixed)
* 
********************************************************/

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <thread>

#ifdef SERIALPORT_EXPORTS
#define SERIALPORT_API __declspec(dllexport)
#else
#define SERIALPORT_API __declspec(dllimport)
#endif

#pragma warning(disable: 4800)

typedef void (*fReadCallback)(const char*, int32_t);

namespace LuHang{

	/** 停止位 */
	enum SERIALPORT_API StopBits {ONEBIT, ONE5BITS, TWOBITS};

	/** 奇偶校验方法 */
	enum SERIALPORT_API Parity {NO, ODD, EVEN, MARK, SPACE};

	/** 流控类型 */
	enum SERIALPORT_API FlowControl{NONE, XON_XOFF, RTS_CTS, DSR_DTR};


	/** 串口类 */
	class SERIALPORT_API SerialPort
	{
	public:
		explicit SerialPort(fReadCallback pFun = nullptr);
		~SerialPort();

		/** setters(baudrate/bytesie/parity/stop bits/ ... and so on.) */
		bool setBaudRate(uint32_t baud_rate = 9600);							
		bool setBitsNum(uint32_t bits_size = 8);								
		bool setParity(Parity p = Parity::NO);										
		bool setParityEnable(bool enable = false);									
		bool setBinaryMode(bool enable = false);									
		bool setStopBits(StopBits sb = StopBits::ONEBIT);							
		bool setCommBufSize(uint32_t insize = 1024, uint32_t outsize = 1024);
		void setFlowControl(FlowControl fc = FlowControl::NONE);
		void setReadCallback(fReadCallback pFun);

		/** getters */
		int32_t getBaudRate() const;
		int32_t getBitsNum() const;
		uint32_t  getBytesInCom() const;
		/** err detail */
		const char* getError();

		/** open and close */
		bool open(const char *lpName);
		void close();

		/** is serialport opened? */
		bool isOpen() const;

		/** read and write */
		int32_t read(char *buf, uint32_t len);
		int32_t write(const char *buf2write, uint32_t len);

	private:
		char* wcharStrToCharStr(wchar_t* in);
		char* lpcwstrToString();
		inline void lock();
		inline void unlock();
		inline void asread();

	private:
		fReadCallback			pReadCallback;
		std::thread			*thd;
		DCB				mDCB;		// 串口设备控制块
		HANDLE				mHCom;		// 串口句柄
		OVERLAPPED			mReadOL;	// 异步I/O通信方式下的重叠读操作对象
		OVERLAPPED			mWriteOL;	// 异步I/O通信方式下的重叠写操作对象
		OVERLAPPED			mWaitOL;	// 用于等待数据
		COMMTIMEOUTS			mCommTimeout;	// 超时对象
		CRITICAL_SECTION		mLock;		// 同步I/O通信方式下的临界区保护锁
		FlowControl			mFC;		// 流控方式
	};
	
}


#endif // SERIALPORT_H
