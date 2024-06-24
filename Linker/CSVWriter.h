#pragma once

#include <ostream> // ostream
#include <string> // string
#include <exception> // exception

#include "TAObjectFile.h"

class CSVWriter {
  std::ostream &nodes;
  std::ostream &edges;

public:
  explicit CSVWriter(std::ostream &nodes, std::ostream &edges);

  // Fact Tuple section
  CSVWriter &operator<<(const TAONode &node);
  CSVWriter &operator<<(const TAOEdge &fact);

  // Fact Attribute Section
  CSVWriter &operator<<(const TAONodeAttrs &attrs);
  CSVWriter &operator<<(const TAOEdgeAttrs &attrs);
};
