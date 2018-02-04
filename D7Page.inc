<?php
/**
 * @file
 *  Drupal d7page module
 */

class D7Page {

  /**
   * @return array
   */
  public static function blockInfo() {
    return [
      'd7page_list' => [
        'info' => 'Lodging list',
        'region' => 'content',
        // @todo: set to DRUPAL_CACHE_GLOBAL
        'cache' => DRUPAL_NO_CACHE,
      ],
      'd7page_entry' => [
        'info' => 'Lodging entry',
        'region' => 'content',
        // Can't cache this because dynamic;
        // uses URL query parameter d7page-lodging.
        'cache' => DRUPAL_NO_CACHE,
      ]
    ];
  }

  /**
   * Lodging list block view.
   *
   * hook_block_view() implementation.
   *
   * @return array
   */
  public function blockListView() {
    $list = $this->listLodgings();
    if (!$list) {
      return [
        'subject' => 'Lodgings not found',
        'content' => [
          '#markup' => 'No lodgings found.'
        ],
      ];
    }

    // Create legend (table header).
    $first = reset($list);
    unset($first['id']);
    $header = array_keys($first);
    foreach ($header as &$label) {
      $label = $this->fieldNameViewFormat($label);
    }
    // Iteration ref.
    unset($label);

    $path = current_path();
    $query = drupal_get_query_parameters();
    unset($query['d7page-lodging']);

    $rows = [];
    foreach ($list as &$entry) {
      $id = $entry['id'];
      $title = $entry['headline'];
      unset($entry['id'], $entry['headline']);
      // Fix property type.
      $entry['propertyType'] = ucfirst(
        strtolower(
          str_replace('_', ' ', $entry['propertyType'])
        )
      );

      $values = array_values($entry);
      // Prepend entry headline as link.
      array_unshift(
        $values,
        [
          'data' => [
            '#theme' => 'link',
            '#text' => $title,
            '#path' => $path,
            // theme_link() fails unless these exist.
            '#options' => [
              'query' => $query + [
                'd7page-lodging' => $id
              ],
              'attributes' => [],
              'html' => [],
            ],
          ],
        ]
      );
      $rows[] = $values;
    }
    // Iteration ref.
    unset($entry);

    return [
      'subject' => 'Lodgings',
      'content' => [
        '#theme' => 'table',
        '#header' => $header,
        '#rows' => $rows,
      ],
    ];
  }

  /**
   * Lodging entry block view.
   *
   * hook_block_view() implementation.
   *
   * @param string $id
   *    Entry machine name.
   *
   * @return array
   */
  public function blockEntryView(string $id) {
    $entry = $this->getLodging($id);
    if (!$entry) {
      return [
        'subject' => 'Lodging not found',
        'content' => [
          '#markup' => 'Lodging by id ' . $id . ' could not be found.'
        ],
      ];
    }

    $title = $entry['headline'];
    unset($entry['id'], $entry['headline']);
    // Fix property type.
    $entry['propertyType'] = ucfirst(
      strtolower(
        str_replace('_', ' ', $entry['propertyType'])
      )
    );

    $rows = [];
    foreach ($entry as $field_name => $value) {
      $rows[] = [
        $this->fieldNameViewFormat($field_name),
        $value,
      ];
    }

    return [
      'subject' => $title,
      'content' => [
        '#theme' => 'table',
        '#rows' => $rows,
      ],
    ];
  }

  /**
   * Convert model field name to user-friendly title.
   *
   * @param string $fieldName
   *
   * @return string
   */
  public function fieldNameViewFormat(string $fieldName) {
    // De-camelCase.
    return ucfirst(
      strtolower(
        preg_replace('/([a-z])([A-Z])/', '$1 $2', $fieldName)
      )
    );
  }

  /**
   * Get a lodging from database.
   *
   * @return array|bool
   */
  public function getLodging(string $id) {
    try {
      return db_select('d7page_lodging', 'l', array('fetch' => PDO::FETCH_ASSOC))
        ->fields('l')
        ->condition('l.id', $id, '=')
        ->execute()->fetchAssoc();
    }
    catch (\Throwable $xcptn) {
      $this->log($xcptn, 'Failed to retrieve a lodging from database');
    }
    return false;
  }

  /**
   * Get all lodgings from database.
   *
   * @return array|bool
   */
  public function listLodgings() {
    try {
      $query = db_select('d7page_lodging', 'l', array('fetch' => PDO::FETCH_ASSOC))
        ->fields('l');
      $query->orderBy('l.headline', 'ASC')->orderBy('l.id', 'ASC');
      return $query->execute()->fetchAll();
    }
    catch (\Throwable $xcptn) {
      $this->log($xcptn, 'Failed to retrieve all lodgings from database');
    }
    return false;
  }

  /**
   * Reads lodgings from JSON source into the db table d7page_lodging
   * unless that table is non-empty.
   *
   * hook_enable() implementation.
   */
  public function moduleEnable() {
    $msg = 'Failed to populate db table d7page_lodging';

    // Check if the database already contains entries.
    $lodgings = $this->listLodgings();
    if ($lodgings) {
      // No work to do.
      $this->log($lodgings, '$lodgings', WATCHDOG_DEBUG);
      return;
    }
    if ($lodgings === false) {
      // Fauilure.
      $this->log(null, $msg);
      return;
    }

    // Load from JSON.
    $json_source_file = drupal_get_path('module', 'd7page') . '/d7page_lodging-list.json';
    $json_source = file_get_contents(drupal_get_path('module', 'd7page') . '/d7page_lodging-list.json');
    if (!$json_source) {
      $this->log($json_source_file, $msg . ' because failed to read JSON source lodgings file');
    }
    $lodgings = drupal_json_decode($json_source);
    if (!$lodgings) {
      $this->log(null, $msg . ' because failed to JSON decode source lodgings: ' . json_last_error_msg());
      return;
    }

    // Insert.
    try {
      $query = db_insert('d7page_lodging')->fields(
      // Get field names directly from schema declaration.
        array_keys(
          (d7page_schema())['d7page_lodging']['fields']
        )
      );
      foreach ($lodgings as $entry) {
        // 'id' must go first.
        $id = $entry['id'];
        unset($entry['id']);
        $query->values(
          [ 'id' => $id ] + $entry
        );
      }
      $query->execute();
    }
    catch (\Throwable $xcptn) {
      $this->log($xcptn, $msg . ' because failed to insert');
    }
  }

  /**
   * Log exception or variables to watchdog.
   *
   * Uses module inspect if exists.
   *
   * @param mixed $xcptn_or_variables
   *    Null: ignore argument and log arg $message only.
   * @param string $message
   * @param int $severity
   */
  public function log($xcptn_or_variables = null, string $message = '', int $severity = WATCHDOG_ERROR) {
    $inspect = module_exists('inspect');
    if ($xcptn_or_variables instanceof \Throwable) {
      if ($inspect) {
        Inspect::trace(
          $xcptn_or_variables,
          [
            'type' => 'd7page',
            'message' => $message,
            'severity' => $severity,
          ]
        );
      }
      else {
        watchdog(
          'd7page',
          (!$message ? '' : ($message . ': ')) . $xcptn_or_variables->getMessage(),
          [],
          $severity
        );
      }
    }
    elseif ($xcptn_or_variables !== null) {
      if ($inspect) {
        Inspect::log(
          $xcptn_or_variables,
          [
            'type' => 'd7page',
            'message' => $message,
            'severity' => $severity,
          ]
        );
      }
      else {
        watchdog(
          'd7page',
          (!$message ? '' : ($message . ': ')) . var_export($xcptn_or_variables, true),
          [],
          $severity
        );
      }
    }
    else {
      watchdog(
        'd7page',
        $message,
        [],
        $severity
      );
    }
  }
}
