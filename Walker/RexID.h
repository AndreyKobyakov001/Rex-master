#pragma once

#include <string> // for std::string
#include <ostream> // for std::ostream
#include <istream> // for std::istream

// Dear Rob, Davood and future readers,
//
// Ten told me to write this code like this. I don't like using exceptions for
// control flow either but C++ sucks when it comes to returning multiple types.
// You can blame her for this monstrosity. >:D    (┛ಠ_ಠ)┛彡┻━┻
//
// All IDs must have the following:
//
// * A superclass `RexID` so that they get an `IDType type` member variable
// * An exception type specific to that ID type
// * An output operator that *first* outputs the `type` and then outputs its
//   own fields
// * An input operator that does *not* read the `type` (that field is used to
//   dispatch to the right input operator)
// * An entry in the `RexID::read` function that dispatches on the ID type and
//   produces an exception. All control paths in that function should produce
//   an exception.
struct RexID {
    enum IDType {
        Decl,
        Topic
    };
    IDType type;

    explicit RexID(IDType type);
    virtual ~RexID();

    // For reading all types of IDs
    static void read(std::istream &in);
    // For writing out *just* the ID type
    static void writeType(std::ostream &out, const RexID &id);
};

struct RexDeclID : public RexID {
    // The specific feature or executable that this decl was defined in
    std::string featureName;
    // The fully qualified name of the ID. No assumptions are made about the
    // contents of this string and it does *not* need to be a valid C++ fully
    // qualified name. (In fact, it will most often not be one.)
    std::string fullyQualifiedName;
    // If the decl is static (internal linkage), we include its filename so that
    // the ID can't conflict with the same symbol in another file.
    // Can be empty if the decl does not have internal linkage.
    std::string staticFilename;
    // Boolean flag indicating if the feature name should be append or not
    bool appendFeatureName;

    RexDeclID();
    RexDeclID(std::string featureName, std::string fullyQualifiedName,
        std::string staticFilename, bool appendFeatureName);
    ~RexDeclID();
    explicit operator std::string() const;

    // Friendship isn't really needed because this is a struct
    friend std::ostream &operator<<(std::ostream &out, const RexDeclID &id);
    friend std::istream &operator>>(std::istream &in, RexDeclID &id);
};

struct FoundDecl {
    RexDeclID id;

    FoundDecl(RexDeclID id);
};

struct RexTopicID : public RexID {
    std::string topic;

    RexTopicID();
    explicit RexTopicID(std::string topic);
    ~RexTopicID();
    explicit operator std::string() const;

    // Friendship isn't really needed because this is a struct
    friend std::ostream &operator<<(std::ostream &out, const RexTopicID &id);
    friend std::istream &operator>>(std::istream &in, RexTopicID &id);
};

struct FoundTopic {
    RexTopicID id;

    FoundTopic(RexTopicID id);
};
