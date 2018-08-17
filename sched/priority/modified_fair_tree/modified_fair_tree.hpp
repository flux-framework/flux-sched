#ifndef _MODIFIED_PRIORITY_TREE_H

#include <iostream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <flux/core.h>

namespace Flux {
namespace Priority {

class prio_node;
enum class prio_node_type {root_account, account, user};

class priority_tree {
public:
    priority_tree ();
    void calc_fs_factors();
    double get_fair_share_factor (const std::string &parent,
                                  const std::string &user);
    double get_fair_share_factor (const char *parent, const char *user);
    void add_usage (const std::string &parent,
                    const std::string &user, double usage);
    void add_usage (const char *parent, const char *user, double usage);
    unsigned get_node_count ();
    void update_start ();
    void update_add_user (std::string user, std::string parent,
                          unsigned long shares);
    void update_add_account (std::string account, std::string parent,
                             unsigned long shares);
    void update_finish ();

private:
    std::shared_ptr<prio_node> root;
    std::unordered_map<std::string, std::shared_ptr<prio_node>> node_map;
    bool update_in_progress;
    std::unordered_set<std::string> known_keys;

    std::shared_ptr<prio_node> get_node (const std::string key);
    std::shared_ptr<prio_node> get_node (const std::string account,
                                         const std::string user);
    void create_node (std::string key, prio_node_type nodetype,
                      std::string name, std::string parent,
                      unsigned long shares);
    void refresh_node (std::string key, unsigned long shares);
    void remove_node (std::string key);
    friend std::ostream& operator<<(std::ostream& s, priority_tree const& t);
};

std::ostream& operator<<(std::ostream& s, priority_tree const& t);
std::ostream& operator<<(std::ostream& s, prio_node const& n);

} // namespace Priority
} // namespace Flux

#endif // _MODIFIED_PRIORITY_TREE_H
