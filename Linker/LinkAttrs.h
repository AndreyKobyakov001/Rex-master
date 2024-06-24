#pragma once

#include <list>
#include <algorithm> // for std::sort
#include <memory> // for std::unique_ptr
#include <limits>
#include <unordered_map>

#include <boost/filesystem/fstream.hpp> // boost::filesystem::ifstream

#include "TAWriter.h"

// This file contains an implementation of a "linking" process for node/edge
// attributes. The process of linking attributes is the same for both nodes and
// edges, however the types are different because we need to represent them
// differently.
//
// Node/edge attributes can be distributed across .tao files. We can add
// attributes to any node/edge while walking any file. This process exists
// because we really wouldn't want to lose any of that information by assuming
// that all the attributes for a given node/edge will always be added in at most
// one file.
//
// Much like the rest of the linking implementation, this process is designed to
// load as little in memory as possible. At least in the initial implementation,
// we have a memory usage bounded by the number of .tao files. This is much
// better than a brute force approach that would likely be bounded by the number
// of nodes/edges with attributes.
//
// The algorithm is inspired by the "merge" routine in merge sort. It requires
// that the input lists of attributes be sorted by the node/edge. We use this
// preprocessing step to link all of the attributes in only a single pass
// through the .tao files.

template<class TAOAttrs, class Writer, class AttrsSize, class ShouldKeep, class PreOutput>
void linkAttrs(
    // Input .tao files
    const std::vector<boost::filesystem::path> &taoFiles,
    // Output
    Writer &writer,
    // Callback for determining how many TAOAttrs there are to load in each file
    // Passed the index into objFiles, must return an unsigned int
    AttrsSize &&attrsSize,
    // Callback for filtering the TAOAttrs instances that should be written out
    // The TAOAttrs instance will only be written to taFile if this returns
    // something truthy when a const reference to that TAOAttrs is passed in
    ShouldKeep &&shouldKeep,
    // Modifications before outputing
    PreOutput &&preOutput,
    // Mapping of object files to their starting position
    // in an input stream
    //
    // This mapping will be updated at the end of the routine
    std::unordered_map<std::string, std::streampos> &objFilePos
) {
    namespace fs = boost::filesystem;
    using std::istream;
    using std::vector;
    using std::list;
    using std::unique_ptr;

    // Note: You definitely don't need to compare against nullptr directly in
    // C++ (the `!` operator is sufficient). I just think it makes things more
    // clear and easier to understand.

    // Using inner classes to organize smaller details and repetitive code.
    // Hopefully that makes for a more readable final algorithm at the bottom.

    struct AttrsSlot {
		
		std::string taoFile;
        // The number of nodes/edges remaining to be loaded
        unsigned int remaining;
        
        std::streampos currPos;
        // The currently loaded attributes from the file. nullptr if we have
        // finished reading all of the attributes from the file.
        //
        // Note that remaining == 0 does *not* imply that this is nullptr
        // because even if we don't have anything remaining we may still have
        // not processed the very last TAOAttrs.
        TAOAttrs *attrs;

        AttrsSlot(const fs::path &taoFile, unsigned int remaining, std::streampos currPos):
            taoFile{taoFile.string()}, remaining{remaining}, currPos{currPos}, attrs{new TAOAttrs} {
            // Must initialize attrs with an instance before calling load or
            // else we will read into uninitialized memory
            load();
        }

        // A slot is only considered empty if its final value has been read.
        bool isEmpty() const {
            return attrs == nullptr;
        }
        
        std::streampos getCurrPos() const {
			return currPos;
		}

        // For finding duplicates to merge with
        // NOTE: If `this` is the same as `other`, this can result in undefined
        // behaviour happening when `merge` is called in the algorithm below.
        bool canMerge(const AttrsSlot &other) const {
            if (attrs == nullptr || other.attrs == nullptr) {
                // Either or both are nullptr
                return attrs == other.attrs;
            } else {
                // Both must not be nullptr
                return attrs->canMerge(*other.attrs);
            }
        }

        // Load the next set of attributes (if any)
        void load() {
            if (remaining > 0) {
				fs::ifstream objFile(taoFile);
				objFile.seekg(currPos);
                objFile >> *attrs;
                currPos = objFile.tellg();
                remaining--;
            } else if (attrs != nullptr) {
                delete attrs;
                attrs = nullptr;
            }
        }

        // For sorting
        bool operator<(const AttrsSlot &other) {
            // Always sort nullptr later and try to preserve the current order
            // if the later element is already nullptr. (Avoids unnecessary swaps)
            // We try to keep nullptr later so that we don't have to iterate
            // past nullptr values all the time in the algorithm.
            if (attrs == other.attrs) {
                return false;
            } else if (attrs == nullptr) {
                return false;
            } else if (other.attrs == nullptr) {
                return true;
            } else {
                return *attrs < *other.attrs;
            }
        }
    };

    unsigned int remainingFiles = 0;

    list<AttrsSlot> attrsSlots;
    //attrsSlots.reserve(taoFiles.size());
    // Perform an initial load of a single set of attributes from each file
    for (unsigned int i = 0; i < taoFiles.size(); i++) {
        unsigned int remaining = attrsSize(i);
        attrsSlots.emplace_back(taoFiles[i], remaining, objFilePos[taoFiles[i].string()]);
        if (!attrsSlots.back().isEmpty()) {
            remainingFiles += 1;
        }
    }
	attrsSlots.sort();
    while (remainingFiles > 1) {
        
        // Since elements with attrs == nullptr are sorted to the back, we can
        // find the next element to look at very easily at the front.
        // This is guaranteed to be there with attrs != nullptr because
        // remainingFiles > 0.
        AttrsSlot &current = attrsSlots.front();

        // Going to just mutate `current` because we're going to overwrite the
        // memory in its `attrs` field at the end of this loop iteration anyway
        TAOAttrs &attrs = *current.attrs;
        // Look for any duplicates to merge with (this is the "linking" step)
        // Must start at 1 so we don't accidentally try to merge current with itself

		auto otherIt = attrsSlots.begin();
		otherIt++;
		AttrsSlot &other = *otherIt;
		if (current.canMerge(other))
		{
			// Merge the duplicate into the current
			attrs.merge(*other.attrs);
			// We can load the next one right away since we are done
			// processing this duplicate
			other.load();
			if (other.isEmpty()) {
				remainingFiles -= 1;
				objFilePos[other.taoFile] = other.currPos;
				attrsSlots.erase(otherIt);
				std::cout << "Remaining Files: " << remainingFiles << std::endl;
			}
			else
			{
				auto it = attrsSlots.begin(); it++; it++;
				while(it != attrsSlots.end())
				{
					AttrsSlot &next = *it;
					if(!(next < other))
					{	
						attrsSlots.insert(it, other);
						attrsSlots.erase(otherIt);
						break;
					}
					it++;
				}
                if(it == attrsSlots.end())
                {
                    attrsSlots.insert(it, other);
                    attrsSlots.erase(otherIt);
                }
			}

		} 
		else 
		{
			// Only write out the merged attributes if we are supposed to
			if (shouldKeep(attrs)) {
				preOutput(attrs);
				writer << attrs;
			}
			
			current.load();
			if (current.isEmpty()) {
				remainingFiles -= 1;
				objFilePos[current.taoFile] = current.currPos;
				attrsSlots.erase(attrsSlots.begin());
				std::cout << "Remaining Files: " << remainingFiles << std::endl;
			}
			else
			{
				auto it = attrsSlots.begin(); it++;
				while(it != attrsSlots.end())
				{
					AttrsSlot &next = *it;
					if(!(next < current))
					{
						attrsSlots.insert(it, current);
						attrsSlots.erase(attrsSlots.begin());
						break;
					}
					it++;
				}
                if(it == attrsSlots.end())
                {
                    attrsSlots.insert(it, current);
                    attrsSlots.erase(attrsSlots.begin());
                }
			}
		}	
    }
    while(remainingFiles > 0)
	{
		AttrsSlot &current = attrsSlots.front();

		// Going to just mutate `current` because we're going to overwrite the
		// memory in its `attrs` field at the end of this loop iteration anyway
		TAOAttrs &attrs = *current.attrs;

		if (shouldKeep(attrs)) {
			preOutput(attrs);
			writer << attrs;
		}

		// Load the next item in this slot since we are done with this one
		current.load();
		if (current.isEmpty()) {
			remainingFiles -= 1;
			objFilePos[current.taoFile] = current.currPos;
			attrsSlots.erase(attrsSlots.begin());
			std::cout << "Remaining Files: " << remainingFiles << std::endl;
		}
	}	
    
}
