#ifndef SYNARE_H
#define SYNARE_H

#include <iostream>

namespace Synare {

    enum VirusResultFlag {
        NotInfected = 1,
        Infected = 2,
    };

    enum VirusExeResult {
        EXE_Failed = NotInfected,
        EXE_Disguised = 0x10 | NotInfected,
        EXEVSNX_NotFound = 0x40 | NotInfected,
        EXERESX_NotFound = 0x80 | Infected,
        EXERESX_Found = 0x100 | Infected,
    };

    VirusExeResult parseWinExecutable(const std::wstring &fileName, std::string *version, std::string *data);

    enum VirusXlsmResult {
        XLSM_Failed = NotInfected,
        XLSM_NoVBAProject = 0x10 | NotInfected,
        XLSM_VirusNotFound = 0x20 | NotInfected,
        XLSM_VirusFound = 0x40 | Infected,
    };

    VirusXlsmResult parseXlsmFile(const std::wstring &fileName, const std::wstring &outFileName,
                                  std::wstring *errorString);

}

#endif // SYNARE_H
