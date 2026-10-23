// check_media_set.cpp: read a media set folder and report what the library finds in it.
#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: check_media_set <folder that holds member_001, member_002, ...>\n";
        return 2;
    }

    // The members of the set, as they are on disk.
    const auto members = adl::media_tree::read_media_set_tree(argv[1]);
    if (!members)
    {
        std::cerr << "cannot read " << members.error().path.string() << ": " << to_string(members.error().error)
                  << '\n';
        return 1;
    }

    // The set the members describe: its loads and batches, and where it departs from the standard.
    const auto set = adl::arinc665::read_media_set(*members);
    if (!set)
    {
        std::cerr << "not a media set: " << to_string(set.error()) << '\n';
        return 1;
    }

    std::cout << members->size() << " member(s), " << set->loads.size() << " load(s), " << set->batches.size()
              << " batch(es)\n";
    for (const adl::arinc665::Finding& finding : set->findings)
    {
        std::cout << "  " << finding.clause << ": " << finding.message << '\n';
    }
    return 0;
}
