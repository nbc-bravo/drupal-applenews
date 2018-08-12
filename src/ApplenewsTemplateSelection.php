<?php

namespace Drupal\applenews;

use Drupal\Core\Entity\EntityTypeManagerInterface;
use Drupal\Core\StringTranslation\StringTranslationTrait;

/**
 * Class ApplenewsTemplateSelection.
 *
 * @package Drupal\applenews
 */
class ApplenewsTemplateSelection {
  use StringTranslationTrait;

  /**
   * Entity type manager.
   *
   * @var \Drupal\Core\Entity\EntityTypeManagerInterface
   */
  protected $entityTypeManager;

  /**
   * Constructs an ApplenewsTemplateSelection object.
   *
   * @param \Drupal\Core\Entity\EntityTypeManagerInterface $entity_type_manager
   *   Entity type manager.
   */
  public function __construct(EntityTypeManagerInterface $entity_type_manager) {
    $this->entityTypeManager = $entity_type_manager;
  }

  /**
   * Get a list of fully-loaded applenews template objects.
   *
   * @param string $node_type
   *   String node type.
   *
   * @return array
   *   An array indexed by entity id of all templates available for
   *   the node type.
   *
   * @throws \Drupal\Component\Plugin\Exception\InvalidPluginDefinitionException
   */
  public function getTemplatesForNodeType($node_type) {
    $templates = \Drupal::entityTypeManager()->getStorage('applenews_template')->loadMultiple();

    $return = [];
    foreach ($templates as $id => $template) {
      if ($template->node_type == $node_type) {
        $return[$id] = $template;
      }
    }

    return $return;
  }

  /**
   * Get available templates for a given node type.
   *
   * @param string $node_type
   *   String node type.
   *
   * @return array
   *   A form element for selecting an applenews_template.
   *
   * @throws \Drupal\Component\Plugin\Exception\InvalidPluginDefinitionException
   */
  public function getTemplateSelectionElement($node_type) {
    $templates = $this->getTemplatesForNodeType($node_type);
    $options = [];
    foreach ($templates as $id => $template) {
      $options[$id] = $template->label;
    }

    return [
      '#type' => 'select',
      '#title' => $this->t('Available templates'),
      '#options' => $options,
    ];
  }

}
