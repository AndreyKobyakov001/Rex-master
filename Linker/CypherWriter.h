#pragma once

#include <ostream> // ostream
#include <string> // string
#include <exception> // exception

#include "TAObjectFile.h"


class CypherWriter {
  std::ostream &out;

public:
  explicit CypherWriter(std::ostream &out);

  // Fact Tuple section
  CypherWriter &operator<<(const TAONode &node);
  CypherWriter &operator<<(const TAOEdge &fact);

  // Fact Attribute Section
  CypherWriter &operator<<(const TAONodeAttrs &attrs);
  CypherWriter &operator<<(const TAOEdgeAttrs &attrs);
  void writeAttrsCommon(const std::string& id, const TAOAttrs& attrs);
};
