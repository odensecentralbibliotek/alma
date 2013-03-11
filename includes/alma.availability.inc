<?php
/**
 * @file
 * Ding_availability implementation for the Alma ding_provider.
 */

/**
 * Get the holding parts defined for Alma.
 */
function alma_availability_holding_parts() {
  static $holding_parts;
  if (!isset($holding_parts)) {
    $holding_parts = array('branch', 'department', 'location', 'sublocation', 'collection', 'shelf_mark_1', 'shelf_mark_2');
  }
  return $holding_parts;
}

/**
 * Implements provider availability, holdings.
 */
function alma_availability_holdings($provider_ids, $only_available = TRUE) {
  $ids = join(',', $provider_ids);

  $details = alma_client_invoke('catalogue_record_detail', $ids);
  $result = array();
  if ($details && isset($details['records'])) {
    foreach ($details['records'] as $alma_id => $record) {
      $holding = array(
        'local_id' => $alma_id,
        'available' => ($record['available_count'] > 0),
        'reservable' => $record['reservable'],
        'reserved_count' => (int) $record['reservation_count'],
        'deferred_period' => FALSE,
        'is_periodical' => ($record['media_class'] == 'periodical'),
      );

      $total = $total_reservable = 0;

      // START periodicals
      if ($holding['is_periodical']) {
        $holding['issues'] = $record['issues'];
        foreach ($record['holdings'] as $volume => $issues) {
          foreach ($issues as $issue_no => $holds) {
            $issue_holding['holdings'] = array();
            foreach ($holds as $key => $branch_holding) {
              _alma_availability_branch_holding($branch_holding, $issue_holding, $only_available);
            }
            $holding['issues'][$volume][$issue_no] = array_merge($holding['issues'][$volume][$issue_no], $issue_holding);
            $total_reservable += $holding['issues'][$volume][$issue_no]['reservable_count'];
            $total += $holding['issues'][$volume][$issue_no]['total_count'];
          }
        }
      } // END periodicals
      else {
        foreach ($record['holdings'] as $branch_holding) {
          _alma_availability_branch_holding($branch_holding, $holding, $only_available);
        }
        $total_reservable = $record['reservable_count'];
        $total = $record['total_count'];
      }

      $holding['reservable_count'] = $total_reservable;
      $holding['total_count'] = $total;
      $result[$alma_id] = $holding;
    }
  }

  return $result;
}

/**
 * Build the location name and count the amount of materials available.
 * Utility function to avoid code duplication.
 *
 * @param type $branch_holding Input Alma client holding info, will not be modified.
 * @param type $holding Output holding info
 * @param bool $only_available Only add to $holding if available_count > 0.
 * @return type NULL
 */
function _alma_availability_branch_holding(&$branch_holding, &$holding, $only_available) {
  if (in_array($branch_holding['collection_id'], array('karens', 'karens-'))) {
    $holding['deferred_period'] = TRUE;
  }

  $parts = array();

  if ($only_available && $branch_holding['available_count'] == 0) {
    return;
  }

  $org = alma_get_organisation();
  $holding_parts = variable_get('alma_availability_holding_parts', alma_availability_holding_parts());
  foreach ($holding_parts as $part) {
    if (!empty($branch_holding[$part . '_id'])) {
      $parts[] = $org[$part][$branch_holding[$part . '_id']];
    }
  }

  if ((isset($holding_parts['shelf_mark_1']) || isset($holding_parts['shelf_mark_2'])) && !empty($branch_holding['shelf_mark'])) {
    // Shelf mark might have leading >. Trimming will make them empty strings,
    // which will be removed by array_filter.
    $shelf_mark = array_map("trim", explode(">", $branch_holding['shelf_mark']));
    if (isset($holding_parts['shelf_mark_1'])) {
      // If enabled, don't show shelf_mark_1 if equal to previous part.
      if (!variable_get('alma_availability_remove_duplicate', TRUE) || strtolower($parts[count($parts) - 1]) != strtolower($shelf_mark[0])) {
        // If enabled, don't show shelf_mark_1 if it consist of only upper case characters.
        if (!variable_get('alma_availability_remove_all_caps', FALSE) || !ctype_upper($shelf_mark[0])) {
          $parts[] = $shelf_mark[0];
        }
      }
    }
    if (isset($holding_parts['shelf_mark_2'])) {
      $parts[] = $shelf_mark[1];
    }
  }

  // Remove any empty strings in parts
  // Remove the keys to make sure it translates to an array in javascript
  $parts = array_values(array_filter($parts));

  if ($parts) {
    $holding['holdings'][] = array(
      'location' => $parts,
      'total_count' => $branch_holding['total_count'],
      'available_count' => $branch_holding['available_count'],
    );
  }
}
