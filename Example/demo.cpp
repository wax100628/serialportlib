#include "serialport.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>

using namespace LuHang;

#pragma comment(lib, "serialport.lib")

/* read callback */
void readCallBack(const char* buffer, int len){
    std::cout << "Recv: " << buffer <<  std::endl;
}

int main(int argc, char** argv)
{
    // check arguments
    if(argc < 2){
        std::cout << "Usage: " << argv[0] << " [COM#]." << std::endl;
        return 0;
    }
    
    // construct SerialPort obj.
    SerialPort sp(&readCallBack);

	// open COM#
    auto ret = sp.open(argv[1]);
    if(!ret)
    {
        std::cout << argv[1] << " open failed\n";
        return 0;
    }

    std::string hcom(argv[1]);
    std::transform(hcom.cbegin()
			, hcom.cend()
			, hcom.begin()
			, [](unsigned char c){ return std::toupper(c);}
			);
    std::cout << "Serialport " << hcom << " Open Successfully!" << std::endl;

    // write data to COM#
    std::string line;
    while(std::getline(std::cin, line)){
        if(line.size() == 0){
            std::cout << "Bye!\n";
            break;
        }
        
        sp.write(line.c_str(), line.length());
    }

    // close COM# -- optional.
    sp.close();
    
    return 0;
}
