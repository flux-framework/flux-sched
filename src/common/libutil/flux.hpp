extern "C" {
#include <flux/core.h>
}

#include "json.hpp"

#include <memory>
#include <system_error>

namespace flux
{
struct msg_handler_wrapper {
    msg_handler_wrapper (flux_t* h,
                         const char* topic_glob,
                         flux_msg_handler_f cb,
                         void* arg,
                         int typemask = FLUX_MSGTYPE_REQUEST,
                         uint32_t rolemask = 0,
                         uint32_t matchtag = FLUX_MATCHTAG_NONE)
        : cb_ (cb), arg_original_ (arg)
    {
        flux_match m = {typemask, matchtag, const_cast<char*> (topic_glob)};
        handler_ = flux_msg_handler_create (h, m, msg_handler_bounce, this);
        if (!handler_) {
            throw std::system_error (errno,
                                     std::generic_category (),
                                     "flux_msg_handler_create failed");
        }
        if (rolemask != 0) {
            flux_msg_handler_allow_rolemask (handler_, rolemask);
        }
        flux_msg_handler_start (handler_);
    }
    ~msg_handler_wrapper ()
    {
        if (handler_) {
            flux_msg_handler_destroy (handler_);
        }
    }

    flux_msg_handler_t* handler_;
    flux_msg_handler_f cb_;
    void* arg_original_;

   private:
    static void msg_handler_bounce (flux_t* h,
                                    flux_msg_handler_t* mh,
                                    const flux_msg_t* msg,
                                    void* arg)
    {
        msg_handler_wrapper* handler = reinterpret_cast<msg_handler_wrapper*> (arg);
        try {
            handler->cb_ (h, mh, msg, handler->arg_original_);
        } catch (const std::bad_alloc& e) {
            std::string errs =
                std::string ("Allocation failed in wrapped msg_handler: ") + e.what ();
            if (flux_respond_error (h, msg, ENOMEM, errs.c_str ()))
                flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
        } catch (const std::system_error& e) {
            std::string errs =
                std::string ("system_error in wrapped msg_handler with meaning: ")
                + e.what ();
            if (flux_respond_error (h, msg, e.code ().value (), errs.c_str ()))
                flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
        } catch (const std::exception& e) {
            std::string errs =
                std::string ("std::exception caught in wrapped msg_handler: ")
                + e.what ();
            if (flux_respond_error (h, msg, ENOMEM, errs.c_str ()))
                flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
        } catch (...) {
            flux_log_error (h, "unknown exception thrown in wrapped msg_handler");
            if (flux_respond_error (h,
                                    msg,
                                    errno,
                                    "unknown exception thrown in wrapped msg_handler")
                < 0)
                flux_log_error (h, "%s: flux_respond_error", __FUNCTION__);
        }
    }
};

nlohmann::json request_to_json (const flux_msg_t* msg)
{
    const char* data = nullptr;
    if (flux_request_decode (msg, NULL, &data) < 0 || data == nullptr) {
        throw std::system_error (errno,
                                 std::generic_category (),
                                 "flux_request_decode failed");
    }
    return nlohmann::json::parse (data);
}

};  // namespace flux
