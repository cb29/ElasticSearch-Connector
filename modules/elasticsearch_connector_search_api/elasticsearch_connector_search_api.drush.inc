<?php

/**
 * @file
 * Drush integration for the elasticsearch_connector_search_api module.
 */

/**
 * Implements hook_drush_command().
 */
function elasticsearch_connector_search_api_drush_command() {
  $items['reset-elasticsearch'] = array(
    'description' => dt('Drop and re-create a Search API Elasticsearch index.'),
    'arguments' => array(
      'index' => 'The name of the Search API index whose Elasticsearch index to re-create.',
    ),
    'required-arguments' => TRUE,
    'options' => array(
      'yes' => 'Skip confirmation and proceed.',
    ),
  );
  return $items;
}

/**
 * Callback for the reset-elasticsearch drush command.
 */
function drush_elasticsearch_connector_search_api_reset_elasticsearch($search_api_index_name) {
  // Load the Search API index.
  $index = search_api_index_load($search_api_index_name);
  if (!$index || $index->server()->class != 'search_api_elasticsearch_connector') {
    return drush_set_error('elasticsearch_connector_search_api', t("Search API index '@index_name' not found.", array('@index_name' => $search_api_index_name)));
  }

  $es_index = $index->options['index_name'];

  // Find the affected Search API indexes.
  $all_indexes = search_api_index_load_multiple(FALSE, array('enabled' => TRUE));
  // Always list the specified index first.
  $affected_indexes = array($index);
  foreach ($all_indexes as $index) {
    if ($index->machine_name == $search_api_index_name) {
      continue;
    }

    $server = $index->server();
    if ($server->class == 'search_api_elasticsearch_connector' && $index->options['index_name'] == $es_index) {
      $affected_indexes[] = $index;
    }
  }
  
  // List Search API indexes this opperation will affect.
  drush_print('The following Search API indexes will be affected:');
  foreach ($affected_indexes as $index) {
    drush_print(dt(' * @name (@machine_name)', array(
      '@name' => $index->name,
      '@machine_name' => $index->machine_name,
    )));
  }

  // Give the user a chance to bail.
  if (!drush_confirm(dt("Do you really want to destroy and re-create the Elasticsearch index '!es_index'?", array('!es_index' => $es_index)))) {
    return drush_user_abort();
  }

  // Remove the existing indexes.
  foreach ($affected_indexes as $index) {
    try {
      $index->server()->removeIndex($index);
    }
    catch (Exception $e) {
      // The removeIndex() method throws an exception when the index does not
      // exist, but that is ok in this situation.
    }
  }

  // Re-create and re-index the indexes.
  foreach ($affected_indexes as $index) {
    try {
      $index->server()->addIndex($index);
      $index->reindex();
      drush_log(dt('Re-initialized Search API index @name (@machine_name)', array(
        '@name' => $index->name,
        '@machine_name' => $index->machine_name,
      )), 'ok');
    }
    catch (Exception $e) {
      // This exception, on the other hand, is important.
      return drush_set_error($e->getMessage());
    }
  }
}