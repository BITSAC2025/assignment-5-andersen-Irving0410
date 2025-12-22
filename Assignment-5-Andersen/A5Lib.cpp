#include "A5Header.h"

void Andersen::dumpResult()
{
    std::string fname = SVF::PAG::getPAG()->getModuleIdentifier() + ".res.txt";
    std::ofstream outFile(fname, std::ios::out);
    if (!outFile) {
        std::cout << "error opening " + fname + "!!\n";
        return;
    }

    // 输出 S-边
    for (auto &pointerIt : pts)
    {
        outFile << pointerIt.first << " points to: {";
        for (auto pointee : pointerIt.second)
        {
            outFile << pointee << ", ";
        }
        outFile << "}\n";
    }
}