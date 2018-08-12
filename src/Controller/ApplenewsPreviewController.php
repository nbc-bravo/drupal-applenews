<?php

namespace Drupal\applenews\Controller;

use Drupal\applenews\ApplenewsPreviewBuilder;
use Drupal\Component\Serialization\Json;
use Drupal\Core\Controller\ControllerBase;
use Psr\Log\LoggerInterface;
use Symfony\Component\DependencyInjection\ContainerInterface;
use Symfony\Component\HttpFoundation\BinaryFileResponse;
use Symfony\Component\Serializer\Serializer;

/**
 * Class ApplenewsPreviewController.
 *
 * @package Drupal\applenews\Controller
 */
class ApplenewsPreviewController extends ControllerBase {

  /**
   * @var \Psr\Log\LoggerInterface
   */
  protected $logger;

  /**
   * @var \Symfony\Component\Serializer\Serializer
   */
  protected $serializer;

  /**
   * @var \Drupal\applenews\ApplenewsPreviewBuilder
   */
  protected $previewBuilder;

  /**
   * ApplenewsPreviewController constructor.
   *
   * @param \Symfony\Component\Serializer\Serializer $serializer
   * @param \Drupal\applenews\ApplenewsPreviewBuilder $preview_builder
   */
  public function __construct(LoggerInterface $logger, Serializer $serializer, ApplenewsPreviewBuilder $preview_builder) {
    $this->logger = $logger;
    $this->serializer = $serializer;
    $this->previewBuilder = $preview_builder;
  }

  /**
   * {@inheritdoc}
   */
  public static function create(ContainerInterface $container) {
    return new static(
      $container->get('logger.channel.applenews'),
      $container->get('serializer'),
      $container->get('applenews.preview_builder')
    );
  }

  /**
   * Generates article.json and assets for preview.
   *
   * @param \Drupal\node\NodeInterface $node
   *   Node object.
   * @param string $template_id
   *   String template ID.
   * @param $revision_id
   *   Revision ID.
   *
   * @return array
   *   An array of preview element.
   */
  public function preview($entity_type, $entity, $revision_id) {
    $context['template_id'] = 'article';
    $directory = 'public://applenews_preview';
    // $filename = 'applenews-node-' . $entity->id() . '-' . $context['template_id'] . '.json';
    $filename = NULL;
    $entity_archive = TRUE;
    $entity_id = $entity->id();
    $entity_ids = [$entity_id];

    /** @var \ChapterThree\AppleNewsAPI\Document $document */
    $document = $this->serializer->normalize($entity, 'applenews', $context);

    $this->exportToFile($entity_id, $entity_ids, $filename, $entity_archive, $document);
    $archive_path = $this->exportFilePath($entity_id);
    $archive = $archive_path . '.zip';
    $output = file_get_contents($archive, FILE_USE_INCLUDE_PATH);

    $this->previewBuilder->entityArchiveDelete($entity_id);
    $this->previewBuilder->removeDirectories($entity_ids);

    $headers = [
      'Content-Type' => 'application/zip',
      'Content-Disposition' => 'attachment; filename=' . $entity_id . '.zip',
    ];
    print $output;exit;
    // return new BinaryFileResponse($archive, 200, $headers, FALSE);
  }

  /**
   * @param $entity_id
   * @param $entity_ids
   * @param $filename
   * @param $entity_archive
   * @param $data
   *
   * @return int|null|string
   */
  protected function exportToFile($entity_id, $entity_ids, $filename, $entity_archive, $data) {
    $preview = $this->previewBuilder->setEntity($entity_id, $filename, $entity_archive, $data);

    if (!empty($entity_id)) {
      $preview->toFile();
      try {
        $preview->archive($entity_ids);
      }
      catch (\Exception $e) {
        $this->logger->error('Could not create archive: @err', ['@err' => $e->getMessage()]);
      }
      return NULL;
    }
    else {
      $file_url = $preview->getArchiveFilePath();
      try {
        $preview->archive($entity_ids);
      }
      catch (\Exception $e) {
        $this->logger->error('Could not create archive: @err', ['@err' => $e->getMessage()]);
        return NULL;
      }
      return $file_url;
    }
  }

  protected function exportFilePath($entity_id) {
    return $this->previewBuilder->getEntityArchivePath($entity_id);
  }

}

/*
         if ($export = applenews_entity_get_export($entity_type, $entity_id)) {
          $data = applenews_entity_export($export, $entity_type, $entity_id);
          applenews_export_to_file($entity_id, [$entity_id], NULL, TRUE, $data);
          $archive_path = applenews_export_file_path($entity_id);

          $archive = $archive_path . '.zip';
          $output = file_get_contents($archive, FILE_USE_INCLUDE_PATH);

          // Remove individual entity archive.
          applenews_export_file_delete($entity_id);
          // Remove individual entity directory.
          applenews_export_dir_cleanup([$entity_id]);

          drupal_add_http_header('Content-Type', 'application/zip');
          drupal_add_http_header('Content-Disposition', 'attachment; filename=' . $entity_id . '.zip');

          echo $output;

        }
        else {
          return t('Export not defined for this entity.');
        }
      }
      else {
        return t('Entity not found.');
      }

      drupal_exit();
      return NULL;

 */
