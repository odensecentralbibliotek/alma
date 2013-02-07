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
    $holding_parts = array('branch', 'department', 'location', 'sublocation', 'collection', 'shelf_mark');
  }
  return $holding_parts;
}

/**
 * Implements provider availability, holdings.
 */
function alma_availability_holdings($provider_ids) {
  $ids = join(',', $provider_ids);

  $details = alma_client_invoke('catalogue_record_detail', $ids);
  $result = array();
  if ($details && isset($details['records'])) {
    foreach ($details['records'] as $alma_id => $record) {
      $holding = array(
        'local_id' => $alma_id,
        'available' => ($record['available_count'] > 0),
        'reservable' => $record['show_reservation_button'],
        'show_reservation_button' => $record['show_reservation_button'],
        'reserved_count' => (int) $record['reservation_count'],
        'deferred_period' => FALSE,
        'is_periodical' => ($record['media_class'] == 'periodical'),
      );

      $total = $total_reservable = 0;

      // START periodicals
      if ($holding['is_periodical']) {
        $holding['holdings'] = array();
        foreach ($record['holdings'] as $volume => $issues) {
          foreach ($issues as $issue_no => $holds) {
            $issue = array();
            $issue['branches'] = array();
            $issue['local_id'] = $branch_holding['reservable'];
            $issue['reservable'] = (($branch_holding['status'] == 'availableForLoan') && ((int)$branch_holding['total_count'] - (int)$branch_holding['reference_count']));
            $issues_array[$volume][$issue_no] = $issue;

            foreach ($holds as $key => $branch_holding) {
              if (!in_array($branch_holding['branch_id'], $issue['branches'])) {
                $issue['branches'][] = $branch_holding['branch_id'];
              }

              _alma_availability_branch_holding($branch_holding, $holding, $total, $total_reservable);
            }
          }
        }

        if (is_array($holding['holdings'])) {
          asort($holding['holdings']);
        }
        $holding['issues'] = $issues_array;
      } // END periodicals
      else {
        foreach ($record['holdings'] as $branch_holding) {
          _alma_availability_branch_holding($branch_holding, $holding, $total, $total_reservable);
        }
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
 * @param type $total Output total amount
 * @param type $total_reservable Output total reservable amount
 * @return type NULL
 */
function _alma_availability_branch_holding(&$branch_holding, &$holding, &$total, &$total_reservable) {
  if (in_array($branch_holding['collection_id'], array('karens', 'karens-'))) {
    $holding['deferred_period'] = TRUE;
  }

  $parts = array();
  $total += (int) $branch_holding['total_count'];

  // Reservable is total items minus reference (which cannot be
  // loaned).
  $reservable_count = (int) $branch_holding['total_count'] - (int) $branch_holding['reference_count'];
  $total_reservable += $reservable_count;

  $org = alma_get_organisation();
  foreach (alma_availability_holding_parts() as $part) {
    if (!empty($branch_holding[$part . '_id'])) {
      $parts[] = $org[$part][$branch_holding[$part . '_id']];
    }
  }

  if (!empty($branch_holding['shelf_mark'])) {
    // Shelf mark might have leading >, strip any and replace the rest
    // with the proper arrow.
    $parts[] = strtr(trim($branch_holding['shelf_mark'], " >\n\t"), array('>' => '→'));
  }

  $parts = array_filter($parts);

  if ($parts && $branch_holding['total_count'] > $branch_holding['checked_out_count']) {
    $branch_string = join(' → ', $parts);
    if (!in_array($branch_string, $holding['holdings'])) {
      $holding['holdings'][] = $branch_string;
    }
  }
}
