#include "A5Header.h"

using namespace llvm;
using namespace std;

void Andersen::runPointerAnalysis()
{
    // 点到集和工作列表在 A5Header.h 中定义。
    //  约束图的实现由 SVF 库提供。
    WorkList<unsigned> workList;

    auto scheduleCopyEdge = [&](unsigned src, unsigned dst) {
        SVF::ConstraintNode *dstNode = consg->getConstraintNode(dst);
        bool exists = false;

        if (dstNode != nullptr) {
            for (auto *e : dstNode->getCopyInEdges()) {
                if (e->getSrcID() == src) {
                    exists = true;
                    break;
                }
            }
        }

        if (!exists) {
            consg->addCopyCGEdge(src, dst);
            workList.push(src);
        }
    };

    // 初始化：把 addr 边上的对象加入点到集并入队
    for (auto it = consg->begin(); it != consg->end(); ++it) {
        const unsigned nid = it->first;
        SVF::ConstraintNode *node = it->second;

        for (auto *e : node->getAddrInEdges()) {
            auto *addrEdge = SVF::SVFUtil::dyn_cast<SVF::AddrCGEdge>(e);
            const unsigned srcId = addrEdge->getSrcID();
            auto &pointSet = pts[nid];

            if (pointSet.insert(srcId).second) {
                workList.push(nid);
            }
        }
    }

    while (!workList.empty()) {
        const unsigned curId = workList.pop();
        SVF::ConstraintNode *curNode = consg->getConstraintNode(curId);
        auto &curPts = pts[curId];

        // 对当前点到集中的每个对象，调度 store/load 引起的 copy
        for (auto obj : curPts) {
            for (auto *se : curNode->getStoreInEdges()) {
                auto *storeEdge = SVF::SVFUtil::dyn_cast<SVF::StoreCGEdge>(se);
                scheduleCopyEdge(storeEdge->getSrcID(), obj);
            }

            for (auto *le : curNode->getLoadOutEdges()) {
                auto *loadEdge = SVF::SVFUtil::dyn_cast<SVF::LoadCGEdge>(le);
                scheduleCopyEdge(obj, loadEdge->getDstID());
            }
        }

        // 处理 copy 边：把 curPts 并入目标点到集
        for (auto *ce : curNode->getCopyOutEdges()) {
            auto *copyEdge = SVF::SVFUtil::dyn_cast<SVF::CopyCGEdge>(ce);
            const unsigned dstId = copyEdge->getDstID();
            auto &dstPts = pts[dstId];
            bool changed = false;

            for (auto obj : curPts) {
                changed |= dstPts.insert(obj).second;
            }

            if (changed) {
                workList.push(dstId);
            }
        }

        // 处理 gep 边：把带字段偏移的对象并入目标点到集
        for (auto *ge : curNode->getGepOutEdges()) {
            auto *gepEdge = SVF::SVFUtil::dyn_cast<SVF::GepCGEdge>(ge);
            const unsigned dstId = gepEdge->getDstID();
            auto &dstPts = pts[dstId];
            bool changed = false;

            for (auto obj : curPts) {
                const unsigned fieldObj = consg->getGepObjVar(obj, gepEdge);
                changed |= dstPts.insert(fieldObj).second;
            }

            if (changed) {
                workList.push(dstId);
            }
        }
    }
}


int main(int argc, char **argv)
{
    auto moduleNameVec = OptionBase::parseOptions(
            argc, argv, "Whole Program Points-to Analysis",
            "[options] <input-bitcode...>");

    SVF::LLVMModuleSet::buildSVFModule(moduleNameVec);

    SVF::SVFIRBuilder builder;
    auto pag = builder.build();
    auto consg = new SVF::ConstraintGraph(pag);
    consg->dump();

    Andersen andersen(consg);

    andersen.runPointerAnalysis();
    andersen.dumpResult();

    SVF::LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}