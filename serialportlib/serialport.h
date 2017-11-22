
/********************************************************
*
* Description:����ͨ�ò�����
* 
* Copyrigth(C),2017,�ɶ�����·���Ƽ�
* 
* @author ZhangJie
* @email  wax628@gmail.com
* 
* @version 1.0 2017-04-06 (init)
* @version 1.1 2017-04-25 (function read() improved)
* @version 1.2 2017-11-22 (add read callback function)
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

typedef void (*fReadCallback)(const char*, int32_t);

namespace LuHang{

	/** ֹͣλ */
	enum SERIALPORT_API StopBits {ONEBIT, ONE5BITS, TWOBITS};

	/** ��żУ�鷽�� */
	enum SERIALPORT_API Parity {NO, ODD, EVEN, MARK, SPACE};

	/** �������� */
	enum SERIALPORT_API FlowControl{NONE, XON_XOFF, RTS_CTS, DSR_DTR};


	/** ������ */
	class SERIALPORT_API SerialPort
	{
	public:
		explicit SerialPort(fReadCallback pFun = nullptr);
		~SerialPort();

		/** setters(baudrate/bytesie/parity/stop bits/ ... and so on.) */
		bool setBaudRate(uint32_t baud_rate = 9600);							
		bool setBitsNum(uint8_t bits_size = 8);								
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
		fReadCallback getReadCallback() const;
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
		uint32_t  getBytesInCom() const;

	private:
		fReadCallback			pReadCallback;
		std::thread				*thd;
		DCB						mDCB;			// �����豸���ƿ�
		HANDLE					mHCom;			// ���ھ��
		OVERLAPPED				rdOL;			// �첽I/Oͨ�ŷ�ʽ�µ��ص�����������
		OVERLAPPED				wrOL;			// �첽I/Oͨ�ŷ�ʽ�µ��ص�д��������
		COMMTIMEOUTS			mCommTimeout;	// ��ʱ����
		CRITICAL_SECTION		mLock;			// ͬ��I/Oͨ�ŷ�ʽ�µ��ٽ���������
		FlowControl				mFC;			// ���ط�ʽ
	};
	
}

void asread(LuHang::SerialPort*);

#endif // SERIALPORT_H