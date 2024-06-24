#include "CypherWriter.h"

#include "TASchema.h"
#include "TASchemeAttribute.h"

#include <algorithm>

using namespace std;


static string quote(const string &name) { return '"' + name + '"'; }

CypherWriter::CypherWriter(std::ostream &out)
  : out{ out } {
}

CypherWriter &CypherWriter::operator<<(const TAONode &node) {
  out << "CREATE ("
      << ":" << RexNode::typeToString(node.type)
      << " "
      << "{id:" << quote(node.id) << "}"
      << ");\n";
  return *this;
}

CypherWriter &CypherWriter::operator<<(const TAOEdge &edge) {
  out << "MATCH"
      << "(from {id:" + quote(edge.sourceId) + "}),"
      << "(to {id: " + quote(edge.destId) + "}) "
      << "CREATE (from)-[:" << RexEdge::typeToString(edge.type) << "]->(to);\n";
  return *this;
}

CypherWriter &CypherWriter::operator<<(const TAONodeAttrs &attrs) {
    if (attrs.empty())
        return *this;

    // match
    out << "MATCH (n {id: " << quote(attrs.id) << "})\n"
        << " SET";

    // write the attributes
    writeAttrsCommon("n", attrs);
    return *this;
}

CypherWriter &CypherWriter::operator<<(const TAOEdgeAttrs &attrs) {
    if (attrs.empty())
        return *this;
    // match
    out << "MATCH"
        << "(from {id:" + quote(attrs.edge.sourceId) + "})"
        << "-[r:" << RexEdge::typeToString(attrs.edge.type) << "]->"
        << "(to {id: " + quote(attrs.edge.destId) + "}) \n"
        << " SET";

    // write the attributes
    writeAttrsCommon("r", attrs);
    return *this;
}

void CypherWriter::writeAttrsCommon(const string& id, const TAOAttrs& attrs)
{
  bool first = true;

  // SINGLE ATTRIBUTES
  for (auto &entry : attrs.singleAttrs) {
    if (first) {
      out << " ";
      first = false;
    } else {
      out << ", ";
    }
    out << id << "." << entry.first << " = " << quote(entry.second);
  }

  // MULTI ATTRIBUTES
  for (auto &entry : attrs.multiAttrs) {
    if (first) {
      out << " ";
      first = false;
    } else {
      out << ", ";
    }
    out << id << "." << entry.first << " = [";

    bool firstVal = true;
    for (auto &attr: entry.second) {
      if (!firstVal) {
          out << ", ";
      }
      firstVal = false;
      out << quote(attr);
    }
    out << "]";
  }

  out << ";" << endl;
}
