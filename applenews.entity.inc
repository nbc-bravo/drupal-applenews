<?php
/**
 * @file
 * Entity hook implementations.
 */

/**
 * Implements hook_entity_load().
 */
function applenews_entity_load($entities, $type) {
  foreach ($entities as $entity) {
    /** @var EntityDrupalWrapper $wrapper */
    $wrapper = entity_metadata_wrapper($type, $entity);
    $settings = db_select('applenews_entity_settings', 'a')
      ->fields('a', ['data'])
      ->condition('entity_type', $type)
      ->condition('entity_id', $wrapper->getIdentifier())
      ->condition('revision_id', $wrapper->getIdentifier())
      ->execute()
      ->fetchField();
    if ($settings) {
      $entity->applenews = unserialize($settings);
    }
  }
}

/**
 * Implements hook_entity_insert().
 */
function applenews_entity_insert($entity, $type) {
  applenews_entity_save('insert', $entity, $type);
}

/**
 * Implements hook_entity_update().
 */
function applenews_entity_update($entity, $type) {
  applenews_entity_save('update', $entity, $type);
}

/**
 * Helper function that handles entity insert and update by an $op pararm.
 * Saves the Apple News entity specific settings and pushed to Apple News.
 *
 * @param string $op
 *   Operation happening on $entity, one of 'insert' or 'update'.
 * @param object $entity
 *   Entity object.
 * @param string $type
 *   Entity type.
 */
function applenews_entity_save($op, $entity, $type) {
  if (isset($entity->applenews)) {
    // Save Apple News entity settings.
    _applenews_entity_settings_save($op, $entity, $type, $entity->applenews);

    // Allow others to alter the per-entity $settings before use.
    drupal_alter('applenews_entity_settings', $entity->applenews, $entity, $type);

    if (!empty($entity->applenews['publish_flag']) && $entity->applenews['publish_flag']) {
      applenews_entity_push('update', $entity, $type);
    }
  }
}

/**
 * Save the Apple News settings for a particular entity.
 *
 * @param string $op
 *   One of 'insert' or 'update'.
 * @param object $entity
 *   Entity object.
 * @param string $type
 *   Entity type.
 * @param array $settings
 *   Settings to save for this entity.
 */
function _applenews_entity_settings_save($op, $entity, $type, $settings) {
  module_load_include('inc', 'applenews');

  /** @var EntityDrupalWrapper $wrapper */
  $wrapper = entity_metadata_wrapper($type, $entity);
  // Allow others to alter the per-entity $settings before save.
  drupal_alter('applenews_entity_settings_pre_save', $settings, $entity, $type);
  $record = [
    'entity_type' => $type,
    'entity_id' => $wrapper->getIdentifier(),
    'revision_id' => $wrapper->getIdentifier(),
    'data' => $settings,
  ];
  if ($op === 'insert') {
    drupal_write_record('applenews_entity_settings', $record);
  }
  elseif ($op === 'update') {
    drupal_write_record('applenews_entity_settings', $record,
      ['entity_type', 'entity_id', 'revision_id']
    );
  }
}

/**
 * Push node to Apple News.
 *
 * @param string $op
 *   One of 'insert' or 'update'.
 * @param object $entity
 *   Entity object.
 * @param string $type
 *   Entity type.
 */
function applenews_entity_push($op, $entity, $type) {
  module_load_include('inc', 'applenews');

  /** @var EntityDrupalWrapper $wrapper */
  $wrapper = entity_metadata_wrapper($type, $entity);
  $export = applenews_entity_get_export($type, $wrapper->getIdentifier());

  // Attempt to load settings from the database.
  if (isset($entity->applenews)) {
    $settings = $entity->applenews;
  }
  else {
    $settings = db_select('applenews_entity_settings', 'a')
      ->fields('a', ['data'])
      ->condition('entity_type', $type)
      ->condition('entity_id', $wrapper->getIdentifier())
      ->condition('revision_id', $wrapper->getIdentifier())
      ->execute()
      ->fetchField();
    if ($settings) {
      $settings = unserialize($settings);
    }
  }

  try {
    foreach (applenews_channels(TRUE) as $channel_id => $channel) {
      if (!in_array($channel_id, $settings['channels'])) {
        continue;
      }
      $metadata = [];
      foreach ($channel['sections'] as $section_id => $section) {
        if (in_array($section_id, $settings['sections'])) {
          $metadata['data']['links']['sections'][] = applenews_section_url($section_id);
        }
      }
      $metadata['data']['isPreview'] = $settings['is_preview'];
      if (!applenews_op($op, $channel_id, $entity, $type, $export, $metadata)) {
        drupal_set_message(t('Unable to push %title to %channel', [
            '%title' => $wrapper->label(),
            '%channel' => $channel['name'],
          ]),
          'error'
        );
      }
    }
  }
  catch (ApplenewsInvalidExport $e) {
    drupal_set_message(t('Unable to push %title to Apple News: @message', [
        '%title' => $wrapper->label(),
        '@message' => $e->getMessage(),
      ]),
      'error'
    );
  }
}

/**
 * Implements hook_entity_delete().
 */
function applenews_entity_delete($entity, $type) {
  module_load_include('inc', 'applenews');

  /** @var EntityDrupalWrapper $wrapper */
  $wrapper = entity_metadata_wrapper($type, $entity);
  if ($channel_ids = applenews_load_article_channels($entity, $type)) {
    foreach ($channel_ids as $channel_id) {
      if (!applenews_op('delete', $channel_id, $entity, $type)) {
        $channel = applenews_channel($channel_id);
        drupal_set_message(t('Unable to delete %title from %channel', [
            '%title' => $wrapper->label(),
            '%channel' => filter_xss($channel->name)
          ]),
          'error'
        );
      }
    }
  }

  // Delete the per-entity settings.
  db_delete('applenews_entity_settings')
    ->condition('entity_type', $type)
    ->condition('entity_id', $wrapper->getIdentifier())
    ->condition('revision_id', $wrapper->getIdentifier())
    ->execute();
}
