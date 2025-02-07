#ifndef ABSL_BASE_INTERNAL_ERRNO_SAVER_H_
#define ABSL_BASE_INTERNAL_ERRNO_SAVER_H_

#include <cerrno>
#include "absl/base/config.h"

namespace absl {
    ABSL_NAMESPACE_BEGIN
        namespace base_internal {

        // `ErrnoSaver` captures the value of `errno` upon construction and restores it
        // upon deletion. It handles platform-specific behavior safely.
        class ErrnoSaver {
        public:
            ErrnoSaver() : saved_errno_(get_errno()) {}
            ~ErrnoSaver() { set_errno(saved_errno_); }
            int operator()() const { return saved_errno_; }

        private:
            int saved_errno_;

            // Platform-specific accessors for `errno`
            static int get_errno() {
#ifdef _MSC_VER
                int temp;
                _get_errno(&temp);
                return temp;
#else
                return errno;
#endif
            }

            static void set_errno(int value) {
#ifdef _MSC_VER
                _set_errno(value);
#else
                errno = value;
#endif
            }
        };

    }  // namespace base_internal
    ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_INTERNAL_ERRNO_SAVER_H_
