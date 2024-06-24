#include "TAWriter.h"

#include "TASchema.h"
#include "TASchemeAttribute.h"

using namespace std;

static string quote(const string &name) { return '"' + name + '"'; }

// Write the title for the given section to the output stream
void TAWriter::writeSectionTitle(TASection section) {
    string sectionTitle;
    switch (section) {
        // should never happen
        case TASection::FileStart: throw "unreachable";

        case TASection::SchemeTuple:
        sectionTitle = "SCHEME TUPLE";
        break;
        case TASection::SchemeAttribute:
        sectionTitle = "SCHEME ATTRIBUTE";
        break;
        case TASection::FactTuple:
        sectionTitle = "FACT TUPLE";
        break;
        case TASection::FactAttribute:
        sectionTitle = "FACT ATTRIBUTE";
        break;
    }

    out << sectionTitle << " :" << endl;
}

// If we are in a section prior to the given section, this will move us into it
// and write the section title.
//
// If we are already in the given section, does nothing.
//
// If we are in a section past the given section, throws SectionOutOfOrder to
// indicate that there must be a bug.
void TAWriter::beginIfNecessary(TASection section) {
    if (currentSection < section) {
        writeSectionTitle(section);
        // An implication of this is that if no fact tuples are written, that
        // section will not be present in the TA file at all.
        currentSection = section;
    } else if (currentSection == section) {
        // do nothing
    } else {
        throw SectionOutOfOrder("Attempt to write the sections of the TA file out of order");
    }
}

// Writes out the TA Scheme Tuple section
// This section gives an E/R description of allowed edges and nodes
void TAWriter::writeSchemeTupleSection() {
    beginIfNecessary(TASection::SchemeTuple);

    out << "//Nodes" << endl;
    out << NodeScheme::full() << endl;
    out << "//Relationships" << endl;
    writeEdgeScheme(out);
    out << endl;
}

// Writes out the TA Scheme Attribute section
// This section represents the allowed attribute names for each type of entity
void TAWriter::writeSchemeAttributeSection() {
    beginIfNecessary(TASection::SchemeAttribute);

    for (const auto &attr : TASchemeAttribute::allNodeAttrs()) {
        out << attr << endl;
    }
    for (const auto &attr : TASchemeAttribute::allEdgeAttrs()) {
        out << attr << endl;
    }
}

TAWriter::TAWriter(ostream &out): out{out}, currentSection{TASection::FileStart} {
    out << "//Full Rex Extraction" << endl;
    out << "//Original Author: Bryan J Muscedere" << endl;
    out << "//Current Author: WatForm & SWAG (University of Waterloo)" << endl;
    out << endl;

    writeSchemeTupleSection();
    writeSchemeAttributeSection();
}

TAWriter &TAWriter::operator<<(const TAONode &node) {
    beginIfNecessary(TASection::FactTuple);
    out << "$INSTANCE " << quote(node.id) << ' ' << RexNode::typeToString(node.type);
    out << endl;
    return *this;
}

TAWriter &TAWriter::operator<<(const TAOEdge &edge) {
    beginIfNecessary(TASection::FactTuple);
    out << RexEdge::typeToString(edge.type) << ' ';
    out << quote(edge.sourceId) << ' ';
    out << quote(edge.destId);
    out << endl;
    return *this;
}

static void writeSingleAttributes(ostream &out, const map<string, string> &attrs) {
    for (auto &entry : attrs) {
        out << entry.first << " = " << quote(entry.second) << ' ';
    }
}

static void writeMultiAttributes(ostream &out, const map<string, vector<string>> &attrs) {
    for (auto &entry : attrs) {
        out << entry.first << " = ( ";
        for (auto &vecEntry : entry.second) {
            out << quote(vecEntry) << ' ';
        }
        out << ") ";
    }
}

TAWriter &TAWriter::operator<<(const TAONodeAttrs &attrs) {
    if (attrs.empty())
        return *this;
    beginIfNecessary(TASection::FactAttribute);

    out << quote(attrs.id) << " { ";
    writeSingleAttributes(out, attrs.singleAttrs);
    writeMultiAttributes(out, attrs.multiAttrs);
    out << '}';
    out << endl;

    return *this;
}

TAWriter &TAWriter::operator<<(const TAOEdgeAttrs &attrs) {
    if (attrs.empty())
        return *this;
    beginIfNecessary(TASection::FactAttribute);

    out << '(';
    out << RexEdge::typeToString(attrs.edge.type) << ' ';
    out << quote(attrs.edge.sourceId) << ' ';
    out << quote(attrs.edge.destId) << ") ";

    out << "{ ";
    writeSingleAttributes(out, attrs.singleAttrs);
    writeMultiAttributes(out, attrs.multiAttrs);
    out << '}';
    out << endl;

    return *this;
}
