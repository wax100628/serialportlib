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

#include "serialport.h"
using namespace LuHang;

SerialPort::SerialPort() :
	hFile(INVALID_HANDLE_VALUE),
    mFC(FlowControl::NONE)
{
	InitializeCriticalSection(&mLock);
}

SerialPort::~SerialPort()
{
    this->close();
    DeleteCriticalSection(&mLock);
}

/**
*
*�򿪴���
*@param  lpName			��������, ��COM1��COM2..
*@param  cp             ����I/O��д��ʽ--���첽��ͬ��, CommunicateProtocol::ASYNCIO, CommunicateProtocol::SYNCIO
*@return                ����ֵ, �����Ƿ�ɹ��򿪴���
*
**/
bool SerialPort::open(const char *lpName, CommunicateProtocol cp)
{
	if (lpName == nullptr || strnlen_s(lpName, 16) == 0)
		return false;

	size_t size = strlen(lpName);

	// convert to wchar
	wchar_t *buffer = new wchar_t[size + 1];
	MultiByteToWideChar(CP_ACP, 0, lpName, size, buffer, size * sizeof(wchar_t));
	buffer[size] = 0;

	// ����I/O���ݶ�д��ʽ
	mCP = cp;
	DWORD io_method = (mCP == CommunicateProtocol::ASYNCIO ? FILE_FLAG_OVERLAPPED : 0);

	/*
	*buffer:                              ���ں�
	*GENERIC_READ | GENERIC_WRITE��       �����д
	*0��                                  ��ռ��ʽ��
	*NULL��                               Ĭ�ϰ�ȫ����
	*OPEN_EXISTING:                       Opens the file. The function fails if the file does not exist.
	*io_method                            I/O���ݴ��䷽ʽ, �첽��ͬ��, Ĭ���첽
	*NULL��                               �����ļ�����ģ��, �μ�Win32API
	*/
	hFile = CreateFile(buffer, \
		GENERIC_READ | GENERIC_WRITE, \
		0, \
		NULL, \
		OPEN_EXISTING, \
		io_method, \
		NULL);

	delete buffer;
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// ���ô���Ĭ������
	GetCommState(hFile, &mDCB);
    setFlowControl(mFC);
    // ��ʱ����
    GetCommTimeouts(hFile, &mCommTimeout);
    mCommTimeout.ReadIntervalTimeout = 500;
    mCommTimeout.ReadTotalTimeoutMultiplier = 500;
    mCommTimeout.ReadTotalTimeoutConstant = 250;   
    mCommTimeout.WriteTotalTimeoutMultiplier = 500;
    mCommTimeout.WriteTotalTimeoutConstant = 2000;
    SetCommTimeouts(hFile, &mCommTimeout);
    
	return true;
}

/**
*
*��ȡ��������
*@param  buf	   ��Ŷ�ȡ���ݵ�buffer
*@param  len	   ������ȡ���ֽ���
*@return	       ����ʵ�ʶ�ȡ���ֽ���;0 ��ʾ������, ���ڵȴ�״̬;-1��ʾ����; >0 = ʵ�ʶ�ȡ���ֽ���
*
**/
int SerialPort::read(unsigned char *buf, unsigned int len)
{
	if (hFile == INVALID_HANDLE_VALUE || \
		buf == NULL)
	{
		return -1;
	}

    // �ٽ�������
	lock();
	DWORD bytesReaded = 0;
    COMSTAT rdStat;
    DWORD dwErrFlags;
	BOOL rf = TRUE;

	if (mCP == CommunicateProtocol::ASYNCIO)
	{
		memset(&rdOL, 0, sizeof(OVERLAPPED));
		// ������ʼevent--û��λ�����ź�ģʽ
		rdOL.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	// ��������־
	ClearCommError(hFile, &dwErrFlags, &rdStat);
	// bytesReaded = min(bytesReaded, rdStat.cbInQue);
	// ��ȡ�������뻺�������ֽ���

	if (mCP == CommunicateProtocol::ASYNCIO)
	{  
		// �첽��ʽ��ȡ����
		rf = ReadFile(hFile, buf, len, &bytesReaded, &rdOL);
	}		
	else
		// ͬ����ʽ��ȡ
		rf = ReadFile(hFile, buf, len, &bytesReaded, NULL);

	if (!rf)
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			
#if 1
			if (mCP == CommunicateProtocol::ASYNCIO)
			{
				WaitForSingleObject(rdOL.hEvent, 5000);
				GetOverlappedResult(hFile, &rdOL, &bytesReaded, TRUE);
				return bytesReaded;
			}
			else
			{
				unlock();
				return 0;
			}

#else
			BOOL ret = true;
			if (mCP == CommunicateProtocol::ASYNCIO)
				ret = GetOverlappedResult(hFile, &rdOL, &bytesReaded, TRUE);
#endif
		}
		else
		{
			PurgeComm(hFile, PURGE_RXCLEAR|PURGE_RXABORT);
			unlock();
			return -1;
		}
	}

	unlock();
	return bytesReaded;
}

/**
*
*�򴮿�д������
*@param  buf2write ��д������buffer
*@param  len	   Ҫд����ֽ���
*@return
*
**/
int SerialPort::write(unsigned char* buf2write, unsigned int len)
{
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	BOOL ret = TRUE;
	DWORD bytesSent = 0;
	COMSTAT wrStat;
	DWORD dwErrFlags;

	if (mCP == CommunicateProtocol::ASYNCIO)
	{
		memset(&wrOL, 0, sizeof(OVERLAPPED));
		wrOL.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		// ��������־
		ClearCommError(hFile, &dwErrFlags, &wrStat);
		// �첽д������ 
		ret = WriteFile(hFile,
			buf2write,
			len,
			&bytesSent,
			&wrOL);
	}
	else
		// ͬ��д
		ret = WriteFile(hFile,
		buf2write,
		len,
		&bytesSent,
		NULL);

	DWORD dwErr = GetLastError();
	if (!ret)
	{
		if (ERROR_IO_PENDING == dwErr || ERROR_IO_INCOMPLETE == dwErr)
		{
			if (mCP == CommunicateProtocol::ASYNCIO)
            {
				DWORD wait_ret = WaitForSingleObject(wrOL.hEvent, 1000);
                switch(wait_ret)
                {
                    // func failed
                    case WAIT_FAILED: PurgeComm(hFile, PURGE_TXCLEAR); return -1;
                    // func success status
                    case WAIT_TIMEOUT: 
					case WAIT_ABANDONED:
                    case WAIT_OBJECT_0: return bytesSent;
                    default: return bytesSent;
                }
            }
			else
			{
				return bytesSent;
			}
		}
		else
        {
            PurgeComm(hFile, PURGE_TXCLEAR);
            return -1;
        }
	}

	return bytesSent;
}

/**
*
*��ȡ���뻺�����ֽ���
*@param
*@return                 �������뻺�����ֽ���
*
**/
unsigned int SerialPort::getBytesInCom() const
{
	DWORD dwError = 0;  /** ������ */
	COMSTAT  comstat;   /** COMSTAT�ṹ��,��¼ͨ���豸��״̬��Ϣ */
	memset(&comstat, 0, sizeof(COMSTAT));

	UINT BytesInQue = 0;
	/** �ڵ���ReadFile��WriteFile֮ǰ,ͨ�������������ǰ�����Ĵ����־ */
	if (ClearCommError(hFile, &dwError, &comstat))
	{
		BytesInQue = comstat.cbInQue; /** ��ȡ�����뻺�����е��ֽ��� */
	}

	return BytesInQue;
}

/**
*
*��ȡ��ǰ���ڵĲ�����
*@param
*@return                  ���ص�ǰ���ڲ�����
*
**/
int SerialPort::getBaudRate() const
{
	return (hFile == INVALID_HANDLE_VALUE ? -1 : mDCB.BaudRate);
}

/**
*
*��ȡ��ǰ����������λ��
*@param
*@return int32      ���ص�ǰ�����ֽ�λ��
*
**/
int SerialPort::getBitsNum() const
{
	return (hFile == INVALID_HANDLE_VALUE ? -1 : mDCB.ByteSize);
}

/**
*
*���ò�����
*@param  baud_rate ������
*@return bool             ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setBaudRate(unsigned int baud_rate)
{
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	mDCB.BaudRate = baud_rate;
	if (SetCommState(hFile, &mDCB))
		return true;

	return false;
}

/**
*
*�����ֽ�λ��
*@param  bits_size λ��
*@return                   ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setBitsNum(unsigned char bits_size)
{
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	mDCB.ByteSize = bits_size;
	if (SetCommState(hFile, &mDCB))
		return true;

	return false;
}

/**
*
*����ֹͣλ
*@param  sb		   ֹͣλ StopBits::ONEBIT  StopBits::ONE5BITS  StopBits::TWOBITS
*@return                  ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setStopBits(StopBits sb)
{
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	mDCB.StopBits = sb;
	if (SetCommState(hFile, &mDCB))
		return true;

	return false;
}

/**
*
*�����Ƿ�ʹ�ܴ�����żУ��
*@param  enable    ��żУ��|ʹ��
*@return                   ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setParityEnable(bool enable)
{
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	mDCB.fParity = enable ? 1 : 0;
	if (SetCommState(hFile, &mDCB))
		return true;

	return false;
}

/**
*
*���ö����ƴ���ģʽ(�����EOF), Ĭ���ַ�ģʽ
*@param  enable     ������ģʽ����/�ر�
*@return                    ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setBinaryMode(bool enable)
{
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	mDCB.fBinary = enable ? 1 : 0;
	if (SetCommState(hFile, &mDCB))
		return true;

	return false;
}

/**
*
*���������Ʒ�ʽ����, Ĭ��Ϊnone
*@param  fc    ȡֵFlowControl::NONE FlowControl::XON_XOFF FlowControl::RTS_CTS
*@return
*
**/
void SerialPort::setFlowControl(FlowControl fc)
{
    if (INVALID_HANDLE_VALUE == hFile)
        return;
    
    mFC = fc;
    // ��������--����
    /*
    swtich(fc)
    {
        case FlowControl::NONE:
            mDCB.fInX = 0; mDCB.fOutX = 0;
            mDCB.fRtsControl = RTS_CONTROL_DISABLE; mDCB.fOutxCtsFlow = 0;
            break;
        case FlowControl::XON_XOFF:
            mDCB.fRtsControl = 0; mDCB.fOutxCtsFlow = 0;
            mDCB.fInX = 1; mDCB.fOutX = 1;
            mDCB.XonLim = 32; mDCB.XoffLim = 32;
            break;
        case FlowControl::RTS_CTS:
            mDCB.fInX = 0; mDCB.fOutX = 0;
            mDCB.fRtsControl = RTS_CONTROL_ENABLE; mDCB.fOutxCtsFlow = 1;
            break;
        case FlowControl::DSR_DTR:
            break;
    }
    SetCommState(hFile, &mDCB);
    */
}

/**
*
*У�鷽ʽ����
*@param  parity    ȡֵParity::NONE Parity::ODD Parity::EVEN Parity::MARK Parity::SPACE
*@return                 ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setParity(Parity p)
{
	if (INVALID_HANDLE_VALUE == hFile)
		return false;

	mDCB.Parity = p;
	if (SetCommState(hFile, &mDCB))
		return true;

	return false;
}

/**
*
*����I/O��������С
*@param  insize           ���뻺�����ֽ���
*@param  outsize        ����������ֽ���
*@return                        ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setCommBufSize(unsigned int insize, unsigned int outsize)
{
    if(hFile == INVALID_HANDLE_VALUE)
        return false;

	if (SetupComm(hFile, insize, outsize))
		return true;

	return false;
}

/**
*
*�����ٽ���
*
*/
inline void SerialPort::lock()
{
	if (mCP == CommunicateProtocol::SYNCIO)
		EnterCriticalSection(&mLock);
}

/**
*
*�뿪�ٽ���
*
*/
inline void SerialPort::unlock()
{
	if (mCP == CommunicateProtocol::SYNCIO)
		LeaveCriticalSection(&mLock);
}

/**
*
*�رմ���
*
**/
void SerialPort::close()
{
	try
	{
		if (INVALID_HANDLE_VALUE != this->hFile)
		{
            // ��ֹ��Ӧ���������¼�
            SetCommMask(hFile, 0) ;
            // ��������ն˾����ź�
            EscapeCommFunction( hFile, CLRDTR ) ;
            // �������ݻ�����|��ֹͨ����Դ��д�������
            PurgeComm( hFile, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;
            // �رմ��ھ��
			CloseHandle(this->hFile);
			this->hFile = INVALID_HANDLE_VALUE;
		}

	}
	catch (std::exception e)
	{
		// e.what()
	}
}

/**
*
*��ȡ������
*@return  Windows������
*
**/
int SerialPort::getError() const
{
	return GetLastError();
}

/**
*
*��ȡcom�ڴ�״̬
*@return  ����ֵ, ��ʾcom���Ѵ򿪻��ѹر�
*
**/
bool SerialPort::isOpen() const
{
	return (hFile != INVALID_HANDLE_VALUE);
}

/**
*string ת lpcwstr
**/
wchar_t* SerialPort::stringTolpcwstr(std::string &str) const
{
	size_t size = str.length();
	wchar_t *buffer = new wchar_t[size + 1];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), size, buffer, size * sizeof(wchar_t));
	buffer[size] = 0;

	return buffer;
}
