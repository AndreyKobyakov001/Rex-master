#pragma once

#include <ostream>

#include "../Graph/TAGraph.h"

// Writes a TAGraph to an output stream in the object file format
class TAObjectWriter {
    const TAGraph &graph;

  public:
    explicit TAObjectWriter(const TAGraph &graph);
    
    friend std::ostream &operator<<(std::ostream &out, const TAObjectWriter &writer);
};
