#pragma once

#include <ostream> // ostream
#include <string> // string
#include <exception> // exception

#include "TAObjectFile.h"

// Writes a TA file directly to disk (without keeping anything in memory).
//
// Works by wrapping an output stream and allowing you to use the output
// operator on different types corresponding to each section. The section is
// set automatically as soon as you start writing the type corresponding to that
// section.
//
// Throws a SectionOutOfOrder error if an attempt is made to write to the
// previous section after we have already moved on from that. An example of this
// is trying to write a fact tuple after we have already started writing fact
// attributes.
class TAWriter {
    enum class TASection {
        // The numeric values of these sections is important. We use it to
        // determine their ordering so we can assert that we never write an
        // invalid TA file.

        // Special section for when the file is still empty
        FileStart = 0,

        SchemeTuple = 1,
        SchemeAttribute = 2,
        FactTuple = 3,
        FactAttribute = 4,
    };

    std::ostream &out;
    TASection currentSection;

    void writeSectionTitle(TASection section);
    void beginIfNecessary(TASection section);
    void writeSchemeTupleSection();
    void writeSchemeAttributeSection();
public:
    // Represents that an attempt was made to write something that wasn't
    // currently expected.
    struct SectionOutOfOrder : public std::exception {
        std::string message;

        explicit SectionOutOfOrder(std::string message) : message{message} {}

        const char *what() const throw() override {
            return message.c_str();
        }
    };

    // The entire TA schema is written to the stream immediately
    explicit TAWriter(std::ostream &out);

    // Fact Tuple section
    TAWriter &operator<<(const TAONode &node);
    TAWriter &operator<<(const TAOEdge &fact);

    // Fact Attribute Section
    TAWriter &operator<<(const TAONodeAttrs &attrs);
    TAWriter &operator<<(const TAOEdgeAttrs &attrs);
};
