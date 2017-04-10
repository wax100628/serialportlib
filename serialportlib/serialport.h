/**
*
*Description:串口通用操作类
*Copyrigth(C),2017,路航科技
*Date:2017.04.06
*@author ZhangJie
*@email  wax628@gmail.com
*@version 1.0
*
**/

#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <Windows.h>
#include <string>

#ifdef SERIALPORT_EXPORTS
#define SERIALPORT_API __declspec(dllexport)
#else
#define SERIALPORT_API __declspec(dllimport)
#endif

namespace LuHang{

	/** I/O传输方式 */
	enum SERIALPORT_API CommunicateProtocol
	{
		ASYNCIO,
		SYNCIO
	};

	/** 停止位 */
	enum SERIALPORT_API StopBits
	{
		// 1位
		ONEBIT,
		// 1.5位
		ONE5BITS,
		// 2位
		TWOBITS
	};

	/** 奇偶校验方法 */
	enum SERIALPORT_API Parity
	{
		NO,
		ODD,
		EVEN,
		MARK,
		SPACE
	};
    
    /** 流控类型 */
    enum SERIALPORT_API FlowControl
    {
        NONE,
        XON_XOFF,
        RTS_CTS,
        DSR_DTR
    };


	/** 串口类 */
	class SERIALPORT_API SerialPort
	{
	public:
		explicit SerialPort();
		~SerialPort();

		/** setters */
		bool setBaudRate(unsigned int baud_rate = 9600);													// 设置波特率
		bool setBitsNum(unsigned char bits_size = 8);														// 设置一次通信传输的总位数
		bool setParity(Parity p = Parity::NO);																// 奇偶校验方法设置
		bool setParityEnable(bool enable = false);															// 奇偶校验使能
        bool setBinaryMode(bool enable = false);															// 设置二进制传输模式(不检查EOF)
		bool setStopBits(StopBits sb = StopBits::ONEBIT);													// 设置停止位
        bool setCommBufSize(unsigned int insize = 32, unsigned int outsize = 32);							// 设置I/O缓冲区大小
        void setFlowControl(FlowControl fc = FlowControl::NONE);											// 流控设置

		/** getters */
		unsigned int  getBytesInCom() const;																// 获取输入缓冲区中的字节数
		int getBaudRate() const;																			// 获取当前串口波特率
		int getBitsNum() const;																				// 获取字节位数设置

		/** open and close */
		bool open(const char *lpName, CommunicateProtocol cp = CommunicateProtocol::ASYNCIO);				// 打开串口
		void close();																						// 关闭串口

		/** read and write */
		int read(unsigned char *buf, unsigned int len);														// 读取串口数据
		int write(unsigned char *buf2write, unsigned int len);												// 向串口写入数据

		/** err detail */
		int getError() const;
		/** is serialport opened? */
		bool isOpen() const;

	private:
		wchar_t* stringTolpcwstr(std::string &str) const;
		inline void lock();
		inline void unlock();

	private:
		//std::string			lpComName;																	// 串口号(名称)
		DCB				    mDCB;																			// 串口设备控制块
		HANDLE			    hFile;																			// 串口句柄
		OVERLAPPED			rdOL;																			// 异步I/O通信方式下的重叠读操作对象
		OVERLAPPED			wrOL;																			// 异步I/O通信方式下的重叠写操作对象
        COMMTIMEOUTS		mCommTimeout;																	// 超时对象
		CRITICAL_SECTION	mLock;																			// 同步I/O通信方式下的临界区保护锁
		CommunicateProtocol	mCP;																			// I/O通信方式
        FlowControl         mFC;																			// 流控方式
	};
}

#endif // SERIALPORT_H