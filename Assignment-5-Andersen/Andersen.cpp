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
    // Initialize worklist with all constraint edges
    WorkList<SVF::ConstraintEdge*> worklist;
    
    // Add all constraint edges to worklist
    for (auto it = consg->begin(); it != consg->end(); ++it)
    {
        SVF::ConstraintNode* node = it->second;
        for (auto edge : node->getOutEdges())
        {
            worklist.push(edge);
        }
    }
    
    // Process worklist until empty
    while (!worklist.empty())
    {
        SVF::ConstraintEdge* edge = worklist.pop();
        SVF::ConstraintNode* src = edge->getSrcNode();
        SVF::ConstraintNode* dst = edge->getDstNode();
        
        // Handle different types of constraints
        switch (edge->getEdgeKind())
        {
            case SVF::ConstraintEdge::Addr:
            {
                // Address-of constraint: a = &b
                // Add b to the points-to set of a
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                if (pts[srcId].insert(dstId).second)
                {
                    // Points-to set changed, propagate to copy edges
                    for (auto copyEdge : src->getOutEdges())
                    {
                        if (copyEdge->getEdgeKind() == SVF::ConstraintEdge::Copy)
                        {
                            worklist.push(copyEdge);
                        }
                    }
                }
                break;
            }
            case SVF::ConstraintEdge::Copy:
            {
                // Copy constraint: a = b
                // Copy points-to set from b to a
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                bool changed = false;
                if (pts.find(dstId) != pts.end())
                {
                    for (auto pointee : pts[dstId])
                    {
                        if (pts[srcId].insert(pointee).second)
                        {
                            changed = true;
                        }
                    }
                }
                
                if (changed)
                {
                    // Propagate to load and store edges
                    for (auto outEdge : src->getOutEdges())
                    {
                        if (outEdge->getEdgeKind() == SVF::ConstraintEdge::Load || 
                            outEdge->getEdgeKind() == SVF::ConstraintEdge::Store)
                        {
                            worklist.push(outEdge);
                        }
                    }
                }
                break;
            }
            case SVF::ConstraintEdge::Load:
            {
                // Load constraint: a = *b
                // For each object c in b's points-to set, add c to a's points-to set
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                bool changed = false;
                if (pts.find(dstId) != pts.end())
                {
                    for (auto pointee : pts[dstId])
                    {
                        // Find the object that pointee points to
                        if (pts.find(pointee) != pts.end())
                        {
                            for (auto indirectPointee : pts[pointee])
                            {
                                if (pts[srcId].insert(indirectPointee).second)
                                {
                                    changed = true;
                                }
                            }
                        }
                    }
                }
                
                if (changed)
                {
                    // Propagate to copy edges
                    for (auto copyEdge : src->getOutEdges())
                    {
                        if (copyEdge->getEdgeKind() == SVF::ConstraintEdge::Copy)
                        {
                            worklist.push(copyEdge);
                        }
                    }
                }
                break;
            }
            case SVF::ConstraintEdge::Store:
            {
                // Store constraint: *a = b
                // For each object c in a's points-to set, add b to c's points-to set
                unsigned srcId = src->getId();
                unsigned dstId = dst->getId();
                
                bool changed = false;
                if (pts.find(srcId) != pts.end())
                {
                    for (auto pointee : pts[srcId])
                    {
                        if (pts[pointee].insert(dstId).second)
                        {
                            changed = true;
                        }
                    }
                }
                
                if (changed)
                {
                    // Propagate to load edges from the stored objects
                    for (auto pointee : pts[srcId])
                    {
                        for (auto loadEdge : consg->getConstraintNode(pointee)->getOutEdges())
                        {
                            if (loadEdge->getEdgeKind() == SVF::ConstraintEdge::Load)
                            {
                                worklist.push(loadEdge);
                            }
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }
}