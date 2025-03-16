#ifndef GLOG_EXPORT_H
#define GLOG_EXPORT_H

#if defined(_MSC_VER)
#  if defined(GLOG_STATIC_DEFINES)
#    define GLOG_EXPORT
#    define GLOG_NO_EXPORT
#  else
#    if defined(glog_EXPORTS)
#      define GLOG_EXPORT __declspec(dllexport)
#      define GLOG_NO_EXPORT
#    else
#      define GLOG_EXPORT __declspec(dllimport)
#      define GLOG_NO_EXPORT
#    endif
#  endif
#  define GLOG_DEPRECATED __declspec(deprecated)
#  define GLOG_NORETURN __declspec(noreturn)
#else
#  define GLOG_EXPORT __attribute__((visibility("default")))
#  define GLOG_NO_EXPORT __attribute__((visibility("hidden")))
#  define GLOG_DEPRECATED __attribute__((deprecated))
#  define GLOG_NORETURN __attribute__((noreturn))
#endif

#endif /* GLOG_EXPORT_H */
