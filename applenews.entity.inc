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
  // Save Apple News entity settings.
  if ($settings = _applenews_entity_settings_process($entity, $type)) {
    _applenews_entity_settings_save('insert', $entity, $type, $settings);
  }

  if (!empty($settings['publish_flag'])) {
    applenews_entity_push('insert', $entity, $type, $settings);
  }
}

/**
 * Implements hook_entity_update().
 */
function applenews_entity_update($entity, $type) {
  // Save Apple News entity settings.
  if ($settings = _applenews_entity_settings_process($entity, $type)) {
    _applenews_entity_settings_save('update', $entity, $type, $settings);
  }

  if (!empty($settings['publish_flag'])) {
    applenews_entity_push('update', $entity, $type, $settings);
  }
}

/**
 * Process the form representation of Apple News entity settings into something
 * a little more manageble.
 *
 * @param object $entity
 *   Entity object.
 * @param string $type
 *   Entity type.
 *
 * @return array|null
 */
function _applenews_entity_settings_process($entity, $type) {
  module_load_include('inc', 'applenews');

  if (isset($entity->applenews)) {
    $settings = $entity->applenews;
    // Some of $entity->applenews is form specific, clean it up a bit.
    $channels = $sections = [];
    foreach (applenews_channels(TRUE) as $channel_id => $channel) {
      if (!empty($settings['channels']['channel-' . $channel_id])) {
        $channels[] = $channel_id;
        foreach ($channel['sections'] as $section_id => $section) {
          if (!empty($settings['channels']['section-' . $channel_id . '-' . $section_id])) {
            $sections[] = $section_id;
          }
        }
      }
    }
    $settings['channels'] = $channels;
    $settings['sections'] = $sections;
    $settings['is_preview'] = isset($settings['is_preview']) ? (bool)$settings['is_preview'] : FALSE;

    // Allow others to alter the per-entity $settings, which allows custom
    // logic around publishing and draft mode.
    drupal_alter('applenews_entity_settings', $settings, $entity, $type);

    return $settings;
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
 * @param array $settings
 *   Associative array of Apple News entity settings with the following keys:
 *   - channels: Array of channel id's to publish the entity to. Required.
 *   - sections: Array of section id's to publish the entity to. Optional.
 *   - is_preview: Boolean indicating if this entity should be published as a
 *     Draft or not. Optional, defaults to export configuration.
 *   If a value is not passed this function will attempt to load them from the
 *   database.
 */
function applenews_entity_push($op, $entity, $type, $settings = NULL) {
  module_load_include('inc', 'applenews');

  /** @var EntityDrupalWrapper $wrapper */
  $wrapper = entity_metadata_wrapper($type, $entity);
  $export = applenews_entity_get_export($type, $wrapper->getIdentifier());

  // Attempt to load settings from the database.
  if (!$settings) {
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
    else {
      return;
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
  if ($channel_ids = applenews_load_article_channels($entity, $type)) {
    foreach ($channel_ids as $channel_id) {
      if (!applenews_op('delete', $channel_id, $entity, $type)) {
        $wrapper = entity_metadata_wrapper($type, $entity);
        $channel = applenews_channel($channel_id);
        drupal_set_message(
          t('Unable to delete %title from %channel',
            [
              '%title'   => $wrapper->label(),
              '%channel' => filter_xss($channel->name)
            ]), 'error'
        );
      }
    }
  }
}
