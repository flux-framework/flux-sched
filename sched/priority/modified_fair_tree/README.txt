Modified Fair Tree
------------------

The code in this directory is split into two major parts:

  priority_external_api.[ch]pp
  modified_fair_tree.[ch]pp

This a work in progress, rather than a complete plugin.  The idea was
to isolate the bulk of the fair-share algorithm to the
modified_fair_tree.[ch]pp files, while the external API of the
priority plugin and the rest of the simpler multi-factor components
remain in priority_external_api.[ch]pp.  In the future this could
be further modularized to either allow a selection of different
fair share algorithms within one multi-factor plugin, or perhaps
alternate fair share algorithsm would simply require separate
priority plugins (perhaps with code sharing between the plugins
to avoid duplication).

The priority plugin under way in this directory aims to implement a
multi-factor prioritization algorithm.  Most of the various factors
are simple (time in the queue, job size), and can fairly easily be
fleshed out in the future.  Most of the complexity lies in the
"fair share" factor.

The fair share factor presented in this directory is a new algorithm
that was developed by Don Lipari.  It is based upon SLURM's "Fair Tree"
fairness algorithm, and so it is currently named the "Modified Fair
Tree" algorithm.

To learn how SLURM's Fair Tree works, some resources are:

  https://slurm.schedmd.com/fair_tree.html
  https://slurm.schedmd.com/SUG14/fair_tree.pdf

And a resource about SLURM's multifactor priority plugin (upon which the
multifactor portions of this plugin are based) see:

  https://slurm.schedmd.com/priority_multifactor.html

Both Fair Tree and Modified Fair Tree require a configuration in the
form of a tree of accounts and users, each with relative ranking value.
For now, we assume that this configuration information is stored in
SLURM's accounting database, and one way or another this plugin will
either retrieve or be fed the output of the command:

  sacctmgr -n -p show assoc cluster=<cluster name> format=account,share,parentn,user

(A sanitized example of the output is available in test/sacct_sample)

Some details about the Modified Fair Tree algorithm are in a code
comment just above prio_node::calc_fs_factor() in modified_fair_tree.cpp.

SLURM's Fair Tree alrgorithm essentially performs a depth-first walk
of the tree of accounts and users calculating each entity's "fair share factor".
Every time the fair share factors of all of the children of a single parent
have had their factors calculated, the children are are resorted from highest to
lowest priority.  When the entire tree walk is complete, we are left with
an completely ordered tree based on current fair share factor.  The Fair Tree
algorithm then assigned an integer value from N to 1 to each of the users
in the tree.  This becomes the Fair Tree factor for each user.

The Modified Fair Tree algorithm uses the same type of tree and tree walk.
However the initial "fair share factor" calulated in the tree walk is slightly
different (more similar to SLURM's older Fair Share algoritm, which is not to
confused with Fair Tree).  The change is designed to constrain the fair share
factor to a range of 0.0 to 1.0.  Also, once the tree is sorted, rather than
assinging simple integer values to each user,

My concern with the Modified Fair Tree algorithm is that is may preserve
some of corner case problems that plagued SLURM's original Fair Share algorithm,
and those problems are largly what inspired the creation of SLURM's Fair Tree
algorithm.

Further, one of the major goals of Modified Fair Tree seems to be to make
something that is algorithmically easier to explain to end users.  However,
since much of the tree-walk-and-sort algorithm is shared by Modified-Fair-Tree,
and the major differences are a slightly more complex fair share factor
algorithm, plus an arguably more complex scaling methodology for the final
fair share value (versus Fair Tree's simple integer assignment), perhaps an
arugment can be made that this approach needs further consideration.

Also, LLNL is moving to the use of Fair Tree under SLURM on all of its clusters.

For these reasons, I would recommend that we implement an equivalent Fair
Tree algorithm as Flux's first fairshare algorithm rather than Modified
Fair Tree.  Other fairshare algorithms can always be explored further in the
future.
