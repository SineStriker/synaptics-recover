#ifndef SYNARE_H
#define SYNARE_H

#include <iostream>

namespace Synare {

    enum VirusResultFlag {
        NotInfected = 0,
        Infected = 1,
    };

    enum VirusExeResult {
        EXE_Failed = 0,
        EXE_Disguised = 0x10,
        EXEVSNX_NotFound = 0x40,
        EXERESX_NotFound = 0x80 | Infected,
        EXERESX_Found = 0x100 | Infected,
    };

    VirusExeResult parseWinExecutable(const std::wstring &fileName, std::string *version, std::string *data);

    enum VirusXlsmResult {
        XLSM_Failed = 0,
    };

    VirusExeResult parseXlsmFile(const std::wstring &fileName, std::string *data);

}

#endif // SYNARE_H
