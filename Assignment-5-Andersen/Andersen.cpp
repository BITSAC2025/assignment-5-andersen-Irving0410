#include "A5Header.h"

using namespace llvm;
using namespace std;

int main(int argc, char** argv)
{
    auto moduleNameVec =
            OptionBase::parseOptions(argc, argv, "Whole Program Points-to Analysis",
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


void Andersen::runPointerAnalysis()
{
    std::cout << "=== Starting Andersen Pointer Analysis ===" << std::endl;
    
    // 使用所有约束边初始化工作队列
    WorkList<SVF::ConstraintEdge*> worklist;
    
    // 将所有约束边加入工作队列
    int edgeCount = 0;
    for (auto it = consg->begin(); it != consg->end(); ++it)
    {
        SVF::ConstraintNode* node = it->second;
        for (auto edge : node->getOutEdges())
        {
            worklist.push(edge);
            edgeCount++;
        }
    }
    std::cout << "Initial worklist size: " << edgeCount << " edges" << std::endl;
    
    // 辅助：将指定节点的所有出边压入工作队列
    auto pushOutEdgesFromNode = [&](unsigned nodeId) {
        SVF::ConstraintNode* n = consg->getConstraintNode(nodeId);
        if (!n) return;
        for (auto e : n->getOutEdges())
            worklist.push(e);
    };

    // 处理工作队列直到为空
    int iteration = 0;
    while (!worklist.empty())
    {
        iteration++;
        SVF::ConstraintEdge* edge = worklist.pop();
        SVF::ConstraintNode* src = edge->getSrcNode();
        SVF::ConstraintNode* dst = edge->getDstNode();
        
        std::cout << "\n[Iteration " << iteration << "] Processing edge: " 
                  << src->getId() << " -> " << dst->getId() 
                  << " (Type: " << edge->getEdgeKind() << ")" << std::endl;
        
        // 处理不同类型的约束
        switch (edge->getEdgeKind())
        {
            case SVF::ConstraintEdge::Addr:
            {
            // 取地址约束：a = &b
            // 将 b 加入 a 的点到集合
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                std::cout << "  Address-of constraint: " << srcId << " = &" << dstId << std::endl;
                
                if (pts[srcId].insert(dstId).second)
                {
                    std::cout << "  Added " << dstId << " to pts[" << srcId << "]" << std::endl;
                    pushOutEdgesFromNode(srcId);
                }
                else
                    std::cout << "  No change - " << dstId << " already in pts[" << srcId << "]" << std::endl;
                break;
            }
            case SVF::ConstraintEdge::Copy:
            {
                // 赋值拷贝约束：a = b
                // 将 b 的点到集合合并到 a
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                std::cout << "  Copy constraint: " << srcId << " = " << dstId << std::endl;
                
                {
                    bool changed = false;
                    int addedCount = 0;
                    if (pts.find(dstId) != pts.end())
                    {
                        std::cout << "  pts[" << dstId << "] = {";
                        for (auto pointee : pts[dstId])
                            std::cout << pointee << " ";
                        std::cout << "}" << std::endl;

                        for (auto pointee : pts[dstId])
                        {
                            if (pts[srcId].insert(pointee).second)
                            {
                                changed = true;
                                addedCount++;
                                std::cout << "  Added " << pointee << " to pts[" << srcId << "]" << std::endl;
                            }
                        }
                    }
                    else
                        std::cout << "  pts[" << dstId << "] is empty" << std::endl;

                    if (changed)
                    {
                        std::cout << "  Changed: added " << addedCount << " elements" << std::endl;
                        // 从已更新节点出发进行传播
                        pushOutEdgesFromNode(srcId);
                    }
                    else
                        std::cout << "  No change in pts[" << srcId << "]" << std::endl;
                }
                break;
            }
            case SVF::ConstraintEdge::Load:
            {
                // Load 约束：a = *b
                // 对 b 的每个指向对象 c，将 c 的指向对象并入 a 的点到集合
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                std::cout << "  Load constraint: " << srcId << " = *" << dstId << std::endl;
                
                {
                    bool changed = false;
                    int addedCount = 0;
                    if (pts.find(dstId) != pts.end())
                    {
                        std::cout << "  pts[" << dstId << "] = {";
                        for (auto pointee : pts[dstId])
                            std::cout << pointee << " ";
                        std::cout << "}" << std::endl;

                        for (auto pointee : pts[dstId])
                        {
                            if (pts.find(pointee) != pts.end())
                            {
                                std::cout << "  pts[" << pointee << "] = {";
                                for (auto indirectPointee : pts[pointee])
                                    std::cout << indirectPointee << " ";
                                std::cout << "}" << std::endl;

                                for (auto indirectPointee : pts[pointee])
                                {
                                    if (pts[srcId].insert(indirectPointee).second)
                                    {
                                        changed = true;
                                        addedCount++;
                                        std::cout << "  Added " << indirectPointee << " to pts[" << srcId << "]" << std::endl;
                                    }
                                }
                            }
                            else
                                std::cout << "  pts[" << pointee << "] is empty" << std::endl;
                        }
                    }
                    else
                        std::cout << "  pts[" << dstId << "] is empty" << std::endl;

                    if (changed)
                    {
                        std::cout << "  Changed: added " << addedCount << " indirect elements" << std::endl;
                        // 从已更新节点出发进行传播
                        pushOutEdgesFromNode(srcId);
                    }
                    else
                        std::cout << "  No change in pts[" << srcId << "]" << std::endl;
                }
                break;
            }
            case SVF::ConstraintEdge::Store:
            {
                // Store 约束：*a = b
                // 对 a 的每个指向对象 c，将 b 的点到集合并入 c 的点到集合
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                std::cout << "  Store constraint: *" << srcId << " = " << dstId << std::endl;
                
                {
                    bool changed = false;
                    int addedCount = 0;
                    if (pts.find(srcId) != pts.end())
                    {
                        std::cout << "  pts[" << srcId << "] = {";
                        for (auto pointee : pts[srcId])
                            std::cout << pointee << " ";
                        std::cout << "}" << std::endl;

                        for (auto pointee : pts[srcId])
                        {
                            if (pts.find(dstId) != pts.end())
                            {
                                std::cout << "  pts[" << dstId << "] = {";
                                for (auto targetPointee : pts[dstId])
                                    std::cout << targetPointee << " ";
                                std::cout << "}" << std::endl;

                                for (auto targetPointee : pts[dstId])
                                {
                                    if (pts[pointee].insert(targetPointee).second)
                                    {
                                        changed = true;
                                        addedCount++;
                                        std::cout << "  Added " << targetPointee << " to pts[" << pointee << "]" << std::endl;
                                    }
                                }
                            }
                            else
                                std::cout << "  pts[" << dstId << "] is empty" << std::endl;
                        }
                    }
                    else
                        std::cout << "  pts[" << srcId << "] is empty" << std::endl;

                    if (changed)
                    {
                        std::cout << "  Changed: added " << addedCount << " elements to indirect targets" << std::endl;
                        // 从被指向对象节点开始传播更新
                        for (auto pointee : pts[srcId])
                            pushOutEdgesFromNode(pointee);
                    }
                    else
                        std::cout << "  No change in indirect targets" << std::endl;
                }
                break;
            }
            default:
                break;
        }
    }
    
    std::cout << "Pointer analysis completed in " << iteration << " iterations" << std::endl;
    
    // 输出最终点到集合结果
    std::cout << "\n=== Final Points-to Sets ===" << std::endl;
    for (const auto& entry : pts)
    {
        if (!entry.second.empty())
        {
            std::cout << "pts[" << entry.first << "] = {";
            for (auto obj : entry.second)
            {
                std::cout << obj << " ";
            }
            std::cout << "}" << std::endl;
        }
    }
    std::cout << "=== End of Results ===" << std::endl;
}