#pragma once

#include <string> // string
#include <vector> // string
#include <ostream> // ostream

#include "../Graph/RexNode.h"
#include "../Graph/RexEdge.h"

// Represents the names and default values of the attributes allowed for a given
// entity.
class TASchemeAttribute {
public:
    // static constexpr const char* CFG_BLOCKS = "cfgBlocks";
    static constexpr const char* CFG_CALLING_FUNCTION_ID = "callingFunctionID";
    static constexpr const char* CFG_INVOKE = "cfgInvoke";
    static constexpr const char* CFG_RETURN = "cfgReturn";
    static constexpr const char* CLASS_STYLE="class_style";
    static constexpr const char* COLOR="color";
    static constexpr const char* NO_DEF="noDef";
    // static constexpr const char* CONTAINING_FUNCTIONS="containingFuncs";
    static constexpr const char* FILE="file";
    static constexpr const char* FILENAME="filename";
    static constexpr const char* FILENAME_DECLARE="filenameDeclare";
    static constexpr const char* IS_CONTROL_FLOW="isControlFlow";
    static constexpr const char* IS_PARAM="isParam";
    static constexpr const char* IS_PUBLISHER="isPublisher";
    static constexpr const char* IS_STATIC="isStatic";
    static constexpr const char* IS_SUBSCRIBER="isSubscriber";
    static constexpr const char* IS_UNDER_CONTROL="isUnderControl";
    static constexpr const char* LABEL="label";
    static constexpr const char* LINE="line";
    static constexpr const char* ROS_CALLBACK="callbackFunc";
    static constexpr const char* ROS_IS_CALLBACK="isCallbackFunc";
    static constexpr const char* ROS_NUMBER="rosNumber";
    static constexpr const char* ROS_NUM_ATTRIBUTES="numAttributes";
    static constexpr const char* ROS_PUBLISHER_DATA="pubData";
    static constexpr const char* ROS_PUBLISHER_TYPE="publisherType";
    static constexpr const char* ROS_TIMER_DURATION = "timerDuration";
    static constexpr const char* ROS_TIMER_IS_ONESHOT="isOneshot";
    static constexpr const char* ROS_TOPIC_BUFFER_SIZE="bufferSize";
    static constexpr const char* PRESENCE_CONDITION="condition";

    struct TAAttribute {
        // Name of the attribute, must be non-empty
        std::string name;
        // Default value of the attribute, if empty, no default is assumed.
        //
        // This value is used verbatim in the TA file, so if you want a string
        // value, you need to use quotes inside the string: "\"abcdef\""
        // To have an empty string as the default value, use: "\"\""
        // To have a list of three numbers (e.g. for a color), use: "(0.0 0.0 0.0)"
        std::string defaultValue;
        bool isMulti = false;

        TAAttribute(const char *name, const char *defaultValue = "");
        TAAttribute(const char *name, bool isMulti);

        friend std::ostream &operator<<(std::ostream &out, const TAAttribute &scheme);
    };

    static const std::vector<TASchemeAttribute> &allNodeAttrs();
    static const std::vector<TASchemeAttribute> &allEdgeAttrs();

    // For attributes on $ENTITY which is not a NodeType
    TASchemeAttribute(std::vector<TAAttribute> allowedAttributes);

    TASchemeAttribute(RexNode::NodeType type, std::vector<TAAttribute> allowedAttributes);
    TASchemeAttribute(RexEdge::EdgeType type, std::vector<TAAttribute> allowedAttributes);

    friend std::ostream &operator<<(std::ostream &out, const TASchemeAttribute &scheme);

    const std::vector<TAAttribute> &getAllowedAttributes() const;

private:
    // string in order to support $ENTITY, NodeType, and EdgeType
    std::string entity;
    std::vector<TAAttribute> allowedAttributes;
};
