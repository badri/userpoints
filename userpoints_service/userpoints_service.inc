<?php

/**
 * @file
 * Callbacks and access callbacks for userpoints services integration.
 */

/**
 * Access callback for viewing points of users.
 */
function userpoints_service_view_access($uid = NULL) {
  global $user;

  return user_access('view userpoints') || ($uid && user_access('view own userpoints') && $user->uid == $uid);
}

/**
 * Return an array of paged userpoints list.
 *
 * @param $page
 *   Page number of results to return (in pages of 20).
 *
 * @return
 *   An array of userpoints objects.
 **/
function userpoints_service_index($page, $tid, $sort, $dir) {
  if (!in_array($sort, array('points', 'uid', 'last_updated', 'max_points'))) {
    $sort = 'points';
  }

  if (strtoupper($dir) != 'ASC') {
    $dir = 'DESC';
  }

  $select = db_select('userpoints', 't')
    ->orderBy($sort, $dir);


  if ($tid != 'all') {
    if ($tid === NULL) {
      $tid = userpoints_get_default_tid();
    }
    $select->condition('tid', $tid);
  }
  services_resource_build_index_query($select, $page, 'uid, points, max_points', array(), 20);

  $results = $select->execute();

  return services_resource_build_index_list($results, 'userpoints', 'uid');
}

/**
 * Get the number of points of a given user.
 */
function userpoints_service_get($uid, $tid = NULL, $type = 'current') {
  if (!$uid) {
    return services_error(t('User ID parameter is required.'));
  }

  if ($tid === NULL) {
    $tid = userpoints_get_default_tid();
  }

  if ($type == 'max') {
    return userpoints_get_max_points($uid, $tid);
  }
  return userpoints_get_current_points($uid, $tid);
}

/**
 * Add points to a user.
 */
function userpoints_service_add($uid, $points, $tid, $operation, $description, $entity_type, $entity_id) {
  if (!$uid) {
    return services_error(t('User ID parameter is required.'));
  }

  if (!$points) {
    return services_error(t('Points parameter must be a negative or positive number.'));
  }

  if (!$operation) {
    return services_error(t('Operation parameter is required'));
  }

  $transaction = userpoints_grant_points($operation, $points, $uid)
    ->setDescription($description)
    ->setEntity($entity_type, $entity_id);

  if ($tid !== NULL) {
   $transaction->setTid($tid);
  }
  $transaction->save();

  if (!$transaction->getTxnId()) {
    return services_error(t('Adding points failed.'));
  }

  return (object) array(
    'id' => $transaction->getTxnId(),
    'uri' => services_resource_uri(array('userpoints_transaction', $transaction->getTxnId())),
  );
}
