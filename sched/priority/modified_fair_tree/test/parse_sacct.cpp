#include <iostream>

#include "modified_fair_tree.hpp"
#include "priority_external_api.hpp"

namespace Flux {
namespace Priority {
extern priority_tree ptree;
}
}

using namespace Flux::Priority;

int main(int argc, char *argv[])
{
    int count;

    for (int i = 1; i < argc; i++) {
        count = process_association_file (NULL, argv[i]);
        std::cout << "Processed: " << count << std::endl;
        std::cout << "In tree  : " << ptree.get_node_count() << std::endl;
    }

    ptree.calc_fs_factors();
    return 0;
}
