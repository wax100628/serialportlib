#include "serialport.h"
#include <atomic>

using namespace LuHang;

static std::atomic<bool> stopped = false;
static const uint32_t MaxBufSize = 4096;

SerialPort::SerialPort(fReadCallback pFun) :
	mHCom(INVALID_HANDLE_VALUE),
    mFC(FlowControl::NONE),
	pReadCallback(pFun),
	thd(nullptr)
{
	InitializeCriticalSection(&mLock);
}

SerialPort::~SerialPort()
{
	stopped = true;
	if (thd){
		lock();
		if (thd->joinable())
			thd->join();
		delete thd;
		unlock();
	}

    this->close();
    DeleteCriticalSection(&mLock);
}

inline void SerialPort::asread(){

	DWORD waitEvent = 0;
	BOOL status = FALSE;
	DWORD error;
	DWORD bytesTransffered = 0;
	COMSTAT cs = { 0 };
	char* buf = new char[MaxBufSize];
	mWaitOL.Offset = 0;
	mReadOL.Offset = 0;

	while (!stopped){
		// ��ʼ����ȡ������
		ZeroMemory(buf, sizeof(buf));
		waitEvent = 0;
		// �ȴ��¼�
		status = WaitCommEvent(mHCom, &waitEvent, &mWaitOL);
		if (FALSE == status && GetLastError() == ERROR_IO_PENDING){
			// �������������, ���̻߳��ڴ�����ֱ���������ݵ���; ������ڹر�, �����������false
			status = GetOverlappedResult(mHCom, &mWaitOL, &bytesTransffered, TRUE);
		}

		ClearCommError(mHCom, &error, &cs);
		// ����¼����� �����¼��Ǽ���ʱ�������¼� �������뻺����������
		if (TRUE == status
			&& waitEvent & EV_RXCHAR
			&& cs.cbInQue > 0){
				// ��ȡ����
			auto ret = read(buf, MaxBufSize);
				if (ret > 0){
					pReadCallback(buf, ret);
				}
		}
	}

	delete buf;
}

/**
*
*�򿪴���
*@param  lpName			��������, ��COM1��COM2..
*@param  cp             ����I/O��д��ʽ--���첽��ͬ��, CommunicateProtocol::ASYNCIO, CommunicateProtocol::SYNCIO
*@return                ����ֵ, �����Ƿ�ɹ��򿪴���
*
**/
bool SerialPort::open(const char *lpName)
{
	if (lpName == nullptr || strnlen_s(lpName, 16) == 0)
		return false;

	char strCom[32] = { 0 };
	sprintf_s(strCom, sizeof(strCom), "\\\\.\\%s", lpName);
	size_t size = strlen(strCom);

	// convert to wchar
	wchar_t *buffer = new wchar_t[size + 1];
	MultiByteToWideChar(CP_ACP, 0, strCom, size, buffer, size * sizeof(wchar_t));
	buffer[size] = 0;

	mHCom = CreateFile(buffer, \
		GENERIC_READ | GENERIC_WRITE, \
		0, \
		NULL, \
		OPEN_EXISTING, \
		FILE_FLAG_OVERLAPPED|FILE_ATTRIBUTE_NORMAL, \
		NULL);

	delete buffer;
	if (mHCom == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	stopped = false;
	// ���ô���Ĭ������
	GetCommState(mHCom, &mDCB);
    setFlowControl(mFC);
	setBaudRate();
	setBitsNum();
	setParity();
	setParityEnable();
	setBinaryMode();
	setStopBits();
	setCommBufSize(4096, 4096);
	setFlowControl();

	// ��ʱ����
	COMMTIMEOUTS ct;
	ct.ReadIntervalTimeout = MAXDWORD; 
	ct.ReadTotalTimeoutConstant = 0;
	ct.ReadTotalTimeoutMultiplier = 0;  
	ct.WriteTotalTimeoutMultiplier = 500;
	ct.WriteTotalTimeoutConstant = 5000;
	SetCommTimeouts(mHCom, &ct);

	// ����
	PurgeComm(mHCom, PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR | PURGE_TXABORT);
	// ��ʼ���ص��ṹ
	memset(&mReadOL, 0, sizeof(OVERLAPPED));
	memset(&mWriteOL, 0, sizeof(OVERLAPPED));
	memset(&mWaitOL, 0, sizeof(OVERLAPPED));
	// ������ʼ���źż��Զ�reset�źŵ�event
	mReadOL.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	mWriteOL.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	mWaitOL.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	// �����¼�����
	SetCommMask(mHCom, EV_ERR | EV_RXCHAR);
	// �����߳�
	if (pReadCallback){
		thd = new std::thread(&SerialPort::asread, this);
	}
    
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
int32_t SerialPort::read(char *buf, uint32_t len)
{
	if (mHCom == INVALID_HANDLE_VALUE
		|| buf == NULL){
		return -1;
	}

	// �ٽ�������
	lock();
	// ��ȡ�������뻺�������ֽ���
	BOOL rf = TRUE;
	DWORD bytesReaded = -1;
	rf = ReadFile(mHCom, buf, len, &bytesReaded, &mReadOL);
	PurgeComm(mHCom, PURGE_RXCLEAR | PURGE_RXABORT);
	// ����
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
int32_t SerialPort::write(const char* buf2write, uint32_t len)
{
	if (mHCom == INVALID_HANDLE_VALUE)
		return -1;

	// �����ͻ�����
	PurgeComm(mHCom, PURGE_TXCLEAR | PURGE_TXABORT);
	mWaitOL.Offset = 0;
	BOOL ret = TRUE;
	DWORD bytesSent = 0;

	// д������
	ret = WriteFile(mHCom,
			buf2write,
			len,
			&bytesSent,
			&mWriteOL);

	// �ȴ�д�����
	if (FALSE == ret && GetLastError() == ERROR_IO_PENDING)
	{
		if (FALSE == ::GetOverlappedResult(mHCom, &mWriteOL, &bytesSent, TRUE))
		{
			return -1;
		}
	}

	// ����ʵ�ʷ��͵��ֽ���
	return bytesSent;
}

/**
*
*��ȡ���뻺�����ֽ���
*@param
*@return                 �������뻺�����ֽ���
*
**/
uint32_t SerialPort::getBytesInCom() const
{
	/** ������ */
	DWORD dwError = 0;
	/** COMSTAT�ṹ��,��¼ͨ���豸��״̬��Ϣ */
	COMSTAT  comstat;
	memset(&comstat, 0, sizeof(COMSTAT));

	UINT dwInQueBefore = 0;
	UINT dwSize = 0;
	if (ClearCommError(mHCom, &dwError, &comstat))
		dwInQueBefore = comstat.cbInQue;

	if (!ClearCommError(mHCom, &dwError, &comstat))
		return 0;

	while ((dwSize = comstat.cbInQue) != dwInQueBefore)
	{
		dwInQueBefore = dwSize;
		if (!ClearCommError(mHCom, &dwError, &comstat))
			break;

		Sleep(10);
	}

	return dwSize;
}

/**
*
*��ȡ��ǰ���ڵĲ�����
*@param
*@return                  ���ص�ǰ���ڲ�����
*
**/
int32_t SerialPort::getBaudRate() const
{
	return (mHCom == INVALID_HANDLE_VALUE ? -1 : mDCB.BaudRate);
}

/**
*
*��ȡ��ǰ����������λ��
*@param
*@return int32      ���ص�ǰ�����ֽ�λ��
*
**/
int32_t SerialPort::getBitsNum() const
{
	return (mHCom == INVALID_HANDLE_VALUE ? -1 : mDCB.ByteSize);
}

/**
*
*���ò�����
*@param  baud_rate ������
*@return bool             ����ֵ, �����Ƿ�ɹ�����
*
**/
bool SerialPort::setBaudRate(uint32_t baud_rate)
{
	if (INVALID_HANDLE_VALUE == mHCom)
		return false;

	mDCB.BaudRate = baud_rate;
	if (SetCommState(mHCom, &mDCB))
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
bool SerialPort::setBitsNum(uint32_t bits_size)
{
	if (INVALID_HANDLE_VALUE == mHCom)
		return false;

	mDCB.ByteSize = bits_size;
	return SetCommState(mHCom, &mDCB);
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
	if (INVALID_HANDLE_VALUE == mHCom)
		return false;

	mDCB.StopBits = sb;
	return SetCommState(mHCom, &mDCB);
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
	if (INVALID_HANDLE_VALUE == mHCom)
		return false;

	mDCB.fParity = enable ? 1 : 0;
	return SetCommState(mHCom, &mDCB);
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
	if (INVALID_HANDLE_VALUE == mHCom)
		return false;

	mDCB.fBinary = enable ? 1 : 0;
	if (SetCommState(mHCom, &mDCB))
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
    if (INVALID_HANDLE_VALUE == mHCom)
        return;
    
    mFC = fc;
    switch(fc)
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

    SetCommState(mHCom, &mDCB);
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
	if (INVALID_HANDLE_VALUE == mHCom)
		return false;

	mDCB.Parity = p;
	if (SetCommState(mHCom, &mDCB))
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
bool SerialPort::setCommBufSize(uint32_t insize, uint32_t outsize)
{
    if(mHCom == INVALID_HANDLE_VALUE)
        return false;

	if (SetupComm(mHCom, insize, outsize))
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
	EnterCriticalSection(&mLock);
}

/**
*
*�뿪�ٽ���
*
*/
inline void SerialPort::unlock()
{
	LeaveCriticalSection(&mLock);
}

/**
*
*�رմ���
*
**/
void SerialPort::close()
{

		if (INVALID_HANDLE_VALUE != mHCom)
		{
			stopped = true;
			// ��ֹ��Ӧ���������¼�
			SetCommMask(mHCom, 0);
			// ��������ն˾����ź�
			EscapeCommFunction(mHCom, CLRDTR);
            // �������ݻ�����|��ֹͨ����Դ��д�������
            PurgeComm( mHCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;
            // �رմ��ھ��
			CloseHandle(this->mHCom);
			this->mHCom = INVALID_HANDLE_VALUE;
		}
}

/**
*
*��ȡ������
*@return  Windows��������
*
**/
const char* SerialPort::getError()
{
	auto errCode =  GetLastError();
	auto* buffer = new TCHAR[1024];
	auto ret = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		errCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buffer,
		sizeof(buffer),
		NULL);

	if (ret == 0)
		return NULL;

	auto* err = wcharStrToCharStr(buffer);
	delete buffer;

	return err;
}

/**
*
*��ȡcom�ڴ�״̬
*@return  ����ֵ, ��ʾcom���Ѵ򿪻��ѹر�
*
**/
bool SerialPort::isOpen() const
{
	return (mHCom != INVALID_HANDLE_VALUE);
}

void SerialPort::setReadCallback(fReadCallback pFun)
{
	pReadCallback = pFun;
	if (thd == nullptr){
		thd = new std::thread(&SerialPort::asread, this);
	}
}

/**
*string ת lpcwstr
**/
char* SerialPort::wcharStrToCharStr(wchar_t* in){
	int num = WideCharToMultiByte(0, 0, in, -1, NULL, 0, NULL, NULL);
	auto* chs = new char[num];
	WideCharToMultiByte(0, 0, in, -1, chs, num, NULL, NULL);

	return chs;
}
