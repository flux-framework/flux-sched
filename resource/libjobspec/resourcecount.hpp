/*****************************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#ifndef FLUX_JOBSPEC_RESOURCECOUNT_HPP
#define FLUX_JOBSPEC_RESOURCECOUNT_HPP

extern "C" {
#include <flux/resourcecount.h>
}

#include <cstdlib>
#include <memory>
#include <ostream>
#include <string>

namespace Flux {
namespace Jobspec {

/* RAII wrapper around an RFC 14 resource count from libresourcecount.
 *
 * The underlying struct resourcecount is immutable after creation, so copies
 * of this class share it.  A default constructed ResourceCount does not refer
 * to a count and all accessors behave as if the count were NULL.
 */
class ResourceCount {
   public:
    ResourceCount () = default;

    /* Take ownership of 'count', which may be NULL. */
    explicit ResourceCount (struct resourcecount *count) : m_count (count, resourcecount_destroy)
    {
    }

    /* Return the first valid value in the count, or RESOURCECOUNT_INVALID_VALUE. */
    unsigned int first () const
    {
        return resourcecount_first (m_count.get ());
    }

    /* Return the next valid value following 'value' in the count, or
     * RESOURCECOUNT_INVALID_VALUE if 'value' is the last valid value.
     * Only valid values produced by first()/next() should be passed in.
     */
    unsigned int next (unsigned int value) const
    {
        return resourcecount_next (m_count.get (), value);
    }

    /* Return the largest valid value in the count, or RESOURCECOUNT_INVALID_VALUE. */
    unsigned int last () const
    {
        return resourcecount_last (m_count.get ());
    }

    /* Return true if the count is a simple integer. */
    bool is_integer () const
    {
        return resourcecount_is_integer (m_count.get ());
    }

    /* Return true if the count is an unbounded range. */
    bool is_unbounded () const
    {
        return resourcecount_is_unbounded (m_count.get ());
    }

    /* Encode the count as a string.  An empty string is returned if the
     * count is NULL or cannot be encoded.
     */
    std::string encode (int flags = RESOURCECOUNT_FLAG_SHORT) const
    {
        char *s = resourcecount_encode (m_count.get (), flags);
        std::string result;
        if (s) {
            result = s;
            free (s);
        }
        return result;
    }

   private:
    std::shared_ptr<struct resourcecount> m_count;
};

std::ostream &operator<< (std::ostream &s, ResourceCount const &count);

}  // namespace Jobspec
}  // namespace Flux

#endif  // FLUX_JOBSPEC_RESOURCECOUNT_HPP

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
