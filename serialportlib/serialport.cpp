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
		// 初始化读取缓冲区
		ZeroMemory(buf, sizeof(buf));
		waitEvent = 0;
		// 等待事件
		status = WaitCommEvent(mHCom, &waitEvent, &mWaitOL);
		if (FALSE == status && GetLastError() == ERROR_IO_PENDING){
			// 如果串口无数据, 则线程会在此阻塞直到有新数据到来; 如果串口关闭, 则会立即返回false
			status = GetOverlappedResult(mHCom, &mWaitOL, &bytesTransffered, TRUE);
		}

		ClearCommError(mHCom, &error, &cs);
		// 如果事件发生 并且事件是监听时期望的事件 并且输入缓冲区有数据
		if (TRUE == status
			&& waitEvent & EV_RXCHAR
			&& cs.cbInQue > 0){
				// 读取数据
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
*打开串口
*@param  lpName			串口名称, 如COM1、COM2..
*@param  cp             串口I/O读写方式--分异步和同步, CommunicateProtocol::ASYNCIO, CommunicateProtocol::SYNCIO
*@return                布尔值, 返回是否成功打开串口
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
	// 设置串口默认属性
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

	// 超时设置
	COMMTIMEOUTS ct;
	ct.ReadIntervalTimeout = MAXDWORD; 
	ct.ReadTotalTimeoutConstant = 0;
	ct.ReadTotalTimeoutMultiplier = 0;  
	ct.WriteTotalTimeoutMultiplier = 500;
	ct.WriteTotalTimeoutConstant = 5000;
	SetCommTimeouts(mHCom, &ct);

	// 清理
	PurgeComm(mHCom, PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR | PURGE_TXABORT);
	// 初始化重叠结构
	memset(&mReadOL, 0, sizeof(OVERLAPPED));
	memset(&mWriteOL, 0, sizeof(OVERLAPPED));
	memset(&mWaitOL, 0, sizeof(OVERLAPPED));
	// 创建初始无信号及自动reset信号的event
	mReadOL.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	mWriteOL.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	mWaitOL.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	// 设置事件监听
	SetCommMask(mHCom, EV_ERR | EV_RXCHAR);
	// 开启线程
	if (pReadCallback){
		thd = new std::thread(&SerialPort::asread, this);
	}
    
	return true;
}

/**
*
*读取串口数据
*@param  buf	   存放读取数据的buffer
*@param  len	   期望读取的字节数
*@return	       返回实际读取的字节数;0 表示无数据, 处于等待状态;-1表示出错; >0 = 实际读取的字节数
*
**/
int32_t SerialPort::read(char *buf, uint32_t len)
{
	if (mHCom == INVALID_HANDLE_VALUE
		|| buf == NULL){
		return -1;
	}

	// 临界区加锁
	lock();
	// 获取串口输入缓冲区的字节数
	BOOL rf = TRUE;
	DWORD bytesReaded = -1;
	rf = ReadFile(mHCom, buf, len, &bytesReaded, &mReadOL);
	PurgeComm(mHCom, PURGE_RXCLEAR | PURGE_RXABORT);
	// 解锁
	unlock();

	return bytesReaded;
}

/**
*
*向串口写入数据
*@param  buf2write 待写入数据buffer
*@param  len	   要写入的字节数
*@return
*
**/
int32_t SerialPort::write(const char* buf2write, uint32_t len)
{
	if (mHCom == INVALID_HANDLE_VALUE)
		return -1;

	// 清理发送缓冲区
	PurgeComm(mHCom, PURGE_TXCLEAR | PURGE_TXABORT);
	mWaitOL.Offset = 0;
	BOOL ret = TRUE;
	DWORD bytesSent = 0;

	// 写入数据
	ret = WriteFile(mHCom,
			buf2write,
			len,
			&bytesSent,
			&mWriteOL);

	// 等待写入完成
	if (FALSE == ret && GetLastError() == ERROR_IO_PENDING)
	{
		if (FALSE == ::GetOverlappedResult(mHCom, &mWriteOL, &bytesSent, TRUE))
		{
			return -1;
		}
	}

	// 返回实际发送的字节数
	return bytesSent;
}

/**
*
*获取输入缓冲区字节数
*@param
*@return                 返回输入缓冲区字节数
*
**/
uint32_t SerialPort::getBytesInCom() const
{
	/** 错误码 */
	DWORD dwError = 0;
	/** COMSTAT结构体,记录通信设备的状态信息 */
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
*获取当前串口的波特率
*@param
*@return                  返回当前串口波特率
*
**/
int32_t SerialPort::getBaudRate() const
{
	return (mHCom == INVALID_HANDLE_VALUE ? -1 : mDCB.BaudRate);
}

/**
*
*获取当前串口数据总位数
*@param
*@return int32      返回当前串口字节位数
*
**/
int32_t SerialPort::getBitsNum() const
{
	return (mHCom == INVALID_HANDLE_VALUE ? -1 : mDCB.ByteSize);
}

/**
*
*设置波特率
*@param  baud_rate 波特率
*@return bool             布尔值, 返回是否成功设置
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
*设置字节位数
*@param  bits_size 位数
*@return                   布尔值, 返回是否成功设置
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
*设置停止位
*@param  sb		   停止位 StopBits::ONEBIT  StopBits::ONE5BITS  StopBits::TWOBITS
*@return                  布尔值, 返回是否成功设置
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
*设置是否使能串口奇偶校验
*@param  enable    奇偶校验|使能
*@return                   布尔值, 返回是否成功设置
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
*设置二进制传输模式(不检查EOF), 默认字符模式
*@param  enable     二进制模式开启/关闭
*@return                    布尔值, 返回是否成功设置
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
*数据流控制方式设置, 默认为none
*@param  fc    取值FlowControl::NONE FlowControl::XON_XOFF FlowControl::RTS_CTS
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
*校验方式设置
*@param  parity    取值Parity::NONE Parity::ODD Parity::EVEN Parity::MARK Parity::SPACE
*@return                 布尔值, 返回是否成功设置
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
*设置I/O缓冲区大小
*@param  insize           输入缓冲区字节数
*@param  outsize        输出缓冲区字节数
*@return                        布尔值, 返回是否成功设置
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
*进入临界区
*
*/
inline void SerialPort::lock()
{
	EnterCriticalSection(&mLock);
}

/**
*
*离开临界区
*
*/
inline void SerialPort::unlock()
{
	LeaveCriticalSection(&mLock);
}

/**
*
*关闭串口
*
**/
void SerialPort::close()
{

		if (INVALID_HANDLE_VALUE != mHCom)
		{
			stopped = true;
			// 禁止响应串口所有事件
			SetCommMask(mHCom, 0);
			// 清除数据终端就绪信号
			EscapeCommFunction(mHCom, CLRDTR);
            // 清理数据缓冲区|终止通信资源读写挂起操作
            PurgeComm( mHCom, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;
            // 关闭串口句柄
			CloseHandle(this->mHCom);
			this->mHCom = INVALID_HANDLE_VALUE;
		}
}

/**
*
*获取错误码
*@return  Windows错误详情
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
*获取com口打开状态
*@return  布尔值, 表示com口已打开或已关闭
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
*string 转 lpcwstr
**/
char* SerialPort::wcharStrToCharStr(wchar_t* in){
	int num = WideCharToMultiByte(0, 0, in, -1, NULL, 0, NULL, NULL);
	auto* chs = new char[num];
	WideCharToMultiByte(0, 0, in, -1, chs, num, NULL, NULL);

	return chs;
}
