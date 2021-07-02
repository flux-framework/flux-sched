/*****************************************************************************\
 * Copyright 2019 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include "resource/readers/resource_reader_base.hpp"
#include "resource/store/resource_graph_store.hpp"

extern "C" {
#if HAVE_CONFIG_H
#include "config.h"
#endif
}

using namespace Flux::resource_model;


/********************************************************************************
 *                                                                              *
 *                         Private Base Reader API                              *
 *                                                                              *
 ********************************************************************************/

bool resource_reader_base_t::in_allowlist (const std::string &resource)
{
    return allowlist.empty ()
           || (allowlist.find (resource) != allowlist.end ());
}


/********************************************************************************
 *                                                                              *
 *                         Public Base Reader API                               *
 *                                                                              *
 ********************************************************************************/

resource_reader_base_t::~resource_reader_base_t ()
{

}

int resource_reader_base_t::set_allowlist (const std::string &csl)
{
    if (csl == "")
        return 0;

    int rc = -1;
    size_t pos = 0;
    std::string csl_copy = csl;
    std::string sep = ",";

    try {
        while ((pos = csl_copy.find (sep)) != std::string::npos) {
            std::string resource = csl_copy.substr (0, pos);
            if (resource != "")
                allowlist.insert (resource);
            csl_copy.erase (0, pos + sep.length ());
        }
        if (csl_copy != "")
            allowlist.insert (csl_copy);
        errno = EINVAL;
        rc = allowlist.empty ()? -1 : 0;
    } catch (std::out_of_range &e) {
        errno = EINVAL;
        rc = -1;
    } catch (std::bad_alloc &e) {
        errno = ENOMEM;
        rc = -1;
    }

    return rc;
}

bool resource_reader_base_t::is_allowlist_set ()
{
    return !allowlist.empty ();
}

const std::string &resource_reader_base_t::err_message () const
{
    return m_err_msg;
}

void resource_reader_base_t::clear_err_message ()
{
    m_err_msg = "";
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
