#include "TASchemeAttribute.h"

using namespace std;
using TAAttribute = TASchemeAttribute::TAAttribute;

TASchemeAttribute::TAAttribute::TAAttribute(const char *name, const char *defaultValue):
    name{name}, defaultValue{string(defaultValue)} {}
TASchemeAttribute::TAAttribute::TAAttribute(const char *name, bool isMulti):
    name{name}, defaultValue{""}, isMulti{isMulti} {}

TASchemeAttribute::TASchemeAttribute(vector<TAAttribute> allowedAttributes):
    entity{"$ENTITY"}, allowedAttributes{allowedAttributes} {}
TASchemeAttribute::TASchemeAttribute(RexNode::NodeType type, vector<TAAttribute> allowedAttributes):
    entity{RexNode::typeToString(type)}, allowedAttributes{allowedAttributes} {}
TASchemeAttribute::TASchemeAttribute(RexEdge::EdgeType type, vector<TAAttribute> allowedAttributes):
    entity{"(" + string(RexEdge::typeToString(type)) + ")"}, allowedAttributes{allowedAttributes} {}

const std::vector<TAAttribute> &
TASchemeAttribute::getAllowedAttributes() const {
  return allowedAttributes;
}

const vector<TASchemeAttribute> &TASchemeAttribute::allNodeAttrs() {
    static vector<TASchemeAttribute>scheme{
        TASchemeAttribute(
            /* "$ENTITY", */
            {"x", "y", "width", "height", LABEL, FILENAME, FILENAME_DECLARE}
        ),
        TASchemeAttribute(
            RexNode::ROOT,
            {
                {"elision", "contain"},
                {COLOR, "(0.0 0.0 0.0)"},
                {FILE},
                {LINE},
                {"name"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::ASG_NODE,
            {
                {"beg"},
                {"end"},
                {FILE},
                {LINE},
                {"value"},
                {COLOR, "(0.0 0.0 0.0)"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::ROS_MSG,
            {
                {CLASS_STYLE, "4"},
                {COLOR, "(0.0 0.0 1.0)"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::ENUM,
            {
                //{FILENAME},
                {IS_CONTROL_FLOW, "0"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::FUNCTION,
            {
                //{FILENAME},
                //{FILENAME_DEFINITION},
                {IS_STATIC},
                {IS_CONTROL_FLOW, "0"},
                {"isConst"},
                {"isVolatile"},
                {"isVariadic"},
                {"visibility"},
                {COLOR, "(1.0 0.0 0.0)"},
                {"labelcolor", "(0.0 0.0 0.0)"},
                {ROS_IS_CALLBACK},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::VARIABLE,
            {
                //{FILENAME},
                //{FILENAME_DEFINITION},
                {"scopeType"},
                {IS_STATIC},
                {IS_PARAM},
                {IS_CONTROL_FLOW, "0"},
                {IS_PUBLISHER, "0"},
                {IS_SUBSCRIBER, "0"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::CLASS,
            {
                //{FILENAME},
                {"baseNum"},
                {COLOR, "(0.2 0.4 0.1)"},
                {"labelcolor", "(0.0 0.0 0.0)"},
                {CLASS_STYLE, "2"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::SUBSCRIBER,
            {
                {COLOR, "(0.4 1.0 0.4)"},
                {"labelcolor", "(0.0 0.0 0.0)"},
                {CLASS_STYLE, "6"},
                {"bufferSize"},
                {ROS_NUM_ATTRIBUTES},
                {ROS_NUMBER},
                {ROS_CALLBACK},
                {ROS_TOPIC_BUFFER_SIZE},
                {"callbackFunc"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::PUBLISHER,
            {
                {COLOR, "(1.0 0.0 0.8)"},
                {"labelcolor", "(1.0 1.0 1.0)"},
                {CLASS_STYLE, "6"},
                {ROS_NUM_ATTRIBUTES},
                {ROS_NUMBER},
                {ROS_PUBLISHER_TYPE},
                {ROS_TOPIC_BUFFER_SIZE},
                {IS_UNDER_CONTROL, "0"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::TIMER,
            {
                {COLOR, "(1.0 0.0 0.8)"},
                {"labelcolor", "(1.0 1.0 1.0)"},
                {CLASS_STYLE, "6"},
                {ROS_TIMER_DURATION, "6"},
                {ROS_TIMER_IS_ONESHOT, "6"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexNode::TOPIC,
            {
                {COLOR, "(1.0 1.0 0.6)"},
                {"labelcolor", "(0.0 0.0 0.0)"},
                {CLASS_STYLE, "5"},
                {PRESENCE_CONDITION}
            }
        ),
    };
    return scheme;
}
const vector<TASchemeAttribute> &TASchemeAttribute::allEdgeAttrs() {
    static vector<TASchemeAttribute>scheme{
        TASchemeAttribute(
            RexEdge::CALLS,
            {
                {IS_UNDER_CONTROL, "0"},
                //{NO_DEF, "0"}
                //{CFG_BLOCKS, /*multi*/ true},
                //{CONTAINING_FUNCTIONS, /*multi*/ true}
            }
        ),
        // to uncomment out these section change //multi to /*multi*/ and remove the comments
        /*TASchemeAttribute(
            RexEdge::VAR_WRITES,
            {
                {CFG_BLOCKS, //multi true},
                {CONTAINING_FUNCTIONS, //multi true}
            }
        ),
        TASchemeAttribute(
            RexEdge::PAR_WRITES,
            {
                {CFG_BLOCKS, //multi true},
                {CONTAINING_FUNCTIONS, //multi true}
            }
        ),

        TASchemeAttribute(
            RexEdge::WRITES,
            {
                {CFG_BLOCKS, //multi true}
            }
        ),
        TASchemeAttribute(
            RexEdge::VAR_INFLUENCE,
            {
                {CFG_BLOCKS, //multi true}
            }
        ),
        TASchemeAttribute(
            RexEdge::VAR_INFLUENCE_FUNC,
            {
                {CFG_BLOCKS, //multi true}
            }
        ),
        TASchemeAttribute(
            RexEdge::PAR_WRITES,
            {
                {NO_DEF, "0"}
            }
        ),*/
	TASchemeAttribute(
	    RexEdge::VAR_WRITES,
	    {
		{LINE_NUMBER}
	    }
	),
        TASchemeAttribute(
            RexEdge::PUBLISH,
            {
                {ROS_PUBLISHER_DATA}
            }
        ),
        TASchemeAttribute(
            RexEdge::NEXT_CFG_BLOCK,
            {
                {CFG_INVOKE, /*multi*/ false},
                {CFG_RETURN, /*multi*/ false},
                //{NO_DEF, "0"},
                {IS_UNDER_CONTROL, "0"},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute(
            RexEdge::PUBLISH,
            {
                {ROS_PUBLISHER_DATA},
                {PRESENCE_CONDITION}
            }
        ),
        TASchemeAttribute (
          RexEdge::CONTAINS,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::OBJ,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::MUTATE,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::VAR_WRITES,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::REFERENCES,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::READS,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::WRITES,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::ADVERTISE,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::SUBSCRIBE,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::VAR_INFLUENCE,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::VAR_INFLUENCE_FUNC,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::SET_TIME,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::PUBLISH_VARIABLE,
          {
            {PRESENCE_CONDITION}
          }
        ),
        TASchemeAttribute (
          RexEdge::PUBLISH_TARGET,
          {
            {PRESENCE_CONDITION}
          }
        ),
    };
    return scheme;
}

ostream &operator<<(ostream &out, const TASchemeAttribute &scheme) {
    out << scheme.entity << " {" << endl;

    for (const TAAttribute &attr : scheme.allowedAttributes) {
        out << "  " << attr.name;
        if (!attr.defaultValue.empty()) {
            out << " = " << attr.defaultValue;
        }
        out << endl;
    }

    out << "}" << endl;
    return out;
}
