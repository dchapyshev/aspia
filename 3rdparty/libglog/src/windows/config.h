/* src/config.h.in.  Generated from configure.ac by autoheader.  */

/* Namespace for Google classes */
#define GOOGLE_NAMESPACE google

/* Stops putting the code inside the Google namespace */
#define _END_GOOGLE_NAMESPACE_ }

/* Puts following code inside the Google namespace */
#define _START_GOOGLE_NAMESPACE_ namespace google {

/* Define if you have the `snprintf' function */
#define HAVE_SNPRINTF

/* define if you have google gflags library */
#define HAVE_LIB_GFLAGS

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H

/* define if the compiler implements namespaces */
#define HAVE_NAMESPACES

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* define if the compiler supports using expression for operator */
#define HAVE_USING_OPERATOR

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 4

/* the namespace where STL code like vector<> is defined */
#define STL_NAMESPACE std

/* Always the empty-string on non-windows systems. On windows, should be
   "__declspec(dllexport)". This way, when we compile the dll, we export our
   functions/classes. It's safe to define this here because config.h is only
   used internally, to compile the DLL, and every DLL source file #includes
   "config.h" before anything else. */
#ifndef GOOGLE_GLOG_DLL_DECL
# define GOOGLE_GLOG_IS_A_DLL  1   /* not set if you're statically linking */
# define GOOGLE_GLOG_DLL_DECL  __declspec(dllexport)
# define GOOGLE_GLOG_DLL_DECL_FOR_UNITTESTS  __declspec(dllimport)
#endif
