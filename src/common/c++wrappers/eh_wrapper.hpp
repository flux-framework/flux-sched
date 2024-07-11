/*****************************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, LICENSE)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\*****************************************************************************/

#include <cerrno>
#include <cstring>
#include <utility>
#include <exception>
#include <stdexcept>

namespace Flux {
namespace cplusplus_wrappers {

/*! Exception handling wrapper functor class.
 *  Use perfect forwarding to wrap the target functor of many different types
 *  in order to add an exception handling semantics: mapping certain C++
 *  exception classes to errnos as they are caught.
 */
class eh_wrapper_t {
   public:
    /*! Templated operator() method: An object of eh_wrapper_t *can invoke
     *  its wrapping function using the function invocation convention.
     *  "functor_object()". Different numbers and types of function parameters
     *  are handled with template parameter pack/unpack (i.e. "..." keyword).
     *  Transparent forwarding of function arguments is handled with
     *  the so-called "universal" or "forwarding" reference (Args&&) in
     *  combination with the std::forward method.  Introductory examples
     *  for perfect forwarding can be found in the following:
     *  https://www.modernescpp.com/index.php/perfect-forwarding
     *
     * \param f        functor (function, function pointer or function object)
     * \param args     variadic arguments list that is transparently
     *                 forwarded to f.
     */
    template<typename Functor, typename... Args>
    auto operator() (Functor f, Args &&...args) -> decltype (f (std::forward<Args> (args)...))
    {
        int rc = -1;
        exception_raised = false;
        memset (err_message, '\0', 4096);
        try {
            return f (std::forward<Args> (args)...);
        } catch (std::bad_alloc &e) {
            errno = ENOMEM;
            snprintf (err_message, 4096, "ENOMEM: %s", e.what ());
        } catch (std::length_error &e) {
            errno = ERANGE;
            snprintf (err_message, 4096, "ERANGE: %s", e.what ());
        } catch (std::out_of_range &e) {
            errno = ENOENT;
            snprintf (err_message, 4096, "ENOENT: %s", e.what ());
        } catch (std::exception &e) {
            errno = ENOSYS;
            snprintf (err_message, 4096, "ENOSYS: %s", e.what ());
        } catch (...) {
            errno = ENOSYS;
            snprintf (err_message, 4096, "ENOSYS: Caught unknown exception");
        }
        exception_raised = true;
        return return_<decltype (f (std::forward<Args> (args)...))> (rc);
    }

    /*! Has a C++ exception been raised from the last invocation of operator()?
     */
    bool bad ()
    {
        return exception_raised;
    }

    /*! Return the exception error message associated from the last invocation
     *  of operator().
     */
    const char *get_err_message ()
    {
        return exception_raised ? err_message : NULL;
    }

   private:
    template<typename T>
    T return_ (int i)
    {
        return T (i);
    }

    bool exception_raised = false;
    char err_message[4096];  // to avoid a bad_alloc exception with std::string
};

template<>
inline void eh_wrapper_t::return_<void> (int i)
{
    return;
}

}  // namespace cplusplus_wrappers
}  // namespace Flux

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
