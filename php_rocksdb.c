/*
  +----------------------------------------------------------------------+
  | PHP RocksDB Extension                                                |
  +----------------------------------------------------------------------+
  | This source file is subject to the MIT license.                      |
  +----------------------------------------------------------------------+
  | Author: Lawrence Zhou                                                |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include <rocksdb/c.h>
#include "php_rocksdb.h"

/* Macros */
#define ROCKSDB_CHECK_ERROR(err) \
  if ((err) != NULL) { \
    zend_throw_exception(php_rocksdb_exception_ce, err, 0); \
    rocksdb_free(err); \
    return; \
  }

/* Handler declarations */
zend_object_handlers rocksdb_object_handlers;
zend_object_handlers rocksdb_write_batch_object_handlers;
zend_object_handlers rocksdb_iterator_object_handlers;

/* Class entries */
zend_class_entry *php_rocksdb_ce;
zend_class_entry *php_rocksdb_write_batch_ce;
zend_class_entry *php_rocksdb_iterator_ce;
zend_class_entry *php_rocksdb_exception_ce;

/* ---------------------- Internal Structures ---------------------- */

/* RocksDB object */
typedef struct _rocksdb_object {
  rocksdb_t *db;
  rocksdb_options_t *options;
  rocksdb_readoptions_t *read_options;
  rocksdb_writeoptions_t *write_options;
  zend_object std;
} rocksdb_object;

static inline rocksdb_object *php_rocksdb_object_from_zobj(zend_object *obj) {
  return (rocksdb_object *)((char*)(obj) - XtOffsetOf(rocksdb_object, std));
}

/* WriteBatch object */
typedef struct _rocksdb_write_batch_object {
  rocksdb_writebatch_t *batch;
  zend_object std;
} rocksdb_write_batch_object;

static inline rocksdb_write_batch_object *
php_rocksdb_write_batch_object_from_zobj(zend_object *obj) {
  return (rocksdb_write_batch_object *)((char*)(obj)
    - XtOffsetOf(rocksdb_write_batch_object, std));
}

/* Iterator object */
typedef struct _rocksdb_iterator_object {
  rocksdb_iterator_t *iter;
  rocksdb_object *db_obj;
  char *prefix;
  size_t prefix_len;
  zend_object std;
} rocksdb_iterator_object;

static inline rocksdb_iterator_object *
php_rocksdb_iterator_object_from_zobj(zend_object *obj) {
  return (rocksdb_iterator_object *)((char*)(obj)
    - XtOffsetOf(rocksdb_iterator_object, std));
}

/* ---------------------- Free / Create Methods ---------------------- */

static void php_rocksdb_object_free(zend_object *object) {
  rocksdb_object *obj = php_rocksdb_object_from_zobj(object);
  if (obj->db) {
    rocksdb_close(obj->db);
  }
  if (obj->options) {
    rocksdb_options_destroy(obj->options);
  }
  if (obj->read_options) {
    rocksdb_readoptions_destroy(obj->read_options);
  }
  if (obj->write_options) {
    rocksdb_writeoptions_destroy(obj->write_options);
  }
  zend_object_std_dtor(&obj->std);
}

static zend_object *php_rocksdb_object_new(zend_class_entry *ce) {
  rocksdb_object *obj = ecalloc(1,
    sizeof(rocksdb_object) + zend_object_properties_size(ce));
  zend_object_std_init(&obj->std, ce);
  object_properties_init(&obj->std, ce);
  obj->std.handlers = &rocksdb_object_handlers;
  return &obj->std;
}

static void php_rocksdb_write_batch_object_free(zend_object *object) {
  rocksdb_write_batch_object *obj =
    php_rocksdb_write_batch_object_from_zobj(object);
  if (obj->batch) {
    rocksdb_writebatch_destroy(obj->batch);
  }
  zend_object_std_dtor(&obj->std);
}

static zend_object *php_rocksdb_write_batch_object_new(zend_class_entry *ce) {
  rocksdb_write_batch_object *obj = ecalloc(1,
    sizeof(rocksdb_write_batch_object) + zend_object_properties_size(ce));
  zend_object_std_init(&obj->std, ce);
  object_properties_init(&obj->std, ce);
  obj->std.handlers = &rocksdb_write_batch_object_handlers;
  return &obj->std;
}

static void php_rocksdb_iterator_object_free(zend_object *object) {
  rocksdb_iterator_object *obj =
    php_rocksdb_iterator_object_from_zobj(object);
  if (obj->iter) {
    rocksdb_iter_destroy(obj->iter);
  }
  if (obj->prefix) {
    efree(obj->prefix);
  }
  zend_object_std_dtor(&obj->std);
}

static zend_object *php_rocksdb_iterator_object_new(zend_class_entry *ce) {
  rocksdb_iterator_object *obj = ecalloc(1,
    sizeof(rocksdb_iterator_object) + zend_object_properties_size(ce));
  zend_object_std_init(&obj->std, ce);
  object_properties_init(&obj->std, ce);
  obj->std.handlers = &rocksdb_iterator_object_handlers;
  return &obj->std;
}

/* ---------------------- Arginfo Declarations ---------------------- */

/* RocksDB::compactRange(?string $begin = null, ?string $end = null): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_compactRange, 0, 0, 0)
  ZEND_ARG_TYPE_INFO(0, begin, IS_STRING, 1)
  ZEND_ARG_TYPE_INFO(0, end,   IS_STRING, 1)
ZEND_END_ARG_INFO()

/* RocksDB::__construct(string $path, array $options = null) */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb___construct, 0, 0, 1)
  ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
  ZEND_ARG_ARRAY_INFO(0, options, 1)
ZEND_END_ARG_INFO()

/* RocksDB::get(string $key): string|false */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_get, 0, 0, 1)
  ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* RocksDB::multiGet(array $keys): array */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_multiGet, 0, 0, 1)
  ZEND_ARG_ARRAY_INFO(0, keys, 0)
ZEND_END_ARG_INFO()

/* RocksDB::put(string $key, string $value): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_put, 0, 0, 2)
  ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
  ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* RocksDB::delete(string $key): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_delete, 0, 0, 1)
  ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* RocksDB::write(RocksDBWriteBatch $batch, array $writeOptions = null): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_write, 0, 0, 1)
  ZEND_ARG_OBJ_INFO(0, batch, RocksDBWriteBatch, 0)
  ZEND_ARG_ARRAY_INFO(0, writeOptions, 1)
ZEND_END_ARG_INFO()

/* RocksDB::getIterator(): RocksDBIterator */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_getIterator, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDB::prefixSearch(string $prefix): RocksDBIterator */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_prefixSearch, 0, 0, 1)
  ZEND_ARG_TYPE_INFO(0, prefix, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* RocksDBWriteBatch::__construct() */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_writebatch___construct, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBWriteBatch::put(string $key, string $value): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_writebatch_put, 0, 0, 2)
  ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
  ZEND_ARG_TYPE_INFO(0, value, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* RocksDBWriteBatch::delete(string $key): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_writebatch_delete, 0, 0, 1)
  ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
ZEND_END_ARG_INFO()

/* RocksDBWriteBatch::clear(): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_writebatch_clear, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBIterator::__construct(RocksDB $db, string $prefix = null) */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator___construct, 0, 0, 1)
  ZEND_ARG_OBJ_INFO(0, db, RocksDB, 0)
  ZEND_ARG_TYPE_INFO(0, prefix, IS_STRING, 1)
ZEND_END_ARG_INFO()

/* RocksDBIterator::valid(): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator_valid, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBIterator::key(): string */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator_key, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBIterator::current(): string */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator_current, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBIterator::next(): void */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator_next, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBIterator::rewind(): void */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator_rewind, 0, 0, 0)
ZEND_END_ARG_INFO()

/* RocksDBIterator::destroy(): bool */
ZEND_BEGIN_ARG_INFO_EX(arginfo_rocksdb_iterator_destroy, 0, 0, 0)
ZEND_END_ARG_INFO()

/* ---------------------- Method Implementations ---------------------- */

/* --- RocksDB::__construct(...) with advanced options --- */
PHP_METHOD(RocksDB, __construct)
{
  char *path;
  size_t path_len;
  zval *options_zv = NULL;
  char *err = NULL;
  rocksdb_object *obj;
  zend_bool read_only = 0;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|a",
      &path, &path_len, &options_zv) == FAILURE) {
    return;
  }

  obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));

  obj->options = rocksdb_options_create();
  rocksdb_options_set_create_if_missing(obj->options, 1);

  rocksdb_block_based_table_options_t *table_opts = rocksdb_block_based_options_create();

  if (options_zv && Z_TYPE_P(options_zv) == IS_ARRAY) {
    zval *val;
    HashTable *ht = Z_ARRVAL_P(options_zv);

    if ((val = zend_hash_str_find(ht, "read_only", sizeof("read_only") - 1)) != NULL) {
      if (zend_is_true(val)) {
        read_only = 1;
        rocksdb_options_set_create_if_missing(obj->options, 0);
      }
    }

    if ((val = zend_hash_str_find(ht, "create_if_missing", sizeof("create_if_missing") - 1)) != NULL) {
      rocksdb_options_set_create_if_missing(obj->options, zend_is_true(val));
    }
    if ((val = zend_hash_str_find(ht, "write_buffer_size", sizeof("write_buffer_size") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_write_buffer_size(obj->options, (size_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "max_write_buffer_number", sizeof("max_write_buffer_number") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_max_write_buffer_number(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "min_write_buffer_number_to_merge", sizeof("min_write_buffer_number_to_merge") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_min_write_buffer_number_to_merge(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "max_background_jobs", sizeof("max_background_jobs") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_max_background_jobs(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "max_background_compactions", sizeof("max_background_compactions") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_max_background_compactions(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "max_open_files", sizeof("max_open_files") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_max_open_files(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "target_file_size_base", sizeof("target_file_size_base") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_target_file_size_base(obj->options, (uint64_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "max_bytes_for_level_base", sizeof("max_bytes_for_level_base") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_max_bytes_for_level_base(obj->options, (uint64_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "compression", sizeof("compression") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_compression(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "bottommost_compression", sizeof("bottommost_compression") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_bottommost_compression(obj->options, (int)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "bloom_locality", sizeof("bloom_locality") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_bloom_locality(obj->options, (uint32_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "memtable_prefix_bloom_size_ratio", sizeof("memtable_prefix_bloom_size_ratio") - 1)) != NULL) {
      convert_to_double(val);
      rocksdb_options_set_memtable_prefix_bloom_size_ratio(obj->options, Z_DVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "block_size", sizeof("block_size") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_block_based_options_set_block_size(table_opts, (size_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "block_restart_interval", sizeof("block_restart_interval") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_block_based_options_set_block_restart_interval(table_opts, (int)Z_LVAL_P(val));
    }

    /* Additions: bulk/online control knobs */
    if ((val = zend_hash_str_find(ht, "disable_auto_compactions", sizeof("disable_auto_compactions") - 1)) != NULL) {
      rocksdb_options_set_disable_auto_compactions(obj->options, zend_is_true(val));
    }
    if ((val = zend_hash_str_find(ht, "level_compaction_dynamic_level_bytes", sizeof("level_compaction_dynamic_level_bytes") - 1)) != NULL) {
      rocksdb_options_set_level_compaction_dynamic_level_bytes(obj->options, zend_is_true(val));
    }
    if ((val = zend_hash_str_find(ht, "max_subcompactions", sizeof("max_subcompactions") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_max_subcompactions(obj->options, (uint32_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "bytes_per_sync", sizeof("bytes_per_sync") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_bytes_per_sync(obj->options, (uint64_t)Z_LVAL_P(val));
    }
    if ((val = zend_hash_str_find(ht, "wal_bytes_per_sync", sizeof("wal_bytes_per_sync") - 1)) != NULL) {
      convert_to_long(val);
      rocksdb_options_set_wal_bytes_per_sync(obj->options, (uint64_t)Z_LVAL_P(val));
    }
  }

  rocksdb_options_set_block_based_table_factory(obj->options, table_opts);
  rocksdb_block_based_options_destroy(table_opts);

  obj->read_options = rocksdb_readoptions_create();
  obj->write_options = rocksdb_writeoptions_create();

  if (read_only) {
    obj->db = rocksdb_open_for_read_only(obj->options, path,
      /* error_if_log_file_exist */ 0, &err);
  } else {
    obj->db = rocksdb_open(obj->options, path, &err);
  }
  ROCKSDB_CHECK_ERROR(err);
}

/* public function RocksDB::compactRange(?string $begin = null, ?string $end = null): bool */
PHP_METHOD(RocksDB, compactRange)
{
  char *begin = NULL, *end = NULL;
  size_t blen = 0, elen = 0;
  if (zend_parse_parameters(ZEND_NUM_ARGS(), "|s!s!", &begin, &blen, &end, &elen) == FAILURE) {
    return;
  }
  rocksdb_object *obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));
  /* Correct C API call (no read_options parameter) */
  rocksdb_compact_range(obj->db, begin, blen, end, elen);
  RETURN_TRUE;
}

/* public function RocksDB::get(string $key): string|false */
PHP_METHOD(RocksDB, get)
{
  char *key;
  size_t key_len;
  char *err = NULL;
  size_t val_len;
  rocksdb_object *obj;
  char *val;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &key_len) == FAILURE) {
    return;
  }
  obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));

  val = rocksdb_get(obj->db, obj->read_options, key, key_len, &val_len, &err);
  ROCKSDB_CHECK_ERROR(err);

  if (!val) {
    RETURN_NULL();
  }
  RETVAL_STRINGL(val, val_len);
  rocksdb_free(val);
}

/* public function RocksDB::multiGet(array $keys): array */
PHP_METHOD(RocksDB, multiGet)
{
  zval *keys_zv; HashTable *ht;
  size_t n, i = 0;
  const char **c_keys; size_t *c_key_lens, *c_val_lens;
  char **c_vals, **c_errs;
  rocksdb_object *obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &keys_zv) == FAILURE) return;

  ht = Z_ARRVAL_P(keys_zv);
  n  = zend_hash_num_elements(ht);
  if (!n) { array_init(return_value); return; }

  c_keys     = emalloc(sizeof(char*)  * n);
  c_key_lens = emalloc(sizeof(size_t) * n);
  c_vals     = ecalloc(n, sizeof(char*));
  c_val_lens = emalloc(sizeof(size_t) * n);
  c_errs     = ecalloc(n, sizeof(char*));

  zval *zv;
  ZEND_HASH_FOREACH_VAL(ht, zv) {
    convert_to_string(zv);
    c_keys[i]     = Z_STRVAL_P(zv);
    c_key_lens[i] = Z_STRLEN_P(zv);
    i++;
  } ZEND_HASH_FOREACH_END();

  obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));
  rocksdb_multi_get(obj->db, obj->read_options,
                    n, c_keys, c_key_lens,
                    c_vals, c_val_lens, c_errs);

  array_init_size(return_value, n);
  for (i = 0; i < n; i++) {
    if (c_errs[i]) {
      add_next_index_bool(return_value, 0);
      rocksdb_free(c_errs[i]);
    } else if (c_vals[i]) {
      add_next_index_stringl(return_value, c_vals[i], c_val_lens[i]);
      rocksdb_free(c_vals[i]);
    } else {
      add_next_index_null(return_value);
    }
  }

  efree(c_keys);
  efree(c_key_lens);
  efree(c_vals);
  efree(c_val_lens);
  efree(c_errs);
}

/* public function RocksDB::put(string $key, string $value): bool */
PHP_METHOD(RocksDB, put)
{
  char *key, *value;
  size_t key_len, value_len;
  char *err = NULL;
  rocksdb_object *obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss", &key, &key_len, &value, &value_len) == FAILURE) {
    return;
  }
  obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));

  rocksdb_put(obj->db, obj->write_options, key, key_len, value, value_len, &err);
  ROCKSDB_CHECK_ERROR(err);

  RETURN_TRUE;
}

/* public function RocksDB::delete(string $key): bool */
PHP_METHOD(RocksDB, delete)
{
  char *key;
  size_t key_len;
  char *err = NULL;
  rocksdb_object *obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &key_len) == FAILURE) {
    return;
  }
  obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));

  rocksdb_delete(obj->db, obj->write_options, key, key_len, &err);
  ROCKSDB_CHECK_ERROR(err);

  RETURN_TRUE;
}

/* public function RocksDB::write(RocksDBWriteBatch $batch, array $writeOptions = null): bool */
PHP_METHOD(RocksDB, write)
{
  zval *batch_zv;
  zval *writeoptions_zv = NULL;
  char *err = NULL;
  rocksdb_object *obj;
  rocksdb_write_batch_object *batch_obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|a!",
      &batch_zv, php_rocksdb_write_batch_ce, &writeoptions_zv) == FAILURE) {
    return;
  }

  obj = php_rocksdb_object_from_zobj(Z_OBJ_P(getThis()));
  batch_obj = php_rocksdb_write_batch_object_from_zobj(Z_OBJ_P(batch_zv));

  rocksdb_writeoptions_t *wo = rocksdb_writeoptions_create();

  if (writeoptions_zv && Z_TYPE_P(writeoptions_zv) == IS_ARRAY) {
    zval *val;
    HashTable *ht = Z_ARRVAL_P(writeoptions_zv);

    if ((val = zend_hash_str_find(ht, "disable_wal", sizeof("disable_wal") - 1)) != NULL) {
      if (zend_is_true(val)) {
        rocksdb_writeoptions_disable_WAL(wo, 1);
      }
    }
    if ((val = zend_hash_str_find(ht, "sync", sizeof("sync") - 1)) != NULL) {
      if (zend_is_true(val)) {
        rocksdb_writeoptions_set_sync(wo, 1);
      } else {
        rocksdb_writeoptions_set_sync(wo, 0);
      }
    }
  }

  rocksdb_write(obj->db, wo, batch_obj->batch, &err);
  rocksdb_writeoptions_destroy(wo);

  ROCKSDB_CHECK_ERROR(err);

  RETURN_TRUE;
}

/* public function RocksDB::getIterator(): RocksDBIterator */
PHP_METHOD(RocksDB, getIterator)
{
  object_init_ex(return_value, php_rocksdb_iterator_ce);

  {
    zval db_obj, fname, retval;
    zval params[1];
    ZVAL_COPY_VALUE(&db_obj, getThis());
    ZVAL_STRING(&fname, "__construct");
    ZVAL_COPY_VALUE(&params[0], &db_obj);

    call_user_function(NULL, return_value, &fname, &retval, 1, params);
    zval_dtor(&fname);
    zval_ptr_dtor(&retval);
  }
}

/* public function RocksDB::prefixSearch(string $prefix): RocksDBIterator */
PHP_METHOD(RocksDB, prefixSearch)
{
  char *prefix;
  size_t prefix_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &prefix, &prefix_len) == FAILURE) {
    return;
  }

  object_init_ex(return_value, php_rocksdb_iterator_ce);

  {
    zval db_obj, z_prefix, fname, retval;
    zval params[2];
    ZVAL_COPY_VALUE(&db_obj, getThis());
    ZVAL_STRINGL(&z_prefix, prefix, prefix_len);

    ZVAL_STRING(&fname, "__construct");
    ZVAL_COPY_VALUE(&params[0], &db_obj);
    ZVAL_COPY_VALUE(&params[1], &z_prefix);

    call_user_function(NULL, return_value, &fname, &retval, 2, params);

    zval_dtor(&fname);
    zval_ptr_dtor(&retval);
    zval_dtor(&z_prefix);
  }
}

/* ------------------- RocksDBWriteBatch Methods ------------------- */

/* public function __construct() */
PHP_METHOD(RocksDBWriteBatch, __construct)
{
  rocksdb_write_batch_object *obj;
  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }
  obj = php_rocksdb_write_batch_object_from_zobj(Z_OBJ_P(getThis()));
  obj->batch = rocksdb_writebatch_create();
}

/* public function put(string $key, string $value): bool */
PHP_METHOD(RocksDBWriteBatch, put)
{
  char *key, *value;
  size_t key_len, value_len;
  rocksdb_write_batch_object *obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "ss",
      &key, &key_len, &value, &value_len) == FAILURE) {
    return;
  }
  obj = php_rocksdb_write_batch_object_from_zobj(Z_OBJ_P(getThis()));
  rocksdb_writebatch_put(obj->batch, key, key_len, value, value_len);

  RETURN_TRUE;
}

/* public function delete(string $key): bool */
PHP_METHOD(RocksDBWriteBatch, delete)
{
  char *key;
  size_t key_len;
  rocksdb_write_batch_object *obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &key, &key_len) == FAILURE) {
    return;
  }
  obj = php_rocksdb_write_batch_object_from_zobj(Z_OBJ_P(getThis()));
  rocksdb_writebatch_delete(obj->batch, key, key_len);

  RETURN_TRUE;
}

/* public function clear(): bool */
PHP_METHOD(RocksDBWriteBatch, clear)
{
  rocksdb_write_batch_object *obj;
  if (zend_parse_parameters_none() == FAILURE) {
    return;
  }
  obj = php_rocksdb_write_batch_object_from_zobj(Z_OBJ_P(getThis()));
  rocksdb_writebatch_clear(obj->batch);

  RETURN_TRUE;
}

/* ------------------- RocksDBIterator Methods ------------------- */

/* public function __construct(RocksDB $db, string $prefix = null) */
PHP_METHOD(RocksDBIterator, __construct)
{
  zval *db_zv;
  zval *prefix_zv = NULL;
  rocksdb_iterator_object *it_obj;
  rocksdb_object *db_obj;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "O|z", &db_zv, php_rocksdb_ce, &prefix_zv) == FAILURE) {
    return;
  }
  it_obj = php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  db_obj = php_rocksdb_object_from_zobj(Z_OBJ_P(db_zv));

  it_obj->db_obj = db_obj;
  it_obj->iter = rocksdb_create_iterator(db_obj->db, db_obj->read_options);

  if (prefix_zv && Z_TYPE_P(prefix_zv) == IS_STRING) {
    it_obj->prefix_len = Z_STRLEN_P(prefix_zv);
    it_obj->prefix = estrndup(Z_STRVAL_P(prefix_zv), it_obj->prefix_len);
    rocksdb_iter_seek(it_obj->iter, it_obj->prefix, it_obj->prefix_len);
  } else {
    it_obj->prefix = NULL;
    it_obj->prefix_len = 0;
    rocksdb_iter_seek_to_first(it_obj->iter);
  }
}

/* public function valid(): bool */
PHP_METHOD(RocksDBIterator, valid)
{
  rocksdb_iterator_object *it_obj =
    php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  if (!rocksdb_iter_valid(it_obj->iter)) {
    RETURN_FALSE;
  }
  if (it_obj->prefix && it_obj->prefix_len > 0) {
    size_t key_len;
    const char *key = rocksdb_iter_key(it_obj->iter, &key_len);
    if (key_len < it_obj->prefix_len ||
        memcmp(key, it_obj->prefix, it_obj->prefix_len) != 0) {
      RETURN_FALSE;
    }
  }
  RETURN_TRUE;
}

/* public function key(): string */
PHP_METHOD(RocksDBIterator, key)
{
  rocksdb_iterator_object *it_obj =
    php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  if (!rocksdb_iter_valid(it_obj->iter)) {
    RETURN_FALSE;
  }
  size_t key_len;
  const char *key = rocksdb_iter_key(it_obj->iter, &key_len);
  RETVAL_STRINGL(key, key_len);
}

/* public function current(): string */
PHP_METHOD(RocksDBIterator, current)
{
  rocksdb_iterator_object *it_obj =
    php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  if (!rocksdb_iter_valid(it_obj->iter)) {
    RETURN_FALSE;
  }
  size_t val_len;
  const char *val = rocksdb_iter_value(it_obj->iter, &val_len);
  RETVAL_STRINGL(val, val_len);
}

/* public function next(): void */
PHP_METHOD(RocksDBIterator, next)
{
  rocksdb_iterator_object *it_obj =
    php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  if (rocksdb_iter_valid(it_obj->iter)) {
    rocksdb_iter_next(it_obj->iter);
  }
}

/* public function rewind(): void */
PHP_METHOD(RocksDBIterator, rewind)
{
  rocksdb_iterator_object *it_obj =
    php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  if (it_obj->prefix && it_obj->prefix_len > 0) {
    rocksdb_iter_seek(it_obj->iter, it_obj->prefix, it_obj->prefix_len);
  } else {
    rocksdb_iter_seek_to_first(it_obj->iter);
  }
}

/* public function destroy(): bool */
PHP_METHOD(RocksDBIterator, destroy)
{
  rocksdb_iterator_object *it_obj =
    php_rocksdb_iterator_object_from_zobj(Z_OBJ_P(getThis()));
  if (it_obj->iter) {
    rocksdb_iter_destroy(it_obj->iter);
    it_obj->iter = NULL;
  }
  RETURN_TRUE;
}

/* ------------------- Method Tables ------------------- */

static const zend_function_entry rocksdb_methods[] = {
  PHP_ME(RocksDB, __construct,   arginfo_rocksdb___construct,   ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(RocksDB, compactRange,  arginfo_rocksdb_compactRange,  ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, get,           arginfo_rocksdb_get,           ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, multiGet,      arginfo_rocksdb_multiGet,      ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, put,           arginfo_rocksdb_put,           ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, delete,        arginfo_rocksdb_delete,        ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, write,         arginfo_rocksdb_write,         ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, getIterator,   arginfo_rocksdb_getIterator,   ZEND_ACC_PUBLIC)
  PHP_ME(RocksDB, prefixSearch,  arginfo_rocksdb_prefixSearch,  ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static const zend_function_entry rocksdb_write_batch_methods[] = {
  PHP_ME(RocksDBWriteBatch, __construct, arginfo_rocksdb_writebatch___construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(RocksDBWriteBatch, put,         arginfo_rocksdb_writebatch_put,         ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBWriteBatch, delete,      arginfo_rocksdb_writebatch_delete,      ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBWriteBatch, clear,       arginfo_rocksdb_writebatch_clear,       ZEND_ACC_PUBLIC)
  PHP_FE_END
};

static const zend_function_entry rocksdb_iterator_methods[] = {
  PHP_ME(RocksDBIterator, __construct, arginfo_rocksdb_iterator___construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
  PHP_ME(RocksDBIterator, valid,       arginfo_rocksdb_iterator_valid,       ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBIterator, key,         arginfo_rocksdb_iterator_key,         ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBIterator, current,     arginfo_rocksdb_iterator_current,     ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBIterator, next,        arginfo_rocksdb_iterator_next,        ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBIterator, rewind,      arginfo_rocksdb_iterator_rewind,      ZEND_ACC_PUBLIC)
  PHP_ME(RocksDBIterator, destroy,     arginfo_rocksdb_iterator_destroy,     ZEND_ACC_PUBLIC)
  PHP_FE_END
};

/* ------------------- Module Init / Shutdown / Info ------------------- */

PHP_MINIT_FUNCTION(rocksdb)
{
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, "RocksDB", rocksdb_methods);
  php_rocksdb_ce = zend_register_internal_class(&ce);
  php_rocksdb_ce->create_object = php_rocksdb_object_new;
  memcpy(&rocksdb_object_handlers, zend_get_std_object_handlers(),
         sizeof(zend_object_handlers));
  rocksdb_object_handlers.offset =
    XtOffsetOf(rocksdb_object, std);
  rocksdb_object_handlers.free_obj =
    php_rocksdb_object_free;

  zend_declare_class_constant_long(php_rocksdb_ce, "NO_COMPRESSION",
    sizeof("NO_COMPRESSION")-1, rocksdb_no_compression);
  zend_declare_class_constant_long(php_rocksdb_ce, "SNAPPY_COMPRESSION",
    sizeof("SNAPPY_COMPRESSION")-1, rocksdb_snappy_compression);
  zend_declare_class_constant_long(php_rocksdb_ce, "LZ4_COMPRESSION",
    sizeof("LZ4_COMPRESSION")-1, rocksdb_lz4_compression);
  zend_declare_class_constant_long(php_rocksdb_ce, "ZSTD_COMPRESSION",
    sizeof("ZSTD_COMPRESSION")-1, rocksdb_zstd_compression);

  INIT_CLASS_ENTRY(ce, "RocksDBWriteBatch", rocksdb_write_batch_methods);
  php_rocksdb_write_batch_ce = zend_register_internal_class(&ce);
  php_rocksdb_write_batch_ce->create_object = php_rocksdb_write_batch_object_new;
  memcpy(&rocksdb_write_batch_object_handlers, zend_get_std_object_handlers(),
         sizeof(zend_object_handlers));
  rocksdb_write_batch_object_handlers.offset =
    XtOffsetOf(rocksdb_write_batch_object, std);
  rocksdb_write_batch_object_handlers.free_obj =
    php_rocksdb_write_batch_object_free;

  INIT_CLASS_ENTRY(ce, "RocksDBIterator", rocksdb_iterator_methods);
  php_rocksdb_iterator_ce = zend_register_internal_class(&ce);
  php_rocksdb_iterator_ce->create_object = php_rocksdb_iterator_object_new;
  memcpy(&rocksdb_iterator_object_handlers, zend_get_std_object_handlers(),
         sizeof(zend_object_handlers));
  rocksdb_iterator_object_handlers.offset =
    XtOffsetOf(rocksdb_iterator_object, std);
  rocksdb_iterator_object_handlers.free_obj =
    php_rocksdb_iterator_object_free;

  INIT_CLASS_ENTRY(ce, "RocksDBException", NULL);
  php_rocksdb_exception_ce =
    zend_register_internal_class_ex(&ce, zend_exception_get_default());

  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(rocksdb)
{
  return SUCCESS;
}

PHP_MINFO_FUNCTION(rocksdb)
{
  php_info_print_table_start();
  php_info_print_table_header(2, "RocksDB support", "enabled");
  php_info_print_table_row(2, "Extension Version", PHP_ROCKSDB_VERSION);
  php_info_print_table_end();
}

zend_module_entry rocksdb_module_entry = {
  STANDARD_MODULE_HEADER,
  "rocksdb",
  NULL,
  PHP_MINIT(rocksdb),
  PHP_MSHUTDOWN(rocksdb),
  NULL,
  NULL,
  PHP_MINFO(rocksdb),
  PHP_ROCKSDB_VERSION,
  STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_ROCKSDB
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(rocksdb)
#endif
