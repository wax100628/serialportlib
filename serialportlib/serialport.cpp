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
*打开串口
*@param  lpName			串口名称, 如COM1、COM2..
*@param  cp             串口I/O读写方式--分异步和同步, CommunicateProtocol::ASYNCIO, CommunicateProtocol::SYNCIO
*@return                布尔值, 返回是否成功打开串口
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

	// 设置I/O数据读写方式
	mCP = cp;
	DWORD io_method = (mCP == CommunicateProtocol::ASYNCIO ? FILE_FLAG_OVERLAPPED : 0);

	/*
	*buffer:                              串口号
	*GENERIC_READ | GENERIC_WRITE：       允许读写
	*0：                                  独占方式打开
	*NULL：                               默认安全属性
	*OPEN_EXISTING:                       Opens the file. The function fails if the file does not exist.
	*io_method                            I/O数据传输方式, 异步或同步, 默认异步
	*NULL：                               禁用文件属性模板, 参见Win32API
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

	// 设置串口默认属性
	GetCommState(hFile, &mDCB);
    setFlowControl(mFC);
    // 超时设置
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
*读取串口数据
*@param  buf	   存放读取数据的buffer
*@param  len	   期望读取的字节数
*@return	       返回实际读取的字节数;0 表示无数据, 处于等待状态;-1表示出错; >0 = 实际读取的字节数
*
**/
int SerialPort::read(unsigned char *buf, unsigned int len)
{
	if (hFile == INVALID_HANDLE_VALUE || \
		buf == NULL)
	{
		return -1;
	}

    // 临界区加锁
	lock();
	DWORD bytesReaded = 0;
    COMSTAT rdStat;
    DWORD dwErrFlags;
	BOOL rf = TRUE;

	if (mCP == CommunicateProtocol::ASYNCIO)
	{
		memset(&rdOL, 0, sizeof(OVERLAPPED));
		// 创建初始event--没置位，无信号模式
		rdOL.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	}

	// 清除错误标志
	ClearCommError(hFile, &dwErrFlags, &rdStat);
	// bytesReaded = min(bytesReaded, rdStat.cbInQue);
	// 获取串口输入缓冲区的字节数

	if (mCP == CommunicateProtocol::ASYNCIO)
	{  
		// 异步方式读取数据
		rf = ReadFile(hFile, buf, len, &bytesReaded, &rdOL);
	}		
	else
		// 同步方式读取
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
*向串口写入数据
*@param  buf2write 待写入数据buffer
*@param  len	   要写入的字节数
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
		// 清除错误标志
		ClearCommError(hFile, &dwErrFlags, &wrStat);
		// 异步写入数据 
		ret = WriteFile(hFile,
			buf2write,
			len,
			&bytesSent,
			&wrOL);
	}
	else
		// 同步写
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
*获取输入缓冲区字节数
*@param
*@return                 返回输入缓冲区字节数
*
**/
unsigned int SerialPort::getBytesInCom() const
{
	DWORD dwError = 0;  /** 错误码 */
	COMSTAT  comstat;   /** COMSTAT结构体,记录通信设备的状态信息 */
	memset(&comstat, 0, sizeof(COMSTAT));

	UINT BytesInQue = 0;
	/** 在调用ReadFile和WriteFile之前,通过本函数清除以前遗留的错误标志 */
	if (ClearCommError(hFile, &dwError, &comstat))
	{
		BytesInQue = comstat.cbInQue; /** 获取在输入缓冲区中的字节数 */
	}

	return BytesInQue;
}

/**
*
*获取当前串口的波特率
*@param
*@return                  返回当前串口波特率
*
**/
int SerialPort::getBaudRate() const
{
	return (hFile == INVALID_HANDLE_VALUE ? -1 : mDCB.BaudRate);
}

/**
*
*获取当前串口数据总位数
*@param
*@return int32      返回当前串口字节位数
*
**/
int SerialPort::getBitsNum() const
{
	return (hFile == INVALID_HANDLE_VALUE ? -1 : mDCB.ByteSize);
}

/**
*
*设置波特率
*@param  baud_rate 波特率
*@return bool             布尔值, 返回是否成功设置
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
*设置字节位数
*@param  bits_size 位数
*@return                   布尔值, 返回是否成功设置
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
*设置停止位
*@param  sb		   停止位 StopBits::ONEBIT  StopBits::ONE5BITS  StopBits::TWOBITS
*@return                  布尔值, 返回是否成功设置
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
*设置是否使能串口奇偶校验
*@param  enable    奇偶校验|使能
*@return                   布尔值, 返回是否成功设置
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
*设置二进制传输模式(不检查EOF), 默认字符模式
*@param  enable     二进制模式开启/关闭
*@return                    布尔值, 返回是否成功设置
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
*数据流控制方式设置, 默认为none
*@param  fc    取值FlowControl::NONE FlowControl::XON_XOFF FlowControl::RTS_CTS
*@return
*
**/
void SerialPort::setFlowControl(FlowControl fc)
{
    if (INVALID_HANDLE_VALUE == hFile)
        return;
    
    mFC = fc;
    // 流控设置--暂略
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
*校验方式设置
*@param  parity    取值Parity::NONE Parity::ODD Parity::EVEN Parity::MARK Parity::SPACE
*@return                 布尔值, 返回是否成功设置
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
*设置I/O缓冲区大小
*@param  insize           输入缓冲区字节数
*@param  outsize        输出缓冲区字节数
*@return                        布尔值, 返回是否成功设置
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
*进入临界区
*
*/
inline void SerialPort::lock()
{
	if (mCP == CommunicateProtocol::SYNCIO)
		EnterCriticalSection(&mLock);
}

/**
*
*离开临界区
*
*/
inline void SerialPort::unlock()
{
	if (mCP == CommunicateProtocol::SYNCIO)
		LeaveCriticalSection(&mLock);
}

/**
*
*关闭串口
*
**/
void SerialPort::close()
{
	try
	{
		if (INVALID_HANDLE_VALUE != this->hFile)
		{
            // 禁止响应串口所有事件
            SetCommMask(hFile, 0) ;
            // 清除数据终端就绪信号
            EscapeCommFunction( hFile, CLRDTR ) ;
            // 清理数据缓冲区|终止通信资源读写挂起操作
            PurgeComm( hFile, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR ) ;
            // 关闭串口句柄
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
*获取错误码
*@return  Windows错误码
*
**/
int SerialPort::getError() const
{
	return GetLastError();
}

/**
*
*获取com口打开状态
*@return  布尔值, 表示com口已打开或已关闭
*
**/
bool SerialPort::isOpen() const
{
	return (hFile != INVALID_HANDLE_VALUE);
}

/**
*string 转 lpcwstr
**/
wchar_t* SerialPort::stringTolpcwstr(std::string &str) const
{
	size_t size = str.length();
	wchar_t *buffer = new wchar_t[size + 1];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), size, buffer, size * sizeof(wchar_t));
	buffer[size] = 0;

	return buffer;
}
