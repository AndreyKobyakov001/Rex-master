#include "CSVWriter.h"

#include "TASchema.h"
#include "TASchemeAttribute.h"

#include <algorithm>

using namespace std;

// pair<attr name, isMulti>
static vector<pair<string,bool>> allNodeAttrs;
static vector<pair<string,bool>> allEdgeAttrs;

CSVWriter::CSVWriter(std::ostream &nodes, std::ostream &edges)
  : nodes{ nodes }, edges{ edges } {
    for (const auto &entity : TASchemeAttribute::allNodeAttrs()) {
      for (const auto &attr : entity.getAllowedAttributes()) {
        allNodeAttrs.push_back({attr.name, attr.isMulti});
      }
    }
    sort(allNodeAttrs.begin(), allNodeAttrs.end());
    allNodeAttrs.erase(unique(allNodeAttrs.begin(), allNodeAttrs.end()), allNodeAttrs.end());

    for (const auto &entity : TASchemeAttribute::allEdgeAttrs()) {
      for (const auto &attr : entity.getAllowedAttributes()) {
        allEdgeAttrs.push_back({attr.name, attr.isMulti});
      }
    }
    sort(allEdgeAttrs.begin(), allEdgeAttrs.end());
    allEdgeAttrs.erase(unique(allEdgeAttrs.begin(), allEdgeAttrs.end()), allEdgeAttrs.end());

    nodes << "id:ID"
          << "\t"
          << ":LABEL";
    for (auto &attr : allNodeAttrs) {
      nodes << "\t"
            << attr.first;
      if (attr.second) { // isMulti
          nodes << ":string[]"; // set the column type to be a string array
      }
    }
    nodes << endl;

    // edge file
    edges << ":START_ID"
          << "\t"
          << ":END_ID"
          << "\t"
          << ":TYPE"
	  << ":LINE_NUMBER";
    for (auto &attr : allEdgeAttrs) {
      edges << "\t"
            << attr.first;
      if (attr.second) { // isMulti
          edges << ":string[]"; // set the column type to be a string array
      }
    }
    edges << endl;
}

// Nodes and edges are only written once their attributes have been fully linked
CSVWriter &CSVWriter::operator<<(const TAONode & /*node*/) {
    // do nothing
    return *this;
}
CSVWriter &CSVWriter::operator<<(const TAOEdge & /*edge*/) {
    // do nothing
    return *this;
}

CSVWriter &CSVWriter::operator<<(const TAONodeAttrs &attrs) {
    nodes << attrs.id
        << '\t'
        << RexNode::typeToString(attrs.type);
    for (auto &attr : allNodeAttrs) {
        nodes << '\t';
        if (!attr.second /* !isMulti */ && attrs.singleAttrs.count(attr.first)) {
            // cannot use operator[] because attrs is passed in as a constant
            nodes << attrs.singleAttrs.at(attr.first);
        } else if (attr.second /* isMulti */ && attrs.multiAttrs.count(attr.first)) {
            bool first = true;
            for (auto &val : attrs.multiAttrs.at(attr.first)) {
                if (!first) {
                    nodes << "-"; // array delimiter
                } else {
                    first = false;
                }
                nodes << val;
            }
        }
    }
    nodes << endl;
    return *this;
}

CSVWriter &CSVWriter::operator<<(const TAOEdgeAttrs &attrs) {
    edges << attrs.edge.sourceId
        << "\t"
        << attrs.edge.destId
        << "\t"
        << RexEdge::typeToString(attrs.edge.type);
    for (auto &attr : allEdgeAttrs) {
        edges << '\t';
        if (!attr.second /* !isMulti */ && attrs.singleAttrs.count(attr.first)) {
            // cannot use operator[] because attrs is passed in as a constant
            edges << attrs.singleAttrs.at(attr.first);
        } else if (attr.second /* isMulti */ && attrs.multiAttrs.count(attr.first)) {
            bool first = true;
            for (auto &val : attrs.multiAttrs.at(attr.first)) {
                if (!first) {
                    edges << "-"; // array delimiter
                } else {
                    first = false;
                }
                edges << val;
            }
        }
    }
    edges << endl;
    return *this;
}

