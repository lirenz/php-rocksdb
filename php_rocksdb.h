#ifndef PHP_ROCKSDB_H
#define PHP_ROCKSDB_H

extern zend_module_entry rocksdb_module_entry;
#define phpext_rocksdb_ptr &rocksdb_module_entry

#define PHP_ROCKSDB_VERSION "0.1"

#ifdef PHP_WIN32
# define PHP_ROCKSDB_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
# define PHP_ROCKSDB_API __attribute__ ((visibility("default")))
#else
# define PHP_ROCKSDB_API
#endif

PHP_MINIT_FUNCTION(rocksdb);
PHP_MSHUTDOWN_FUNCTION(rocksdb);
PHP_MINFO_FUNCTION(rocksdb);

#endif /* PHP_ROCKSDB_H */

