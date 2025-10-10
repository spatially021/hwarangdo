#include "hgm/Driver.h"
#include <iostream>

int main(int argc,char**argv){
    if(argc<3){
        std::cerr << "usage: hgmc <in.hgm> <out.ll>\n";
        return 1;
    }
    Driver d;
    return d.compileFile(argv[1],argv[2]);
}
