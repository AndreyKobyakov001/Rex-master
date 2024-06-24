#pragma once

#include <vector>
#include <boost/filesystem.hpp>

#include <unordered_set> // for unordered_set
#include <set>
#include <sstream> // for stringstream
#include <iostream>
#include <unordered_map>
#include <chrono>

#include <boost/filesystem/fstream.hpp> // fs::ifstream

#include "TAObjectFile.h"
#include "TAWriter.h"
#include "LinkAttrs.h"
#include "../Walker/RexID.h"


using namespace std;
using namespace std::chrono;
namespace fs = boost::filesystem;

// C++20 adds this method to unordered_set
static bool contains(const unordered_set<string> &set, const string &s) {
    return set.find(s) != set.cend();
}

static bool isEstablished(const unordered_set<string> &declaredNodes, const TAOEdge &edge) {
    return contains(declaredNodes, edge.sourceId) && contains(declaredNodes, edge.destId);
}

// Link the given .tao files together into a single TA Graph and write that
// graph to the given file path.
//
// Attempts to limit memory usage as much as possible during the linking process.
template<class Writer>
void linkObjectFiles(const vector<fs::path> &taoFiles, Writer &writer) {
    vector<TAOFileMetadata> objMetadata;
    // Mapping of object files to their starting position
    // in an input stream
    unordered_map<string, streampos> objFilePos;
    objFilePos.reserve(taoFiles.size());

    // We know that there will be exactly as many items here as there are files
    objMetadata.reserve(taoFiles.size());

    // Start to read each file
    for (unsigned int iter = 0; iter < taoFiles.size(); iter++) {
		const fs::path &path = taoFiles[iter];
        fs::ifstream file(path);

        /**while (file.fail()) {
            file.clear();
            file.open(path);
        }**/
        // If any of these occur, the .tao file is malformed
        file.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);

        objMetadata.emplace_back();
        file >> objMetadata.back();

        objFilePos[path.string()] = file.tellg();

    }

    // Build a symbol table so we can look up IDs and purge unestablished edges
    unordered_set<string> declaredNodes;
    unordered_map<string, RexNode::NodeType> declaredNodesType;
    high_resolution_clock::time_point start = high_resolution_clock::now();

    TAONode node;
    for (unsigned int iter = 0; iter < taoFiles.size(); iter++) {
		const fs::path &path = taoFiles[iter];
		fs::ifstream objFile(path);
		objFile.seekg(objFilePos[path.string()]);
        const TAOFileMetadata &meta = objMetadata[iter];
        for (unsigned int j = 0; j < meta.nodesSize; j++)
        {
            objFile >> node;
            if(declaredNodes.find(node.id) == declaredNodes.end())
            {
                declaredNodes.insert(node.id);
                declaredNodesType[node.id] = node.type;
                // We can write each node's $INSTANCE line immediately as soon as we read it
                //TODO: Only do this if we haven't previously inserted the ID
                writer << node;
            }
        }
        objFilePos[path.string()] = objFile.tellg();

    }

    high_resolution_clock::time_point end = high_resolution_clock::now();
	auto duration = duration_cast<seconds>(end - start).count();
	cout << "Wrote Nodes in " << duration << " seconds" << endl;

	start = high_resolution_clock::now();
    // Start to write the rest of the TA file using the symbol table to
    // establish edges on the fly
    TAOEdge edge;
    set<TAOEdge> edges;
	for (unsigned int iter = 0; iter < taoFiles.size(); iter++) {
		const fs::path &path = taoFiles[iter];
        const TAOFileMetadata &meta = objMetadata[iter];
		fs::ifstream objFile(path);
		objFile.seekg(objFilePos[path.string()]);
        for (unsigned int j = 0; j < meta.unestablishedEdges; j++) {
            objFile >> edge;

            if (edges.find(edge) == edges.end() && isEstablished(declaredNodes, edge)) {
                writer << edge;
                edges.insert(edge);
            }
        }
		objFilePos[path.string()] = objFile.tellg();

    }
    end = high_resolution_clock::now();
	duration = duration_cast<seconds>(end - start).count();
	cout << "Established Edges in " << duration << " seconds" << endl;

	start = high_resolution_clock::now();
    // Already established edges can be written without any additional lookups
	for (unsigned int iter = 0; iter < taoFiles.size(); iter++) {
		const fs::path &path = taoFiles[iter];
        const TAOFileMetadata &meta = objMetadata[iter];
		fs::ifstream objFile(path);
		objFile.seekg(objFilePos[path.string()]);
        for (unsigned int j = 0; j < meta.establishedEdges; j++) {
            objFile >> edge;
            if (edges.find(edge) == edges.end())
            {
                writer << edge;
                edges.insert(edge);
            }

        }
		objFilePos[path.string()] = objFile.tellg();
    }
    end = high_resolution_clock::now();
	duration = duration_cast<seconds>(end - start).count();
	cout << "Wrote Already Established Edges in " << duration << " seconds" << endl;

	start = high_resolution_clock::now();
    // Write node attributes
    linkAttrs<TAONodeAttrs>(taoFiles, writer, [&objMetadata](int file) {
        return objMetadata[file].nodesWithAttrs;
    }, [&declaredNodes](const TAONodeAttrs &nodeAttrs) {
        // Only keep nodes that have actually been established. This is
        // necessary because we allow .tao files to store node attributes
        // before we even know that the TA file will contain that node. We
        // have to do that because otherwise we wouldn't be able to add
        // attributes for most things at all.
        return contains(declaredNodes, nodeAttrs.id);
    }, [&declaredNodesType](TAONodeAttrs &nodeAttrs) {
        nodeAttrs.type = declaredNodesType[nodeAttrs.id];
    }, objFilePos);
    end = high_resolution_clock::now();
	duration = duration_cast<seconds>(end - start).count();
	cout << "Wrote Node Attributes in " << duration << " seconds" << endl;

	start = high_resolution_clock::now();
    // Write edge attributes only for established edges
    linkAttrs<TAOEdgeAttrs>(taoFiles, writer, [&objMetadata](int file) {
        return objMetadata[file].edgesWithAttrs;
    }, [&declaredNodes](const TAOEdgeAttrs &edgeAttrs) {
        return isEstablished(declaredNodes, edgeAttrs.edge);
    }, [=](const TAOEdgeAttrs &edgeAttrs) {
         // do nothing, surpress warnings
         (void)edgeAttrs;
    }, objFilePos);
    end = high_resolution_clock::now();
	duration = duration_cast<seconds>(end - start).count();
	cout << "Wrote Edge Attributes in " << duration << " seconds" << endl;

}
