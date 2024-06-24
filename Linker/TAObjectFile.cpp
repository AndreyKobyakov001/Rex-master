// This format is designed to make linking efficient and use as little
// memory as possible. Future versions of this code could do more to store
// everything more compactly.
//
// Note: The implementations of output operators should not typically write out
// a newline. This keeps them composable so that other `write` implementations
// can use them as well.
//
// IMPORTANT: If a .tao file is written correctly, it will be read completely
// without setting any of the flags in the istream. You should use
// istream::exceptions to throw exceptions in case something goes wrong.
// Example: in.exceptions(failbit | badbit | eofbit);

#include "TAObjectFile.h"

#include <iostream> // for cerr, endl

using namespace std;

// For outputting a string prefixed with its length.
//
// Makes reading the string faster since we know its length in advance and can
// read exactly that number of bytes + preallocate the space we need. Much
// faster to read than just putting the string in quotes because we don't need
// to search for a quote character.
class LenDataStr {
    string data;
    // Should only be used in input operator
    LenDataStr() {}
public:
    explicit LenDataStr(string data): data{data} {}

    friend ostream &operator<<(ostream &out, const LenDataStr &s) {
        out << s.data.size() << ' ' << s.data;
        return out;
    }

    static istream &read(istream &in, string &s) {
        unsigned int size;
        in >> size;

        // Ignore the separator space
        in.ignore();

        // Single allocation to read the exact length
        s.resize(size);
        in.read(&s[0], size);

        return in;
    }
};

static void writeSingleAttributes(ostream &out, const map<string, string> &attrs) {
    for (auto &entry : attrs) {
        out << LenDataStr(entry.first) << ' ' << LenDataStr(entry.second) << ' ';
    }
}

static void readSingleAttributes(istream &in, unsigned int size, map<string, string> &attrs) {
    // Need to clear the attributes or else this may just append more instead of
    // overwriting the value of `attrs`.
    attrs.clear();

    // Reuse the same buffer in every loop iteration
    string key;

    for (unsigned int i = 0; i < size; i++) {
        LenDataStr::read(in, key);
        LenDataStr::read(in, attrs[key]);

        key.clear();
    }
}

static void writeMultiAttributes(ostream &out, const map<string, vector<string>> &attrs) {
    for (auto &entry : attrs) {
        out << LenDataStr(entry.first) << ' ';

        out << entry.second.size() << ' ';
        for (auto &vecEntry : entry.second) {
            out << LenDataStr(vecEntry) << ' ';
        }
    }
}

static void readMultiAttributes(istream &in, unsigned int size, map<string, vector<string>> &attrs) {
    // Need to clear the attributes or else this may just append more instead of
    // overwriting the value of `attrs`.
    attrs.clear();

    // Reuse the same buffer in every loop iteration
    string key;

    for (unsigned int i = 0; i < size; i++) {
        LenDataStr::read(in, key);

        unsigned int valuesSize;
        in >> valuesSize;

        vector<string> &values = attrs[key];
        values.resize(valuesSize);
        for (unsigned int j = 0; j < valuesSize; j++) {
            LenDataStr::read(in, attrs[key][j]);
        }

        key.clear();
    }
}

static void mergeSingleAttributes(map<string, string> &attrs, const map<string, string> &other) {
    for (const auto &entry : other) {
        // C++20 adds a contains method to std::map that would make this nicer
        if (attrs.find(entry.first) != attrs.cend() && attrs[entry.first] != entry.second) {
            // `isControlFlow` indicates that the node is used in a control flow decision in *at least one* component.
            if (entry.first == "isControlFlow" && entry.second == "1") {
                attrs[entry.first] = "1";
            }
            // We are overwriting an attribute with a different value.
            // That means that we are *losing* some info we meant to record.
            //cerr << "Rex Linker Warning: Single-attribute was overwritten during merging: '" << entry.first << "'" << endl;

            //TODO: We need to do some intelligent merging where we do one of three things based on the attribute name:
            // 1. Merge the values (when there is a logical way to merge)
            // 2. Produce a Rex Warning (when the user's code needs fixing)
            // 3. Throw an exception (when Rex shouldn't produce conflicting values)
            // Note that we only need to merge if the values are different.
            //throw logic_error("Single-attribute '" + entry.first + "' was overwritten during merging");

        }

        attrs.insert(entry);
    }
}

static void mergeMultiAttributes(map<string, vector<string>> &attrs, const map<string, vector<string>> &other) {
    for (const auto &entry : other) {
        const vector<string> &otherValues = entry.second;

        vector<string> &values = attrs[entry.first];
        values.reserve(values.size() + otherValues.size());
        values.insert(values.end(), otherValues.cbegin(), otherValues.cend());
    }
}

TAOFileMetadata::TAOFileMetadata():
    nodesSize{0},
    unestablishedEdges{0},
    establishedEdges{0},
    nodesWithAttrs{0},
    edgesWithAttrs{0} {}

ostream &TAOFileMetadata::write(ostream &out, const TAGraph &graph) {
    out << graph.keptNodes() << ' ';
    out << graph.unestablishedEdgesSize() << ' ';
    out << graph.establishedEdgesSize() << ' ';
    out << graph.keptNodes() << ' ';
    out << graph.unestablishedEdgesSize() + graph.establishedEdgesSize();
    return out;
}

istream &operator>>(istream &in, TAOFileMetadata &meta) {
    in >> meta.nodesSize;
    in >> meta.unestablishedEdges;
    in >> meta.establishedEdges;
    in >> meta.nodesWithAttrs;
    in >> meta.edgesWithAttrs;
    return in;
}

TAONode::TAONode() {}

const string &TAONode::getID() const {
    return id;
}

ostream &TAONode::write(ostream &out, const RexNode &node) {
    out << LenDataStr(node.getID()) << ' ';
    out << LenDataStr(RexNode::typeToString(node.getType()));
    return out;
}

istream &operator>>(istream &in, TAONode &node) {
    LenDataStr::read(in, node.id);

    // Using an integer for the node type would be more compact, but then adding
    // a new NodeType might change the meaning of the integer and invalidate
    // every .tao file.
    string type;
    LenDataStr::read(in, type);
    node.type = RexNode::stringToType(type);
    return in;
}

TAOEdge::TAOEdge() {}
bool TAOEdge::operator==(const TAOEdge &other) const {
    return type == other.type && sourceId == other.sourceId && destId == other.destId;
}
bool TAOEdge::operator<(const TAOEdge &other) const {
    return RexEdge::compare(*this, other);
}

RexEdge::EdgeType TAOEdge::getType() const {
    return type;
}
const string &TAOEdge::getSourceID() const {
    return sourceId;
}
const string &TAOEdge::getDestinationID() const {
    return destId;
}

ostream &TAOEdge::write(ostream &out, const RexEdge &edge) {
    out << LenDataStr(RexEdge::typeToString(edge.getType())) << ' ';
    out << LenDataStr(edge.getSourceID()) << ' ';
    out << LenDataStr(edge.getDestinationID());
    return out;
}

istream &operator>>(istream &in, TAOEdge &edge) {
    // Using an integer for the edge type would be more compact, but then adding
    // a new EdgeType might change the meaning of the integer and invalidate
    // every .tao file.
    string type;
    LenDataStr::read(in, type);
    edge.type = RexEdge::stringToType(type);
    LenDataStr::read(in, edge.sourceId);
    LenDataStr::read(in, edge.destId);
    return in;
}

bool TAOAttrs::empty() const {
    return singleAttrs.empty() && multiAttrs.empty();
}

TAONodeAttrs::TAONodeAttrs() {}

const string &TAONodeAttrs::getID() const {
    return id;
}

bool TAONodeAttrs::canMerge(const TAONodeAttrs &other) const {
    return id == other.id;
}
void TAONodeAttrs::merge(const TAONodeAttrs &other) {
    mergeSingleAttributes(singleAttrs, other.singleAttrs);
    mergeMultiAttributes(multiAttrs, other.multiAttrs);
}
bool TAONodeAttrs::operator<(const TAONodeAttrs &other) const {
    return RexNode::compare(*this, other);
}
bool TAONodeAttrs::empty() const {
    return singleAttrs.empty() && multiAttrs.empty();
}

ostream &TAONodeAttrs::write(ostream &out, const RexNode &node) {
    out << LenDataStr(node.getID()) << ' ';

    out << node.getNumSingleAttributes() << ' ' << node.getNumMultiAttributes() << ' ';
    writeSingleAttributes(out, node.getSingleAttributes());
    writeMultiAttributes(out, node.getMultiAttributes());
    return out;
}

istream &operator>>(istream &in, TAONodeAttrs &attrs) {
    LenDataStr::read(in, attrs.id);

    unsigned int single, multi;
    in >> single >> multi;
    readSingleAttributes(in, single, attrs.singleAttrs);
    readMultiAttributes(in, multi, attrs.multiAttrs);
    return in;
}

TAOEdgeAttrs::TAOEdgeAttrs() {}

bool TAOEdgeAttrs::canMerge(const TAOEdgeAttrs &other) const {
    return edge == other.edge;
}
void TAOEdgeAttrs::merge(const TAOEdgeAttrs &other) {
    mergeSingleAttributes(singleAttrs, other.singleAttrs);
    mergeMultiAttributes(multiAttrs, other.multiAttrs);
}
bool TAOEdgeAttrs::operator<(const TAOEdgeAttrs &other) const {
    return edge < other.edge;
}
bool TAOEdgeAttrs::empty() const {
    return singleAttrs.empty() && multiAttrs.empty();
}

ostream &TAOEdgeAttrs::write(ostream &out, const RexEdge &edge) {
    TAOEdge::write(out, edge) << ' ';

    out << edge.getNumSingleAttributes() << ' ' << edge.getNumMultiAttributes() << ' ';
    writeSingleAttributes(out, edge.getSingleAttributes());
    writeMultiAttributes(out, edge.getMultiAttributes());
    return out;
}

istream &operator>>(istream &in, TAOEdgeAttrs &attrs) {
    in >> attrs.edge;

    unsigned int single, multi;
    in >> single >> multi;
    readSingleAttributes(in, single, attrs.singleAttrs);
    readMultiAttributes(in, multi, attrs.multiAttrs);
    return in;
}
