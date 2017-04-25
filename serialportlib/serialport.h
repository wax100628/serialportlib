/**
*
*Description:����ͨ�ò�����
*Copyrigth(C),2017,·���Ƽ�
*@author ZhangJie
*@email  wax628@gmail.com
*@version 1.0 2017-04-06 (init)
*@version 1.1 2017-04-25 (function read() improved)
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

	/** I/O���䷽ʽ */
	enum SERIALPORT_API CommunicateProtocol {ASYNCIO, SYNCIO};

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
		explicit SerialPort();
		~SerialPort();

		/** setters(baudrate/bytesie/parity/stop bits/ ... and so on.) */
		bool setBaudRate(unsigned int baud_rate = 9600);							
		bool setBitsNum(unsigned char bits_size = 8);								
		bool setParity(Parity p = Parity::NO);										
		bool setParityEnable(bool enable = false);									
		bool setBinaryMode(bool enable = false);									
		bool setStopBits(StopBits sb = StopBits::ONEBIT);							
		bool setCommBufSize(unsigned int insize = 32, unsigned int outsize = 32);	
		void setFlowControl(FlowControl fc = FlowControl::NONE);

		/** getters */
		int getBaudRate() const;
		int getBitsNum() const;

		/** open and close */
		bool open(const char *lpName, CommunicateProtocol cp = CommunicateProtocol::ASYNCIO);
		void close();

		/** read and write */
		int read(unsigned char *buf, unsigned int len);
		int write(unsigned char *buf2write, unsigned int len);

		/** err detail */
		int getError() const;
		/** is serialport opened? */
		bool isOpen() const;

	private:
		wchar_t* stringTolpcwstr(std::string &str) const;
		inline void lock();
		inline void unlock();
		unsigned int  getBytesInCom() const;							

	private:
		DCB						mDCB;			// �����豸���ƿ�
		HANDLE					mHCom;			// ���ھ��
		OVERLAPPED				rdOL;			// �첽I/Oͨ�ŷ�ʽ�µ��ص�����������
		OVERLAPPED				wrOL;			// �첽I/Oͨ�ŷ�ʽ�µ��ص�д��������
		COMMTIMEOUTS			mCommTimeout;	// ��ʱ����
		CRITICAL_SECTION		mLock;			// ͬ��I/Oͨ�ŷ�ʽ�µ��ٽ���������
		CommunicateProtocol		mCP;			// I/Oͨ�ŷ�ʽ
		FlowControl				mFC;			// ���ط�ʽ
	};
}

#endif // SERIALPORT_H