/**
*
*Description:����ͨ�ò�����
*Copyrigth(C),2017,·���Ƽ�
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

	/** I/O���䷽ʽ */
	enum SERIALPORT_API CommunicateProtocol
	{
		ASYNCIO,
		SYNCIO
	};

	/** ֹͣλ */
	enum SERIALPORT_API StopBits
	{
		// 1λ
		ONEBIT,
		// 1.5λ
		ONE5BITS,
		// 2λ
		TWOBITS
	};

	/** ��żУ�鷽�� */
	enum SERIALPORT_API Parity
	{
		NO,
		ODD,
		EVEN,
		MARK,
		SPACE
	};
    
    /** �������� */
    enum SERIALPORT_API FlowControl
    {
        NONE,
        XON_XOFF,
        RTS_CTS,
        DSR_DTR
    };


	/** ������ */
	class SERIALPORT_API SerialPort
	{
	public:
		explicit SerialPort();
		~SerialPort();

		/** setters */
		bool setBaudRate(unsigned int baud_rate = 9600);													// ���ò�����
		bool setBitsNum(unsigned char bits_size = 8);														// ����һ��ͨ�Ŵ������λ��
		bool setParity(Parity p = Parity::NO);																// ��żУ�鷽������
		bool setParityEnable(bool enable = false);															// ��żУ��ʹ��
        bool setBinaryMode(bool enable = false);															// ���ö����ƴ���ģʽ(�����EOF)
		bool setStopBits(StopBits sb = StopBits::ONEBIT);													// ����ֹͣλ
        bool setCommBufSize(unsigned int insize = 32, unsigned int outsize = 32);							// ����I/O��������С
        void setFlowControl(FlowControl fc = FlowControl::NONE);											// ��������

		/** getters */
		unsigned int  getBytesInCom() const;																// ��ȡ���뻺�����е��ֽ���
		int getBaudRate() const;																			// ��ȡ��ǰ���ڲ�����
		int getBitsNum() const;																				// ��ȡ�ֽ�λ������

		/** open and close */
		bool open(const char *lpName, CommunicateProtocol cp = CommunicateProtocol::ASYNCIO);				// �򿪴���
		void close();																						// �رմ���

		/** read and write */
		int read(unsigned char *buf, unsigned int len);														// ��ȡ��������
		int write(unsigned char *buf2write, unsigned int len);												// �򴮿�д������

		/** err detail */
		int getError() const;
		/** is serialport opened? */
		bool isOpen() const;

	private:
		wchar_t* stringTolpcwstr(std::string &str) const;
		inline void lock();
		inline void unlock();

	private:
		//std::string			lpComName;																	// ���ں�(����)
		DCB				    mDCB;																			// �����豸���ƿ�
		HANDLE			    hFile;																			// ���ھ��
		OVERLAPPED			rdOL;																			// �첽I/Oͨ�ŷ�ʽ�µ��ص�����������
		OVERLAPPED			wrOL;																			// �첽I/Oͨ�ŷ�ʽ�µ��ص�д��������
        COMMTIMEOUTS		mCommTimeout;																	// ��ʱ����
		CRITICAL_SECTION	mLock;																			// ͬ��I/Oͨ�ŷ�ʽ�µ��ٽ���������
		CommunicateProtocol	mCP;																			// I/Oͨ�ŷ�ʽ
        FlowControl         mFC;																			// ���ط�ʽ
	};
}

#endif // SERIALPORT_H