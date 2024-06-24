#include "TASchema.h"

#include "../Graph/RexEdge.h"

using namespace std;

NodeScheme::NodeScheme(RexNode::NodeType type, std::vector<NodeScheme> subnodes):
    type{type}, subnodes{subnodes} {}
NodeScheme::NodeScheme(RexNode::NodeType type): type{type} {}

// Returns the full node scheme for our TA file
const NodeScheme &NodeScheme::full() {
    // Every node type should probably be somewhere in this hierarchy, though
    // that is not strictly required.
    static NodeScheme root(RexNode::ROOT, vector<NodeScheme>{
        NodeScheme(RexNode::ASG_NODE, vector<NodeScheme>{
            NodeScheme(RexNode::CLASS),
            NodeScheme(RexNode::STRUCT),
            NodeScheme(RexNode::UNION),
            NodeScheme(RexNode::ENUM),
            NodeScheme(RexNode::FUNCTION),
            NodeScheme(RexNode::VARIABLE),
            NodeScheme(RexNode::RETURN),
        }),
        NodeScheme(RexNode::ROS_MSG, vector<NodeScheme>{
            NodeScheme(RexNode::TOPIC),
            NodeScheme(RexNode::PUBLISHER),
            NodeScheme(RexNode::SUBSCRIBER),
            NodeScheme(RexNode::NODE_HANDLE),
            NodeScheme(RexNode::TIMER),
        })
    });
    return root;
}

ostream &operator<<(ostream &out, const NodeScheme &scheme) {
    string typeStr = RexNode::typeToString(scheme.type);
    for (const auto &sub : scheme.subnodes) {
        out << "$INHERIT " << RexNode::typeToString(sub.type) << " " << typeStr << endl;
    }
    for (const auto &sub : scheme.subnodes) {
        out << sub;
    }
    return out;
}

void writeEdgeScheme(std::ostream &out) {
    // This code is specifically designed to get the compiler to warn us if it
    // needs to be updated when the EdgeType enum changes. That's why it uses
    // a loop for something that could be done without a loop otherwise.

    for (unsigned int i = 0; i < RexEdge::NUM_EDGE_TYPES; i++) {
        RexEdge::EdgeType edgeType = static_cast<RexEdge::EdgeType>(i);
        out << RexEdge::typeToString(edgeType);

        RexNode::NodeType left, right;
        switch (edgeType) {
            case RexEdge::CONTAINS:
                left = RexNode::ROOT; right = RexNode::ROOT;
                break;
            case RexEdge::REFERENCES:
                left = RexNode::NODE_HANDLE; right = RexNode::ROS_MSG;
                break;
            case RexEdge::CALLS:
                left = RexNode::ROOT; right = RexNode::ROOT;
                break;
            case RexEdge::READS:
                left = RexNode::VARIABLE; right = RexNode::FUNCTION;
                break;
            case RexEdge::WRITES:
                left = RexNode::FUNCTION; right = RexNode::VARIABLE;
                break;
            case RexEdge::ADVERTISE:
                left = RexNode::PUBLISHER; right = RexNode::TOPIC;
                break;
            case RexEdge::SUBSCRIBE:
                left = RexNode::TOPIC; right = RexNode::SUBSCRIBER;
                break;
            case RexEdge::PUBLISH:
                left = RexNode::PUBLISHER; right = RexNode::TOPIC;
                break;
            case RexEdge::VAR_WRITES:
                left = RexNode::VARIABLE; right = RexNode::VARIABLE;
                break;
            case RexEdge::PAR_WRITES:
                left = RexNode::VARIABLE; right = RexNode::VARIABLE;
                break;
            case RexEdge::RET_WRITES:
                left = RexNode::VARIABLE; right = RexNode::VARIABLE;
                break;
            case RexEdge::VAR_INFLUENCE:
                left = RexNode::VARIABLE; right = RexNode::ROS_MSG;
                break;
            case RexEdge::VAR_INFLUENCE_FUNC:
                left = RexNode::VARIABLE; right = RexNode::FUNCTION;
                break;
            case RexEdge::SET_TIME:
                left = RexNode::TIMER; right = RexNode::FUNCTION;
                break;
            case RexEdge::PUBLISH_VARIABLE:
                left = RexNode::VARIABLE; right = RexNode::TOPIC;
                break;
            case RexEdge::PUBLISH_TARGET:
                left = RexNode::TOPIC; right = RexNode::VARIABLE;
                break;
            case RexEdge::OBJ:
                left = RexNode::VARIABLE; right = RexNode::CLASS;
                break;
            case RexEdge::MUTATE:
                left = RexNode::FUNCTION; right = RexNode::VARIABLE;
                break;
            case RexEdge::NEXT_CFG_BLOCK:
                left = RexNode::CFG_BLOCK; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::RET_CFG_BLOCK:
                left = RexNode::CFG_BLOCK; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::FUNCTION_CFG_LINK:
                left = RexNode::FUNCTION; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::VW_SOURCE:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::VW_DESTINATION:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PW_SOURCE:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PW_DESTINATION:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::RW_SOURCE:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::RW_DESTINATION:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::W_SOURCE:
                left = RexNode::FUNCTION; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::W_DESTINATION:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::VIF_SOURCE:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::VIF_DESTINATION:
                left = RexNode::FUNCTION; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::VI_SOURCE:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::VI_DESTINATION:
                left = RexNode::ROS_MSG; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::C_SOURCE:
                left = RexNode::FUNCTION; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::C_DESTINATION:
                left = RexNode::FUNCTION; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PV_SOURCE:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PV_DESTINATION:
                left = RexNode::TOPIC; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PT_SOURCE:
                left = RexNode::TOPIC; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PT_DESTINATION:
                left = RexNode::VARIABLE; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::SUB_SOURCE:
                left = RexNode::TOPIC; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::SUB_DESTINATION:
                left = RexNode::SUBSCRIBER; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PUB_SOURCE:
                left = RexNode::PUBLISHER; right = RexNode::CFG_BLOCK;
                break;
            case RexEdge::PUB_DESTINATION:
                left = RexNode::TOPIC; right = RexNode::CFG_BLOCK;
                break;

            // No default case because we want the compiler to warn us when this
            // enum changes and a case isn't covered

            // Should never happen
            case RexEdge::NUM_EDGE_TYPES: throw "unreachable";
        }
        out << ' ' << RexNode::typeToString(left);
        out << ' ' << RexNode::typeToString(right);
        out << endl;
    }
}
