dnl $Id$
dnl config.m4 for extension rocksdb

PHP_ARG_WITH(rocksdb, for rocksdb support,
[  --with-rocksdb[=Path]             Include rocksdb support])

if test "$PHP_ROCKSDB" != "no"; then

  # --with-rocksdb -> check with-path
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="include/rocksdb/c.h"
  SEARCH_LIB="librocksdb"

  dnl search rocksdb
  AC_MSG_CHECKING([for rocksdb location])
  for i in $PHP_ROCKSDB $SEARCH_PATH ; do
    if test -r $i/$SEARCH_FOR; then
      ROCKSDB_INCLUDE_DIR=$i
      AC_MSG_RESULT(rocksdb headers found in $i)
    fi

    if test -r $i/$PHP_LIBDIR/$SEARCH_LIB.a || test -r $i/$PHP_LIBDIR/$SEARCH_LIB.$SHLIB_SUFFIX_NAME; then
      ROCKSDB_LIB_DIR=$i/$PHP_LIBDIR
      AC_MSG_RESULT(rocksdb lib found in $i/lib)
    fi

    dnl from RocksDB build dir
    if test -r $i/$SEARCH_LIB.a || test -r $i/$SEARCH_LIB.$SHLIB_SUFFIX_NAME; then
      ROCKSDB_LIB_DIR=$i
      AC_MSG_RESULT(rocksdb lib found in $i)
    fi

    if test -z "$ROCKSDB_LIB_DIR"; then
      for j in "lib/x86_64-linux-gnu" "lib/x86_64-linux-gnu"; do
        echo find "--$i/$j"
        if test -r $i/$j/$SEARCH_LIB.a || test -r $i/$j/$SEARCH_LIB.$SHLIB_SUFFIX_NAME; then
          ROCKSDB_LIB_DIR=$i/$j
          AC_MSG_RESULT(rocksdb lib found in $i/$j)
        fi
      done
    fi
  done

  if test -z "$ROCKSDB_INCLUDE_DIR" || test -z "$ROCKSDB_LIB_DIR"; then
    AC_MSG_RESULT([rocksdb not found])
    AC_MSG_ERROR([Please reinstall the rocksdb distribution])
  fi

  # --with-rocksdb -> add include path
  PHP_ADD_INCLUDE($ROCKSDB_INCLUDE_DIR/include)

  # --with-rocksdb -> check for lib and symbol presence
  LIBNAME=rocksdb
  PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $ROCKSDB_LIB_DIR, ROCKSDB_SHARED_LIBADD)

  PHP_SUBST(ROCKSDB_SHARED_LIBADD)

  PHP_NEW_EXTENSION(rocksdb, php_rocksdb.c, $ext_shared)
fi

