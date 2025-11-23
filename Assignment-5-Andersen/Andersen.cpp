/**
 * Andersen.cpp
 * @author kisslune
 */

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

    // TODO: complete the following method
    andersen.runPointerAnalysis();

    andersen.dumpResult();
    SVF::LLVMModuleSet::releaseLLVMModuleSet();
	return 0;
}


void Andersen::runPointerAnalysis()
{
    std::cout << "=== Starting Andersen Pointer Analysis ===" << std::endl;
    
    // Initialize worklist with all constraint edges
    WorkList<SVF::ConstraintEdge*> worklist;
    
    // Add all constraint edges to worklist
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
    
    // Helper: push all outgoing edges of a node onto the worklist
    auto pushOutEdgesFromNode = [&](unsigned nodeId) {
        SVF::ConstraintNode* n = consg->getConstraintNode(nodeId);
        if (!n) return;
        for (auto e : n->getOutEdges())
            worklist.push(e);
    };

    // Process worklist until empty
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
        
        // Handle different types of constraints
        switch (edge->getEdgeKind())
        {
            case SVF::ConstraintEdge::Addr:
            {
                // Address-of constraint: a = &b
                // Add b to the points-to set of a
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                std::cout << "  Address-of constraint: " << srcId << " = &" << dstId << std::endl;
                
                if (pts[srcId].insert(dstId).second)
                {
                    std::cout << "  Added " << dstId << " to pts[" << srcId << "]" << std::endl;
                    // Points-to set changed, propagate from this node along all out edges
                    pushOutEdgesFromNode(srcId);
                }
                else
                    std::cout << "  No change - " << dstId << " already in pts[" << srcId << "]" << std::endl;
                break;
            }
            case SVF::ConstraintEdge::Copy:
            {
                // Copy constraint: a = b
                // Copy points-to set from b to a
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
                        // Propagate from updated node
                        pushOutEdgesFromNode(srcId);
                    }
                    else
                        std::cout << "  No change in pts[" << srcId << "]" << std::endl;
                }
                break;
            }
            case SVF::ConstraintEdge::Load:
            {
                // Load constraint: a = *b
                // For each object c in b's points-to set, add c to a's points-to set
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
                            // Find the object that pointee points to
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
                        // Propagate from updated node
                        pushOutEdgesFromNode(srcId);
                    }
                    else
                        std::cout << "  No change in pts[" << srcId << "]" << std::endl;
                }
                break;
            }
            case SVF::ConstraintEdge::Store:
            {
                // Store constraint: *a = b
                // For each object c in a's points-to set, add b's points-to set to c's points-to set
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
                        // Propagate from updated pointee nodes
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
    
    // Output final PTS results for debugging
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