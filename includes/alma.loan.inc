<?php
/**
 * @file
 * Implements provider hooks that is associated with loans in alma.
 */

/**
 * Implements hook_loan_list().
 *
 * This functions finds all loans for a given user account and returns the loans
 * as DingProviderLoan objects.
 *
 */
function alma_loan_list($account) {
  $results = &drupal_static(__FUNCTION__);
  if (!isset($results)) {
    $creds = ding_user_get_creds($account);
    $results = array();

    try {
      // Get all loans form alma for the user account given. 
      $loans =  alma_client_invoke('get_loans', $creds['name'], $creds['pass']);
    } catch (Exception $e) {
      throw new DingProviderLoanUserError($e->getMessage());
    }

    foreach ($loans as $loan) {
      $results[$loan['id']] = new DingProviderLoan($loan['id'], array(
        'ding_entity_id' => variable_get('ting_agency', '') . ':' . $loan['record_id'],
        'loan_date' => $loan['loan_date'],
        'expiry' => $loan['due_date'],
        'renewable' => $loan['is_renewable'],
        'materials_number' => $loan['id'],
        'notes' => isset($loan['notes']) ? $loan['notes'] : DingEntityBase::NULL,
      ));
    }
  }
  return $results;
}

/**
 * Implements hook_loan_renew().
 *
 * Tries to renew the loans with the id's passed as parameter.
 */
function alma_loan_renew($account, $ids) {
  $creds = ding_user_get_creds($account);
  $result = array();
  $res = alma_client_invoke('renew_loan', $creds['name'], $creds['pass'], $ids);

  foreach ($ids as $id) {
    if (isset($res[$id]) && $res[$id] === TRUE) {
      $result[$id] = DingProviderLoan::STATUS_RENEWED;
    }
    else {
      /**
       * @todo there are message saying why, the renew failed.
       */
      $result[$id] = DingProviderLoan::STATUS_NOT_RENEWED;
    }
  }
  return $result;
}

/**********************************
 * Helper functions
 */

function alma_loan_get_loans($account, $reset = FALSE) {
  $creds = ding_user_get_creds($account);

  // Try to look in the session for loan information.
  if (!isset($_SESSION['alma_loans']) || !is_array($_SESSION['alma_loans']) || $reset) {
    $_SESSION['alma_loans'] = alma_client_invoke('get_loans', $creds['name'], $creds['pass']);
  }

  return $_SESSION['alma_loans'];
}

function alma_loan_clear_cache() {
  unset($_SESSION['alma_loans']);
}

function alma_loan_exists($account, $item_id) {
  $loans = alma_loan_get_loans( $account );
  foreach ($loans as $res) {
    if ($res['record_id'] == $item_id) {
      return TRUE;
    }
    return FALSE;
  }
}

function alma_loan_format_date($datetime) {
  $timestamp = (!is_int($datetime)) ? strtotime($datetime) : $datetime;
  return date('Y-m-d', $timestamp);
}