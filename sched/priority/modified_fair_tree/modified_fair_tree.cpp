#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <climits>
#include <cerrno>
#include <cmath>

#include <modified_fair_tree.hpp>
#include <iostream>
#include <streambuf>
#include <iomanip>
#include <functional>
#include <algorithm>

using namespace std;

namespace Flux {
namespace Priority {

/* A node in the priority tree */
class prio_node {
public:
    prio_node (prio_node_type type, std::string name, unsigned long shares);
    void add_child (std::shared_ptr<prio_node>);
    void remove_child (std::shared_ptr<prio_node>);
    void add_parent (std::shared_ptr<prio_node>);
    std::shared_ptr<prio_node> get_parent ();
    void add_usage (double usage);
    void set_shares (unsigned long shares);
    bool is_root ();
    double get_fs_factor ();
    void tree_calc_fs_factors (); // Can only be called on root node

private:
    void tree_calc_fs_factors (double low_factor);
    void calc_fs_factor (unsigned long level_shares,
                         double level_usage,
                         double range_low,
                         double range_high);
    prio_node_type type;
    std::string name;
    unsigned long shares;
    unsigned long child_shares; /* sum of all children's shares */
    double usage;               /* historical cpu-second sum */
    double fs_factor;           /* normalized fair-share factor */
    std::shared_ptr<prio_node> parent;
    std::vector<std::shared_ptr<prio_node>> children;
    friend std::ostream& operator<<(std::ostream& s, prio_node const& n);
};

priority_tree::priority_tree ()
    : update_in_progress{false}
{
    root = make_shared<prio_node>(prio_node_type::root_account,
                                  "root", 1);
    node_map.insert (make_pair ("root", root));
}

void priority_tree::update_start ()
{
    // Make a record of keys for all known nodes.  If any keys
    // remain in known_keys when update_finish() is called, we
    // will know that they were no longer present in the external
    // table of users and accounts.
    for (auto && kv: node_map) {
        if (kv.first == string("root"))
            // The root node is permanent. We will never remove it in an update.
            continue;
        known_keys.insert (kv.first);
    }

    update_in_progress = true;
}


void priority_tree::update_finish ()
{
    // Any remaining known_keys are accounts or users that are no longer
    // present in the external table.  They need to be flagged for removal
    // when there are no longer any jobs that reference them.
    for (const auto & key: known_keys) {
        remove_node (key);
    }
    known_keys.clear ();
    update_in_progress = false;
}

void priority_tree::create_node (string key, prio_node_type nodetype,
                                 string name, string parent,
                                 unsigned long shares)
{
    shared_ptr<prio_node> parent_node = get_node (parent);
    if (parent_node == nullptr) {
        throw std::runtime_error ("parent does not exist");
    }

    shared_ptr<prio_node> node = make_shared<prio_node>(nodetype, name, shares);
    node->add_parent(parent_node);
    parent_node->add_child(node);
    node_map.insert (make_pair (key, node));
}

void priority_tree::refresh_node (string key, unsigned long shares)
{
    auto && iter = node_map.find (key);
    shared_ptr<prio_node> node = iter->second;
    node->set_shares (shares);
}

void priority_tree::remove_node (string key)
{
    // FIXME: Need to implement delayed removal if any live jobs
    // might still accumulate usage in parent accounts.
    shared_ptr<prio_node> node = get_node (key);
    node->get_parent ()->remove_child (node);
    node_map.erase (key);
}

void priority_tree::update_add_user (string user, string parent,
                                     unsigned long shares)
{
    string key = parent + "-" + user;

    auto && iter = known_keys.find (key);
    if (iter != known_keys.end()) {
        refresh_node (key, shares);
        known_keys.erase (key);
    } else {
        create_node (key, prio_node_type::user, user, parent, shares);
    }

}

void priority_tree::update_add_account (string account, string parent,
                                        unsigned long shares)
{
    string key = account;

    auto && iter = known_keys.find (key);
    if (iter != known_keys.end()) {
        refresh_node (key, shares);
        known_keys.erase (key);
    } else {
        create_node (key, prio_node_type::account, account, parent, shares);
    }
}

prio_node::prio_node (prio_node_type type, string name, unsigned long shares)
    : type{type}, name{name}, shares{shares},
      child_shares{0}, usage{0.0}, fs_factor{1.0}
{
}

void prio_node::add_child (shared_ptr<prio_node> child)
{
    child_shares += child->shares;
    children.push_back(child);
}

void prio_node::remove_child (shared_ptr<prio_node> child)
{
    child_shares -= child->shares;
    auto && iter = find (children.begin(), children.end(), child);
    children.erase (iter, iter+1);
}

void prio_node::add_parent (shared_ptr<prio_node> node)
{
    parent = node;
}

std::shared_ptr<prio_node> prio_node::get_parent ()
{
    return parent;
}

void prio_node::add_usage (double new_usage)
{
    usage += new_usage;
    if (parent)
        parent->add_usage (new_usage);
}

void prio_node::set_shares (unsigned long new_shares)
{
    long share_diff = new_shares - shares;
    shares = new_shares;
    parent->child_shares += share_diff;
}

bool prio_node::is_root ()
{
    return type == prio_node_type::root_account;
}

double prio_node::get_fs_factor ()
{
    return fs_factor;
}

/*
 * The formula for the fair-share factor is:
 *   fs = 2**(-U/S)
 * where:
 * U = my_usage / (my_usage + my_siblings_usage)
 * S = my_shares / (my_shares + my_siblings_shares)
 *
 * The fair-share factor ranges from 0.0 to 1.0:
 *  0.0 - represents an over-serviced association
 *  0.5 - usage is commensurate with assigned shares
 *  1.0 - association has no accrued usage
 *
 * Plotted, it looks like this:
 * http://www.wolframalpha.com/input/?i=Plot%5B2**(-u%2Fs)%5D,+%7Bu,+0.,+1.%7D,+%7Bs,+0.,+1.%7D
 *
 * While this gives us our fs factor relative to our siblings, we need
 * to "inherit" the fair-share of our parent's account.  Our parent's
 * fair-share factor.  Pictorially, it looks like this:
 *
 * 0.0                                                   1.0
 *  |-----------------------------------------------------|
 *  |----------|--------------------|-------------------|-|
 *             A                    B                   C
 *             |----|--------|------|
 *                  U1       U2     U3
 *
 * where A, B, and C are three accounts plotted at their fair-share
 * value.  U3 is a user in account B who has no accrued usage.  Hence,
 * U3's fair-share on its own is 1.0.  However, it inherits its parent
 * (B)'s fair-share value.  U1 and U2 have their own fair-share values
 * of 0.3 and 0.7.  However, these also inherit parent B's fairshare.
 * This is done by multiplying (one minus their local values) by the
 * the difference between their parent (B)'s fair-share value and the
 * fair-share value of the next lower sibling of their parent: A in
 * this case; and then subtracting this product from their parent B's
 * fair-share value.
 *
 * You'll see this formula in calc_fs_factor()
 */

void prio_node::calc_fs_factor (unsigned long level_shares,
                                double level_usage,
                                double range_low,
                                double range_high)
{
    double tmp_factor;
    double norm_shares;
    double norm_usage;

    if (level_shares > 0)
        norm_shares = (double)shares / level_shares;
    else
        norm_shares = 0.0;
    if (level_usage > 0.0)
        norm_usage = usage / level_usage;
    else
        norm_usage = 0.0;
    if (norm_shares > 0.0)
        tmp_factor = pow (2.0, -(norm_usage/norm_shares));
    else
        tmp_factor = 0.0;

    fs_factor = range_high - (1.0 - tmp_factor) * (range_high - range_low);
}

/*
 * Sort the nodes by their fair-share factor: low to high
 */
bool compare_prio_node (const shared_ptr<prio_node> &node1,
                        const shared_ptr<prio_node> &node2)
{
    return node1->get_fs_factor() > node2->get_fs_factor();
}

/*
 * A recursive function that is first called for the root association.
 */
void prio_node::tree_calc_fs_factors (double low_factor)
{
    // First have each child calculate its fair-share factor
    for (auto && child: children) {
        child->calc_fs_factor (child_shares, usage, low_factor, fs_factor);
    }

    // Next sort the children by their fair-share factors
    sort(children.begin(), children.end(), compare_prio_node);

    // Finally, call tree_calc_fs_factors() for all children
    double previous_fs_factor = low_factor;
    double current_low_factor = low_factor;
    for (auto && child: children) {
        // All siblings with the same fs_factor are assigned the same low_factor
        if (child->fs_factor != previous_fs_factor)
            current_low_factor = previous_fs_factor;
        child->tree_calc_fs_factors (current_low_factor);
        previous_fs_factor = child->fs_factor;
    }
}

void prio_node::tree_calc_fs_factors ()
{
    if (type != prio_node_type::root_account) {
        throw std::runtime_error("Parameterless tree_calc_fs_factors() may"
                                 " only be called for the root account.\n");
    }
    // The root account has no siblings, so the low_factor is always zero.
    tree_calc_fs_factors (0.0);
}

shared_ptr<prio_node> priority_tree::get_node (const string key)
{
    shared_ptr<prio_node> node;

    try {
        node = node_map.at (key);
    } catch (const out_of_range &err) {
        // do nothing
    }

    return node;
}

shared_ptr<prio_node> priority_tree::get_node (const string account,
                                               const string user)
{
    shared_ptr<prio_node> node;

    try {
        node = node_map.at (account + "-" + user);
    } catch (const out_of_range &err) {
        // do nothing
    }

    return node;
}

void priority_tree::calc_fs_factors()
{
    root->tree_calc_fs_factors();
}

double priority_tree::get_fair_share_factor(const std::string &parent,
                                            const std::string &user)
{
    shared_ptr<prio_node> node;

    node = get_node (parent, user);
    if (!node)
        return 0.0;

    return node->get_fs_factor();
}

double priority_tree::get_fair_share_factor (const char *parent,
                                             const char *user)
{
    get_fair_share_factor (string (user), string (parent));
}

void priority_tree::add_usage (const std::string &parent,
                               const std::string &user, double usage)
{
    shared_ptr<prio_node> node;

    node = get_node (parent, user);
    if (!node)
        return;
    node->add_usage (usage);
}

void priority_tree::add_usage (const char *parent,
                               const char *user, double usage)
{
    add_usage (string (user), string (parent), usage);
}

unsigned priority_tree::get_node_count ()
{
    return node_map.size();
}

namespace {
/*
 * This class magically makes everything in 'dest' stream
 * indented as long as this class is in scope.  Once it
 * it goes out of scope and is destroyed, the indenting
 * disappears.
 */
class IndentingOStreambuf : public std::streambuf
{
    std::streambuf* myDest;
    bool myIsAtStartOfLine;
    std::string myIndent;
    std::ostream* myOwner;
protected:
    virtual int overflow( int ch )
    {
        if (myIsAtStartOfLine && ch != '\n') {
            myDest->sputn (myIndent.data(), myIndent.size());
        }
        myIsAtStartOfLine = ch == '\n';
        return myDest->sputc (ch);
    }
public:
    explicit IndentingOStreambuf (std::streambuf* dest, int indent = 2)
        : myDest (dest)
        , myIsAtStartOfLine (true)
        , myIndent (indent, ' ')
        , myOwner(NULL)
    {
    }
    explicit IndentingOStreambuf (std::ostream& dest, int indent = 2)
        : myDest (dest.rdbuf ())
        , myIsAtStartOfLine (true)
        , myIndent (indent, ' ')
        , myOwner (&dest)
    {
        myOwner->rdbuf (this);
    }
    virtual ~IndentingOStreambuf()
    {
        if ( myOwner != NULL ) {
            myOwner->rdbuf (myDest);
        }
    }
};
}

std::ostream& operator<<(std::ostream& s, priority_tree const& tree)
{
    s << *tree.root;
    return s;
}

std::ostream& operator<<(std::ostream& s, prio_node const& node)
{
    cout << left << setw(12) << node.name;
    cout << right;
    cout << setw(10) << node.shares;
    cout << setw(10) << node.usage;
    cout << setw(10) << node.fs_factor;
    cout << endl;

    for (auto &&child: node.children) {
        IndentingOStreambuf indent (s);
        cout << *child;
    }
    return s;
}

} //namespace Priority
} //namespace Flux
