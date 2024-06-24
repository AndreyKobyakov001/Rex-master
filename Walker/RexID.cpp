#include "RexID.h"

#include <vector> // for vector
#include <sstream> // for stringstream

using namespace std;

RexID::RexID(RexID::IDType type): type{type} {}
RexID::~RexID() {}

// Reads all semicolon separated sections of the given stream until `;;` is
// found at the end. None of the intermediary `;` will be included in the
// result vector.
//
// The vector itself will not be modified. The sections will be read into its
// elements. This allows you to not have to pay to copy the strings later if
// not needed. An exception will be thrown if more sections are read than
// elements in the given vector.
//
// Returns the number of sections read.
//
// Throws a logic_error if no `;` is found or if the output string would be empty.
static unsigned int parseIDSections(istream &in, const vector<string *> &sections) {
    unsigned int numSections = 0;
    for (string *section : sections) {
        // Taking advantage of short-circuit evaluation: data.empty() is
        // evaluated after getline() is done.
        if (!getline(in, *section, ';') || section->empty()) {
            throw logic_error("Unable to find `;` in read data");
        }
        numSections += 1;

        // Check for `;;`
        if (in.peek() == ';') {
            // Not reading into all the sections is potentially OK.
            break;
        }
    }

    // Check for `;;`
    if (in.peek() == ';') {
        in.ignore();
        return numSections;
    } else {
        // Reading too many sections is NOT ok
        throw logic_error("Unexpected number of sections in ID");
    }
}

void RexID::read(istream &in) {
    string type;
    if (!getline(in, type, ';') || type.empty()) {
        throw logic_error("Unable to find `;` in read data");
    }

    // All control paths in this method should throw an exception

    if (type == "decl") {
        RexDeclID id;
        in >> id;
        throw FoundDecl(id);
    } else if (type == "topic") {
        RexTopicID id;
        in >> id;
        throw FoundTopic(id);
    } else {
        throw logic_error("Invalid ID type: " + type);
    }
}

void RexID::writeType(ostream &out, const RexID &id) {
    switch (id.type) {
    case RexID::Decl:
        out << "decl";
        return;
    case RexID::Topic:
        out << "topic";
        return;
    // No default case so that compiler will warn when new variants are added
    }
}

RexDeclID::RexDeclID(): RexID(RexID::Decl) {}
RexDeclID::RexDeclID(
    string featureName,
    string fullyQualifiedName,
    string staticFilename,
    bool appendFeatureName
):
    RexID(RexID::Decl),
    featureName{featureName},
    fullyQualifiedName{fullyQualifiedName},
    staticFilename{staticFilename},
    appendFeatureName{appendFeatureName} {}
RexDeclID::~RexDeclID() {}

RexDeclID::operator std::string() const {
    stringstream ss;
    ss << *this;
    return ss.str();
}

ostream &operator<<(ostream &out, const RexDeclID &id) {
    RexID::writeType(out, id);
    if (id.appendFeatureName){
        out << ';' << id.featureName;
    }
    out << ';' << id.fullyQualifiedName;
    if (!id.staticFilename.empty()) {
        out << ";static;" << id.staticFilename;
    }
    // Having a ";;" ending makes it easier to know when to stop parsing
    return out << ";;";
}
istream &operator>>(istream &in, RexDeclID &id) {
    string staticKeyword;
    vector<string *> parts{
        &id.featureName,
        &id.fullyQualifiedName,
        &staticKeyword,
        &id.staticFilename
    };
    unsigned int readSections = parseIDSections(in, parts);
    if (readSections < 2) {
        throw logic_error("Invalid ID");
    } else if (readSections > 2) {
        if (staticKeyword != "static" || id.staticFilename.empty()) {
            throw logic_error("Invalid ID");
        }
    }
    return in;
}

FoundDecl::FoundDecl(RexDeclID id): id{id} {}

RexTopicID::RexTopicID(): RexID(RexID::Topic) {}
RexTopicID::RexTopicID(string topic): RexID(RexID::Topic), topic{topic} {}
RexTopicID::~RexTopicID() {}

RexTopicID::operator std::string() const {
    stringstream ss;
    ss << *this;
    return ss.str();
}

FoundTopic::FoundTopic(RexTopicID id): id{id} {}

ostream &operator<<(ostream &out, const RexTopicID &id) {
    RexID::writeType(out, id);
    out << ';' << id.topic;
    // Having a ";;" ending makes it easier to know when to stop parsing
    return out << ";;";
}
istream &operator>>(istream &in, RexTopicID &id) {
    vector<string *> parts{
        &id.topic
    };
    // All parts are required
    if (parseIDSections(in, parts) < parts.size()) {
        throw logic_error("Invalid ID");
    }
    return in;
}
