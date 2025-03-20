Rocks DB extension. No guarantees of anything. WORKS in 8.x

Sample usage:

```php
<?php
// - create_if_missing: create the database if it does not exist
// - error_if_exists: error out if the database already exists
// - paranoid_checks: perform extra checks for data integrity
// - write_buffer_size: buffer size for writes (in bytes)
// - max_open_files: maximum number of open files
// - block_restart_interval: interval for block restarts
// - compression: compression type (e.g., RocksDB::SNAPPY_COMPRESSION)
$options = [
  'create_if_missing'      => true,
  'error_if_exists'        => false,
  'paranoid_checks'        => true,
  'write_buffer_size'      => 67108864,    // 64 MB
  'max_open_files'         => 500,
  'block_restart_interval' => 16,
  'compression'            => RocksDB::SNAPPY_COMPRESSION,
];

// Create a new RocksDB instance at the given path with the specified options.
$db = new RocksDB('/your/path', $options);

// Basic put and get operations.
if ($db->put('example_key', 'example_value')) {
  echo "Stored 'example_key' successfully.\n";
} else {
  echo "Failed to store key.\n";
}

$value = $db->get('example_key');
if ($value !== false) {
  echo "Retrieved 'example_key': $value\n";
} else {
  echo "'example_key' not found.\n";
}

// Demonstrate batch writing.
$batch = new RocksDBWriteBatch();
$batch->put('batch_key1', 'batch_value1');
$batch->put('batch_key2', 'batch_value2');
$batch->delete('example_key');
if ($db->write($batch)) {
  echo "Batch operation completed successfully.\n";
} else {
  echo "Batch operation failed.\n";
}

// Demonstrate prefix search.
// Assume keys with the prefix "batch_" are of interest.
$iterator = $db->prefixSearch('batch_');
for ($iterator->rewind(); $iterator->valid(); $iterator->next()) {
  echo $iterator->key() . " => " . $iterator->current() . "\n";
}
?>
