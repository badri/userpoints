<?php

/**
 * @file
 * Contains the UserpointsTransaction and related classes.
 */

/**
 * A Userpoints transaction.
 *
 * @ingroup userpoints_api
 */
class UserpointsTransaction extends Entity {

  /**
   * The transaction has been approved.
   */
  const STATUS_APPROVED = 0;
  /**
   * The transaction is pending for approval.
   */
  const STATUS_PENDING = 1;
  /**
   * The transaction has been declined.
   */
  const STATUS_DECLINED = 2;

  /**
   * The transaction id (primary key) of this transaction
   *
   * @var integer
   */
  public $txn_id = NULL;

  public $type;

  public $uid = 0;

  public $points;

  public $operation;

  public $status;

  public $tid;

  public $expirydate;

  public $expired;

  public $time_stamp;

  public $changed;

  public $approver_uid = 0;

  public $description;

  public $reference;

  public $parent_txn_id = 0;

  public $entity_type;

  public $entity_id = 0;

  /**
   * Deny reasons.
   *
   * @var array
   */
  protected $denied_reasons = array();

  /**
   * TRUE if the transaction should display a message.
   *
   * @var boolean
   */
  protected $display = TRUE;

  /**
   * Overriden message, is used instead of the default in
   * UserpointsTransaction::getReason() if existend.
   *
   * @var string
   */
  protected $message;

  /**
   * The original status of this transaction, used for denying changes to this
   * transaction if is is not pending anymore.
   *
   * @var int
   */
  protected $orig_status;

  /**
   * If a transaction is aborted, it will not be saved automatically.
   *
   * Automatically set whenever an exception occurs.
   */
  protected $aborted = FALSE;

  /**
   * Start a new transaction or update an existing one.
   *
   * @param $txn_id
   *   Transaction id if an existing transaction should be loaded.
   */
  function __construct($values = array()) {
    parent::__construct($values, 'userpoints_transaction');

    // Apply default status.
    if ($this->status === NULL) {
      $this->status = variable_get('userpoints_points_moderation', UserpointsTransaction::STATUS_APPROVED);
    }
    else {
      $this->orig_status = $this->getStatus();
    }

    if ($this->tid === NULL) {
      $this->tid = userpoints_get_default_tid();
    }

    if ($this->expirydate === NULL) {
      $this->expirydate = userpoints_get_default_expiry_date();
    }

    if ($this->time_stamp === NULL) {
      $this->time_stamp = REQUEST_TIME;
    }
  }

  /**
   * Overrides Entity::defaultUri().
   *
   * @param bool $prefix_only
   *   If only the url prefix (without /view suffix) should be returned. Defaults to FALSE.
   */
  function defaultUri($prefix_only = FALSE) {
    global $user;
    $uri = array(
      // Default path is displaying it below myuserpoints for the current user.
      'path' => 'myuserpoints/transaction/' . $this->txn_id,
      'options' => array(),
    );
    // When viewing the transaction in the admin UI (but not the add points page, as this might be called for generating
    // tokens), use the admin link.
    if (strpos($_GET['q'], 'admin/config/people/userpoints') !== FALSE && strpos($_GET['q'], 'admin/config/people/userpoints/add') === FALSE ) {
      $uri['path'] = "admin/config/people/userpoints/transaction/$this->txn_id";
    }
    elseif ($this->uid != $user->uid) {
      // When not in the admin ui but viewing the transaction of someone else, use the path for another user.
      $uri['path'] = "user/$this->uid/points/$this->txn_id";
    }

    if (!$prefix_only) {
      $uri['path'] .= '/view';
    }

    return $uri;
  }

  /**
   * Marks this transaction as aborted.
   */
  function abort() {
    $this->aborted = TRUE;
  }

  /**
   * Checks if this transaction is aborted.
   */
  function isAborted() {
    return $this->aborted;
  }

  /**
   * Define the referenced entity.
   *
   * @param $entity_type
   *   Entity type that should be referenced.
   * @param $entity_id
   *   Id of the referenced entity.
   *
   * @return UserpointsTransaction
   */
  function setEntity($entity_type, $entity_id) {
    $this->checkChange();

    // Ignore empty values.
    if (empty($entity_type) || empty($entity_id)) {
      return $this;
    }

    $this->entity_type = $entity_type;
    $this->entity_id = $entity_id;
    return $this;
  }

  /**
   * Add a free reference text to this transaction.
   *
   * @param $reference
   *   A string that serves as an internal reference for this transaction.
   *
   * @return UserpointsTransaction
   */
  function setReference($reference) {
    $this->checkChange();

    $this->reference = $reference;
    return $this;
  }

  /**
   * Add a description to this transaction.
   *
   * Note that modules should instead implement hook_userpoints_info() and
   * provide a description for their operations. If a description is present, it
   * will be displayed instead of a description provided through the mentioned
   * hook.
   *
   * @param $description
   *   A description for this transaction.
   *
   * @return UserpointsTransaction
   */
  function setDescription($description) {
    $this->checkChange();

    $this->description = $description;
    return $this;
  }

  /**
   * Set the status for a transaction.
   *
   * There are helper functions available to set the status of a transaction to
   * a specific status, e. g. UserpointsTransaction::pending(). It is
   * recommended to use these instead.
   *
   * @param $status
   *   One of the following constants: UserpointsTransaction::STATUS_APPROVED,
   *   UserpointsTransaction::STATUS_DECLINED,
   *   UserpointsTransaction::STATUS_PENDING.
   *
   * @return UserpointsTransaction
   *
   * @see UserpointsTransaction::pending()
   * @see UserpointsTransaction::approve()
   * @see UserpointsTransaction::decline()
   *
   */
  function setStatus($status) {
    $this->checkChange();

    // Check allowed values.
    if (!in_array($status, array(UserpointsTransaction::STATUS_APPROVED, UserpointsTransaction::STATUS_DECLINED, UserpointsTransaction::STATUS_PENDING))) {
      $this->abort();
      throw new UserpointsChangeException(t('Invalid status'));
    }

    if ($this->txn_id > 0) {
      // Preserve the original status to be able to check if changes in this
      // transaction are still allowed.
      $this->orig_status = $this->getStatus();
    }

    $this->status = $status;
    return $this;
  }

  /**
   * Set the expiration date of a transaction.
   *
   * Setting it to a date in the past will immediatly expire the transaction.
   *
   * @param $expirydate
   *   Timestamp of the expiration date.
   *
   * @return UserpointsTransaction
   */
  function setExpiryDate($expirydate) {
    $this->checkChange();

    if ($expirydate > 0 || $expirydate === 0 || $expirydate === NULL) {
      $this->expirydate = (int)$expirydate;
    }
    else {
      $this->abort();
      throw new UserpointsInvalidArgumentException(t('Expiration date must be an integer'));
    }
    return $this;
  }

  /**
   * Marks a transaction as expired.
   *
   * This does not affect the points total, instead, a reverting transaction
   * must be created, see userpoints_expire_transactions().
   *
   * @param $expired
   *   TRUE if the transaction should be marked as expired, FALSE if not.
   *
   * @return UserpointsTransaction
   */
  function setExpired($expired) {

    // A transaction can always be expired but this can not be reversed.
    if (!$expired && $this->expired) {
      $this->checkChange();
    }
    $this->expired = $expired;
    return $this;
  }

  /**
   * The user id of the user to which this transaction belongs.
   *
   * @param $uid
   *   The user id.
   *
   * @return UserpointsTransaction
   */
  function setUid($uid) {
    $this->checkChange();
    $this->uid = $uid;
    return $this;
  }

  /**
   * Set the user who approved this transaction.
   *
   * @param $uid
   *   The user id of the approver.
   *
   * @return UserpointsTransaction
   */
  function setApproverUid($uid) {
    $this->checkChange();
    $this->approver_uid = (int)$uid;
    return $this;
  }

  /**
   * Define the points amount of this transaction, which can be any positive
   * or negative amount but not 0.
   *
   * @param $points
   *   The points as an integer.
   *
   * @return UserpointsTransaction
   */
  function setPoints($points) {
    $this->checkChange();

    // Empty points amount is not allowed.
    if (empty($points)) {
      $this->abort();
      throw new UserpointsInvalidArgumentException();
    }

    $this->points = $points;
    return $this;
  }

  /**
   * Set the creation date of this transaction.
   *
   * This can only be set if the userpoints_transaction_timestamp variable is
   * set to false. If that is set to true, the current timestamp is always
   * enforced.
   *
   * @param $time_stamp
   *   The timestamp of the transaction.
   *
   * @return UserpointsTransaction
   */
  function setTimestamp($time_stamp) {
    $this->checkChange(TRUE);

    if (variable_get('userpoints_transaction_timestamp', 1)) {
      return $this;
    }
    $this->time_stamp = $time_stamp;
    return $this;
  }

  /**
   * Define a parent transaction for this.
   *
   * For example, when expiring another transaction, this allows to add a
   * reference to the expired transaction.
   *
   * @param $txn_id
   *   The transaction id of the referenced transaction.
   *
   * @return UserpointsTransaction
   */
  function setParent($txn_id) {
    $this->checkChange();

    $this->parent_txn_id = $txn_id;
    return $this;
  }

  /**
   * Set the category (term tid) of this transaction.
   *
   * @param $tid
   *   The tid, a term id.
   *
   * @return UserpointsTransaction
   */
  function setTid($tid) {
    $this->checkChange();
    $this->tid = $tid;
    return $this;
  }

  /**
   * Set the operation string for this transaction.
   *
   * A string that can identify this transaction. Can be used to provide a
   * custom, translatable, optionally dynamic reason for this transaction in
   * transaction listings. See hook_userpoints_info().
   *
   * This typically indicates the reason for this transaction, e.g. the user
   * commented, voted, logged in etc.
   *
   * This should be understood as a machine name, e.g. mymodule_category_action.
   *
   * @param $operation
   *   A string to identify this type of transaction.
   *
   * @return UserpointsTransaction
   */
  function setOperation($operation) {
    $this->checkChange();

    $this->operation = $operation;
    return $this;
  }

  /**
   * Define if a message should be displayed to the user about this transaction.
   *
   * This can also be overriden by the userpoints_display_message setting. If
   * that setting is disabled, messages are never displayed.
   *
   * @param $display
   *   TRUE if a message should be displayed, FALSE if not. Defaults to TRUE.
   *
   * @return UserpointsTransaction
   */
  function setDisplay($display) {
    $this->display = $display;
    return $this;
  }

  /**
   * Get the referenced entity, if any.
   *
   * @return
   *   An entity object or NULL.
   */
  function getEntity() {
    if (!empty($this->entity_id) && !empty($this->entity_type) && entity_get_info($this->entity_type)) {
      // Create an array because array_shift passes in by reference.
      $entities = entity_load($this->entity_type, array($this->entity_id));
      return array_shift($entities);
    }
  }

  /**
   * Get the referenced entity type, if any.
   *
   * @return
   *   The entity type as a string.
   */
  function getEntityType() {
    return $this->entity_type;
  }

  /**
   * Get the referenced entity id, if any.
   *
   * @return
   *   The entity id as an integer.
   */
  function getEntityId() {
    return $this->entity_id;
  }

  /**
   * The reference string of this transaction, if defined.
   *
   * @return
   *   A reference string or NULL.
   *
   * @see UserpointsTransaction::setReference()
   */
  function getReference() {
    return $this->reference;
  }

  /**
   * The description string of this transaction, if defined.
   *
   * @return
   *   A description string or NULL.
   *
   * @see UserpointsTransaction::setDescription()
   */
  function getDescription() {
    return $this->description;
  }

  /**
   * The status of this transaction.
   *
   * There are helper functions available to check if the transaction has a
   * specific status, e. g. UserpointsTransaction::isPending(). Considering
   * using these if possible.
   *
   * @return
   *   The status of this transaction (approved, declined, pending).
   *
   * @see UserpointsTransaction::setStatus()
   * @see UserpointsTransaction::isPending()
   * @see UserpointsTransaction::isApproved()
   * @see UserpointsTransaction::isDeclined()
   */
  function getStatus() {
    return $this->status;
  }

  /**
   * The expiration date of this transaction, if defined.
   *
   * @return
   *   The expiration date as timestamp or NULL.
   *
   * @see UserpointsTransaction::setExpiryDate()
   */
  function getExpiryDate() {
    return $this->expirydate;
  }

  /**
   * Returns if the transaction is expired or not.
   *
   * @return
   *   TRUE if the transaction is expired, FALSE if not.
   *
   * @see UserpointsTransaction::setExpired()
   */
  function isExpired() {
    return $this->expired;
  }

  /**
   * Returns the UID of the user this transaction belongs to.
   *
   * @return
   *   The uid of the user.
   *
   * @see UserpointsTransaction::setUid()
   */
  function getUid() {
    return $this->uid;
  }

  /**
   * Returns the user object this transaction belongs to.
   *
   * @return
   *   loaded user object for the user this transaction belongs to.
   *
   * @see UserpointsTransaction::setUid()
   * @see UserpointsTransaction::getUid()
   */
  function getUser() {
    return user_load($this->uid);
  }

  /**
   * Returns the uid of the user who approved this transaction.
   *
   * @return
   *   The approver uid.
   *
   * @see UserpointsTransaction::setApproverUid()
   */
  function getApproverUid() {
    return $this->approver_uid;
  }

  /**
   * The loaded user object of the user who approved this transaction.
   *
   * @return
   *   User object.
   *
   * @see UserpointsTransaction::setApproverUid()
   * @see UserpointsTransaction::getApproverUid()
   */
  function getApprover() {
    return user_load($this->approver_uid);
  }

  /**
   * The amount of points of this transaction.
   *
   * @return
   *   Points as an integer.
   *
   * @see UserpointsTransaction::setPoints()
   */
  function getPoints() {
    return $this->points;
  }

  /**
   * The timestamp of when this transaction was created.
   *
   * @return
   *   Unix timestamp of the creation date.
   *
   * @see UserpointsTransaction::setTimestamp()
   */
  function getTimestamp() {
    return $this->time_stamp;
  }

  /**
   * The timestamp of when this transaction last changed.
   *
   * @return
   *   Unix timestamp of the changed date.
   */
  function getChanged() {
    return $this->changed;
  }

  /**
   * Returns the parent transaction if there is any.
   *
   * @return UserpointsTransaction
   *   A userpoints transaction or NULL.
   *
   * @see UserpointsTransaction::setParent()
   */
  function getParent() {
    if (!empty($this->parent_txn_id)) {
      return userpoints_transaction_load($this->parent_txn_id);
    }
  }

  /**
   * The category id (term id) this transaction belongs to.
   *
   * Use UserpointsTransaction::getCategory() to get the name of the category.
   *
   * @return
   *   Term Id of this transaction.
   *
   * @see UserpointsTransaction::setTid()
   * @see UserpointsTransaction::getCategory()
   */
  function getTid() {
    return $this->tid;
  }

  /**
   * The operation of this transaction.
   *
   * @return
   *   The operation string of this transaction.
   *
   * @see UserpointsTransaction::setOperation()
   */
  function getOperation() {
    return $this->operation;
  }

  /**
   * The transaction id of this transaction.
   *
   * @return
   *   The id of this transaction as an integer. NULL if this transaction has
   *   not yet been saved.
   */
  function getTxnId() {
    return $this->txn_id;
  }

  /**
   * Check if a message about this transaction should be displayed.
   *
   * @return
   *   TRUE if a message should be displayed, FALSE otherwise.
   *
   * @see UserpointsTransaction::setDisplay()
   */
  function getDisplay() {
    return $this->display;
  }

  /**
   * The category of this transaction.
   *
   * @return
   *   The name of the category as a string. Name of th default category the
   *   term of this category has been deleted.
   *
   * @see UserpointsTransaction::setTid()
   * @see UserpointsTransaction::getTid()
   */
  function getCategory() {
    // Load categories.
    $categories = userpoints_get_categories();
    return isset($categories[$this->getTid()]) ? $categories[$this->getTid()] : $categories[userpoints_get_default_tid()];
  }

  /**
   * Mark this transaction as pending.
   *
   * @see UserpointsTransaction::setStatus
   */
  function pending() {
    $this->setStatus(UserpointsTransaction::STATUS_PENDING);
    return $this;
  }

  /**
   * Mark this transaction as approved.
   *
   * @see UserpointsTransaction::setStatus
   */
  function approve() {
    $this->setStatus(UserpointsTransaction::STATUS_APPROVED);
    return $this;
  }

  /**
   * Mark this transaction as declined.
   *
   * @see UserpointsTransaction::setStatus
   */
  function decline() {
    $this->setStatus(UserpointsTransaction::STATUS_DECLINED);
    return $this;
  }

  /**
   * Check if this transaction is pending.
   *
   * @see UserpointsTransaction::getStatus
   */
  function isPending() {
    return $this->getStatus() == UserpointsTransaction::STATUS_PENDING;
  }

  /**
   * Check if this transaction is declined.
   *
   * @see UserpointsTransaction::getStatus
   */
  function isDeclined() {
    return $this->getStatus() == UserpointsTransaction::STATUS_DECLINED;
  }

  /**
   * Check if this transaction is approved.
   *
   * @see UserpointsTransaction::getStatus
   */
  function isApproved() {
    return $this->getStatus() == UserpointsTransaction::STATUS_APPROVED;
  }

  /**
   * Checks if a change is allowed.
   *
   * @param $only_new
   *   If TRUE, only allows changes if this transaction is new. Defaults to
   *   FALSE.
   *
   * @throws UserpointsChangeException
   */
  protected function checkChange($only_new = FALSE) {
    if ($this->isReadOnly($only_new)) {
      $this->abort();
      throw new UserpointsChangeException(t('This transaction is saved and approved or declined and can not be changed.'));
    }
  }

  /**
   * Checks if a change is allowed.
   *
   * Once a transaction is saved and either approved or declined, no alterations
   * of the data is allowed except marking it as expired.
   *
   * @param $only_new
   *   If TRUE, only allows changes if this transaction is new. Defaults to
   *   FALSE.
   *
   * @return
   *   TRUE if changes are allowed, FALSE if not.
   */
  function isReadOnly($only_new = FALSE) {
    if (!empty($this->txn_id)) {
      if ($only_new) {
        return TRUE;
      }
      if ($this->orig_status !== NULL && $this->orig_status <> UserpointsTransaction::STATUS_PENDING) {
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Deny this transaction from being saved.
   *
   * This is typically called in hook_userpoints_transaction_before().
   *
   * @see UserpointsTransaction::isDenied()
   * @see UserpointsTransaction::getDenyReasons()
   */
  function deny($reason) {
    $this->denied_reasons[] = $reason;
  }

  /**
   * Check if this transaction is denied.
   *
   * A transaction is denied if there are any deny reasons.
   *
   * @see UserpointsTransaction::deny().
   * @see UserpointsTransaction::getDenyReasons()
   */
  function isDenied() {
    return !empty($this->denied_reasons);
  }

  /**
   * Returns the deny reasons for this transaction.
   *
   * @return
   *   An array with the reasons why this transaction was denied.
   *
   * @see UserpointsTransaction::deny()
   * @see UserpointsTransaction::isDenied()
   * @see UserpointsTransaction::getDenyReasons()
   */
  function getDenyReasons() {
    return $this->denied_reasons;
  }

  /**
   * Override the generated default message of this transaction.
   *
   * @param $message
   *   The message that should be displayed if configured to do so.
   *
   * @return UserpointsTransaction
   *
   * @see UserpointsTransaction::getMessage()
   * @see UserpointsTransaction::setDisplay()
   */
  function setMessage($message) {
    $this->message = $message;
    return $this;
  }

  /**
   * A message that can be displayed to the current user.
   *
   * If set, the message defined by UserpointsTransaction::setMessage() is used.
   * Otherwise, a message is displayed that takes into account the points amount
   * (negative or positive), the category, the status and if the transaction is
   * for the currently logged in user or not.
   *
   * @return
   *   A message string that describes this transaction to the currently logged
   *   in user. Can be empty if not automated message could have been generated.
   *
   * @see UserpointsTransaction::setMessage().
   */
  function getMessage() {
    global $user;

    // If set, use the overriden message.
    if (!empty($this->message)) {
      return $this->message;
    }
    // Prepare arguments. They are the same for all string combinations.
    $categories = userpoints_get_categories();
    $total_points = userpoints_get_current_points($this->getUid(), $this->getTid());
    $arguments = array_merge(userpoints_translation(), array(
      '!username' => theme('username', array('account' => $this->getUser())),
      '%total' => theme('userpoints_points', array('points' => $total_points)),
      '%category' => $this->getCategory(),
    ));

    $view_own_points = user_access('view own userpoints') || user_access('view userpoints') || user_access('administer userpoints');
    $view_all_points = user_access('view userpoints') || user_access('administer userpoints');

    $points = theme('userpoints_points', array('points' => $this->getPoints()));
    $absolute_points = theme('userpoints_points', array('points' => $this->getPoints()));

    $message = NULL;
    if ($this->isDeclined()) {
      // Points have been declined.
      if ($this->getUid() == $user->uid && $view_own_points) {
        $message = format_plural($points, 'You did not receive approval for @count !point in the %category category.', 'You did not receive approval for @count !points in the %category category.', $arguments);
      }
      elseif ($view_all_points) {
        $message = format_plural($points, '!username did not receive approval for @count !point in the %category category.', '!username did not receive approval for @count !points in the %category category.', $arguments);
      }
    }
    elseif ($this->getPoints() < 0) {
      if ($this->isPending()) {
        if ($this->getUid() == $user->uid && $view_own_points) {
          // Directly address the user if he is loosing points.
          $message = format_plural($absolute_points, 'You just had a !point deducted, pending administrator approval.', 'You just had @count !points deducted, pending administrator approval.', $arguments);
        }
        elseif ($view_all_points) {
          // Only display message about other users if user has permission to view userpoints.
          $message = format_plural($absolute_points, '!username just had a !point deducted, pending administrator approval.', '!username just had @count !points deducted, pending administrator approval.', $arguments);
        }
      }
      else {
        if ($this->getUid() == $user->uid && $view_own_points) {
          $message = format_plural($absolute_points, 'You just had a !point deducted and now have %total !points in the %category category.', 'You just had @count !points deducted and now have %total !points in the %category category.', $arguments);
        }
        elseif ($view_all_points) {
          $message = format_plural($absolute_points, '!username just had a !point deducted and now has %total !points in the %category category.', '!username just had @count !points deducted and now has %total !points in the %category category.', $arguments);
        }
      }
    }
    else {
      if ($this->isPending()) {
        if ($this->getUid() == $user->uid && $view_own_points) {
          // Directly address the user if he is loosing points.
          $message = format_plural($absolute_points, 'You just earned a !point, pending administrator approval.', 'You just earned @count !points, pending administrator approval.', $arguments);
        }
        elseif ($view_all_points) {
          // Only display message about other users if user has permission to view userpoints.
          $message = format_plural($absolute_points, '!username just earned a !point, pending administrator approval.', '!username just earned @count !points, pending administrator approval.', $arguments);
        }
      }
      else {
        if ($this->getUid() == $user->uid && $view_own_points) {
          $message = format_plural($absolute_points, 'You just earned a !point and now have %total !points in the %category category.', 'You just earned @count !points and now have %total !points in the %category category.', $arguments);
        }
        elseif ($view_all_points) {
          $message = format_plural($absolute_points, '!username just earned a !point and now has %total !points in the %category category.', '!username just earned @count !points and now has %total !points in the %category category.', $arguments);
        }
      }
    }
    return $message;
  }

  /**
   * Returns additional information about the operation of this transaction.
   *
   * @return
   *   Information about this operation as an array.
   *
   * @see userpoints_get_info()
   */
  function getOperationInfo() {
    return userpoints_get_info($this->getOperation());
  }

  /**
   * Returns a descriptive reason for this transaction.
   *
   * The following resources are considered, in this order:
   *
   *  * description key in the information array for that operation.
   *  * description of the transaction.
   *  * name of the operation.
   *
   * @param $options
   *   Array of options:
   *   - link: If FALSE, no link is generated to the linked entity even if there
   *     were one. Defaults to TRUE.
   *   - truncate: Define if the reason should be truncated. Defaults to TRUE.
   *   - skip_description: Allows to skip the eventually existing custom
   *     description a transaction has and always use the generated description.
   *
   * @return
   *   The reason for that transaction, linked to the referenced entity if
   *   available.
   */
  function getReason(array $options = array()) {

    // Default options.
    $options += array(
      'link' => TRUE,
      'truncate' => TRUE,
    );

    $safe = FALSE;
    // Check transaction description first to allow custom overrides.
    if (empty($options['skip_description']) && $description = $this->getDescription()) {
      $reason = $description;
    }
    else {
      $info = $this->getOperationInfo();
      // Check if there is a valid description callback defined for this
      // operation.
      if (!empty($info['description callback']) && function_exists($info['description callback'])) {
        $reason = $info['description callback']($this, $this->getEntity());
        $safe = TRUE;
      }
      // Try static description key.
      elseif (!empty($info['description'])) {
        $reason = $info['description'];
        $safe = TRUE;
      }
    }
    // Fallback to the operation name if there is no source.
    if (empty($reason)) {
      $reason = $this->getOperation();
    }

    // Truncate description.
    $attributes = array();
    $stripped_reason = strip_tags($reason);
    if ($options['truncate'] && drupal_strlen($stripped_reason) > variable_get('userpoints_truncate', 30) + 3) {
      // The title attribute will be check_plain()'d again drupal_attributes(),
      // avoid double escaping.
      $attributes['title'] = html_entity_decode($stripped_reason, ENT_QUOTES);
      $reason = truncate_utf8($stripped_reason, variable_get('userpoints_truncate', 30), FALSE, TRUE);
    }

    // Link to the referenced entity, if available.
    if ($this->getEntity() && $options['link']) {
      $uri = entity_uri($this->getEntityType(), $this->getEntity());
      if ($uri) {
        $reason = l($reason, $uri['path'], $uri['options'] + array('html' => $safe, 'attributes' => $attributes));
      }
    }
    if ((!$this->getEntity() || empty($uri)) && !$safe) {
      // Escape possible user provided reason.
      $reason = check_plain($reason);
    }
    return $reason;
  }

  /**
   * Returns a list of operations as links.
   *
   * @param $show_view
   *   FALSE if the view link should not be displayed. Defaults to TRUE.
   *
   * @return
   *   A string with operation links.
   */
  function getActions($show_view = TRUE) {
    $actions = array();
    $url_options = array('query' => drupal_get_destination());

    $uri = $this->defaultUri(TRUE);
    $url_prefix = $uri['path'];

    if ($show_view && userpoints_access_view_transaction($this)) {
      $actions[] = l(t('view'), $url_prefix . '/view');
    }
    if (userpoints_admin_access('edit')) {
      $actions[] = l(t('edit'), $url_prefix . '/edit', $url_options);
    }
    if (userpoints_admin_access('moderate') && $this->isPending()) {
      $actions[] = l(t('approve'), $url_prefix . '/approve', $url_options);
      $actions[] = l(t('decline'), $url_prefix . '/decline', $url_options);
    }
    return implode(' ', $actions);
  }

  /**
   * Returns a single row for a transaction listing.
   *
   * @param $settings
   *   Array with settings about which column shall be displayed. All settings
   *   default to TRUE.
   *   - show_category, show category column.
   *   - show_user, show user column.
   *   - show_status, show status column.
   *
   * @return
   *   A table row array for use with theme_table().
   */
  function getTableRow($settings = array()) {
    $settings += array(
      'show_user' => TRUE,
      'show_status' => TRUE,
    );

    $stati = userpoints_txn_status();
    $css_stati = array(
      UserpointsTransaction::STATUS_APPROVED => 'approved',
      UserpointsTransaction::STATUS_DECLINED => 'declined',
      UserpointsTransaction::STATUS_PENDING => 'pending',
    );
    $row = array('class' => array(
      'userpoints-transaction-row-status-' . $css_stati[$this->getStatus()],
      'userpoints-transaction-row-category-' . $this->getTid()),
    );
    if ($settings['show_user']) {
      $row['data'][] = array(
        'data' => theme('username', array('account' => $this->getUser())),
        'class' => array('userpoints-transactions-field-user'),
      );
    }
    $row['data'][] = array(
      'data' => theme('userpoints_points', array('points' => $this->getPoints())),
      'class' => array('userpoints-transactions-field-points', 'userpoints-transaction-points-' . ($this->getPoints() > 0 ? 'positive' : 'negative')),
    );

    $categories = userpoints_get_categories();
    if (count($categories) > 1) {
      $row['data'][] = array(
        'data' => $this->getCategory(),
        'class' => array('userpoints-transactions-field-category'),
      );
    }
    $row['data'][] = array(
      'data' => format_date($this->getTimestamp(), 'small'),
      'class' => array('userpoints-transactions-field-timestamp'),
    );
    $row['data'][] = array(
      'data' => $this->getReason(),
      'class' => array('userpoints-transactions-field-reason'),
    );
    if ($settings['show_status']) {
      $row['data'][] = array(
        'data' => $stati[$this->getStatus()],
        'class' => array('userpoints-transactions-field-status'),
      );
    }
    $row['data'][] = array(
      'data' => $this->getActions(),
      'class' => array('userpoints-transactions-field-actions'),
    );
    return $row;
  }

  /**
   * Resets the original status to the current one.
   */
  public function resetOriginalStatus() {
    $this->orig_status = $this->getStatus();
  }
}

/**
 * This exception is thrown when a property is changed after a saved transaction
 * has been approved or declined.
 */
class UserpointsChangeException extends Exception {

}

/**
 * This exception is thrown when trying to access an unknown property through
 * the magic UserpointsTransaction::__get() method.
 */
class UserpointsInvalidPropertyException extends Exception {
  function __construct($name, $code = NULL, $previous = NULL) {
    parent::__construct(t('Userpoints transaction does not have a @property property.', array('@property' => $name)), $code, $previous);
  }
}

/**
 * Thrown when trying to set a property to an invalid value.
 */
class UserpointsInvalidArgumentException extends Exception {

}

/**
 * Thrown when trying to save a transaction without points, uid or operation.
 */
class UserpointsTransactionIncompleteException extends Exception {

}

/**
 * Userpoints transaction controller.
 */
class UserpointsTransactionController extends EntityAPIController {

  /**
   * Overrides EntityAPIController::save().
   *
   * It is not permitted to update a approved or denied transaction except
   * marking it as expird. Any attemt to change a property of such a transaction
   * will result in an immediate exception.
   */
  public function save($entity, DatabaseTransaction $transaction = NULL) {

    // Prevent saving when any of the required properties are missing.
    if (!$entity->getPoints() || !$entity->getUid() || !$entity->getOperation()) {
      $entity->abort();
      throw new UserpointsTransactionIncompleteException();
    }

    // Call the before hook to allow modules to change and deny this.
    // @todo: Rename this hook?
    module_invoke_all('userpoints_transaction_before', $entity);

    // Abort if the transaction has been denied.
    if ($entity->isDenied()) {
      $entity->abort();
      return FALSE;
    }

    $return = parent::save($entity, $transaction);

    // Update totals if the transaction is approved and not expired.
    if ($entity->isApproved() && !$entity->isExpired()) {
      $this->updateTotals($entity->tid, $entity->uid, $entity->points);
    }

    // Display a message unless disabled or no message exists.
    if ($entity->getDisplay() && $message = $entity->getMessage()) {
      drupal_set_message($message);
    }

    // Reset original status to current one
    $entity->resetOriginalStatus();
    return $return;
  }


  /**
   * Update the total aggregations of the corresponding user.
   */
  protected function updateTotals($tid, $uid, $points) {
    // Update this category.
    $this->updateTotalsCategory($tid, $uid, $points);
    // Update the total over all categories.
    $this->updateTotalsCategory('all', $uid, $points);
  }

  /**
   * Update the totals of a specific category.
   *
   * Updating the total of all categories is supported by using 'all' for the
   * $tid.
   */
  protected function updateTotalsCategory($tid, $uid, $points) {
    $table = 'userpoints';
    if ($tid === 'all') {
      // Use a different table for the overall total.
      $table = 'userpoints_total';
    }

    // Always update the time stamp and the total points.
    $total = array(
      'last_update' => REQUEST_TIME,
      'points' => $points + userpoints_get_current_points($uid, $tid),
    );
    // Update the total max points if necessary.
    $max_points_total = userpoints_get_max_points($uid, $tid);
    if ($total['points'] > $max_points_total) {
      $total['max_points'] = $total['points'];
    }

    // The keys for the merge query. The tid is only added when not 'all'.
    $keys = array(
      'uid' => $uid,
    );
    if ($tid !== 'all') {
      $keys['tid'] = $tid;
    }

    // Save the updates.
    db_merge($table)
      ->key($keys)
      ->fields($total)
      ->execute();
  }

}

/**
 * Extends property metadata for the Userpoints Transaction entity.
 */
class UserpointsTransactionMetadataController extends EntityDefaultMetadataController {

  /**
   * Overrides EntityDefaultMetadataController::entityPropertyInfo().
   */
  public function entityPropertyInfo() {
    $info = parent::entityPropertyInfo();

    $properties = &$info['userpoints_transaction']['properties'];
    $properties['user'] = array(
      'type' => 'user',
      'label' => t('User'),
      'description' => t('The user that will receive the !points', userpoints_translation()),
      'setter callback' => 'userpoints_transaction_property_set',
      'getter callback' => 'userpoints_transaction_property_get',
      'schema field' => 'uid'
    );
    $properties['points']['label'] = t('!Points', userpoints_translation());
    $properties['points']['description'] = t('Amount of !points to give or take.', userpoints_translation());

    $properties['type'] = array(
      'label' => t('!Points transaction type', userpoints_translation()),
      'description' => t('The bundle entity for !points transactions.', userpoints_translation()),
      'type' => 'userpoints_transaction_type',
    );

    $properties['points_abs'] = array(
      'label' => t('!Points absolute', userpoints_translation()),
      'description' => t('The absolute (positive) amount of !points of this transaction.', userpoints_translation()),
      'type' => 'integer',
      'getter callback' => 'userpoints_transaction_get_points_absolute',
    );

    $properties['tid'] = array(
      'label' => t('!Points category', userpoints_translation()),
      'description' => t('The category to which these transaction belongs.'),
      'options list' => 'userpoints_rules_get_categories',
    ) + $properties['tid'];

    $properties['entity'] = array(
      'label' => t('Entity'),
      'type' => 'entity',
      'description' => t('The entity to which this transaction refers.'),
      'optional' => TRUE,
      'getter callback' => 'userpoints_transaction_property_get',
    );
    $properties['description'] = array(
      'label' => t('Description'),
      'description' => t('Can contain the reason why the points have been given.'),
      'optional' => TRUE,
    ) + $properties['description'];

    $properties['reference'] = array(
      'label' => t('Reference'),
      'description' => t('Can contain a reference for this transaction.'),
      'optional' => TRUE,
    ) + $properties['reference'];

    $properties['operation'] = array(
      'label' => t('Operation'),
      'description' => t('Describes the operation (Insert/Remove/...).'),
    ) + $properties['operation'];

    $properties['reason'] = array(
      'label' => t('Reason'),
      'type' => 'text',
      'description' => t('The reason why the points were granted.'),
      'getter callback' => 'userpoints_transaction_property_get',
    );
    $properties['parent_txn_id'] = array(
      'label' => t('Parent transaction'),
      'description' => t('ID of the parent transaction.'),
    ) + $properties['parent_txn_id'];

    $properties['entity_id'] = array(
      'label' => t('Entity ID'),
      'description' => t('Id of the referenced entity.'),
    ) + $properties['entity_id'];

    $properties['entity_type'] = array(
      'label' => t('Entity type'),
      'description' => t('Type of the referenced entity.'),
    ) + $properties['entity_type'];

    $properties['uid'] = array(
      'label' => t('User ID'),
      'description' => t('ID of the user who received the point.'),
    ) + $properties['uid'];

    $properties['approver_uid'] = array(
      'label' => t('Approver UID'),
      'description' => t('User ID of the user who approved the transaction.'),
    ) + $properties['approver_uid'];

    $properties['time_stamp'] = array(
      'label' => t('Timestamp'),
      'type' => 'date',
      'description' => t('Time when the points were given.'),
    ) + $properties['time_stamp'];

    $properties['changed'] = array(
      'label' => t('Changed'),
      'type' => 'date',
      'description' => t('Time when the transaction was last changed.'),
    ) + $properties['time_stamp'];

    $properties['expired'] = array(
      'label' => t('Expired'),
      'type' => 'boolean',
      'description' => t('If the transaction is expired.'),
    ) + $properties['expired'];

    $properties['expirydate'] = array(
      'label' => t('Expiry date'),
      'type' => 'date',
      'description' => t('Time when the points will expire.'),
    ) + $properties['expirydate'];

    $properties['display'] = array(
      'label' => t('Display'),
      'type' => 'boolean',
      'description' => t('Whether to show a message to the user for this transaction or not.'),
      'setter callback' => 'userpoints_transaction_property_set',
      'getter callback' => 'userpoints_transaction_property_get',
    );
    $properties['status'] = array(
      'label' => t('Status'),
      'description' => t('Status of this transaction.'),
      'options list' => 'userpoints_txn_status',
    ) + $properties['status'];

    // Add default setter callback.
    foreach ($properties as $key => &$property) {
      if (isset($property['setter callback'])) {
        continue;
      }

      // Exclude some read only properties.
      if (in_array($key, array('display', 'reason', 'entity', 'points_abs'))) {
        continue;
      }

      $property['setter callback'] = 'entity_property_verbatim_set';
    }

    return $info;
  }
}

/**
 * Transaction type.
 *
 * @ingroup userpoints_api
 */
class UserpointsTransactionType extends Entity {

  /**
   * Label of the transaction type.
   *
   * @var string
   */
  public $label;

  /**
   * Machine name of the transaction type.
   *
   * @var string
   */
  public $name;

  function __construct($values = array()) {
    parent::__construct($values, 'userpoints_transaction_type');
  }

}

/**
 * Transaction type UI.
 *
 * @ingroup userpoints_api
 */
class UserpointsTransactionTypeUIController extends EntityDefaultUIController {

  /**
   * Implements EntityDefaultUIController::hook_menu().
   *
   * Make transaction type UI fit into userpoints menu structure.
   */
  public function hook_menu() {
    $items = parent::hook_menu();
    $items[$this->path]['title'] = 'Transaction Types';
    $items[$this->path]['description'] = strtr('Manage !Points transaction types.', userpoints_translation());
    $items[$this->path]['type'] = MENU_LOCAL_TASK;
    $items[$this->path]['weight'] = 10;

    foreach ($items as $path => $item) {
      if (substr($path, strlen($this->path) + 1, 6) == 'manage') {
        // Field UI doesn't render local tasks well when the main ui is a local
        // task itself. This places all admin interfaces directly under types
        // instead.
        $new_path = preg_replace('/\/manage/', '', $path);
        $items[$new_path] = $items[$path];
        $items[$new_path]['page arguments'][1] = 5;

        if (isset($items[$new_path]['page arguments'][0]) &&
            $items[$new_path]['page arguments'][0] == 'userpoints_transaction_type_operation_form') {
          // Entity API fails to use the correct bundle argument defined in
          // entity info.
          $items[$new_path]['page arguments'] = array('entity_ui_operation_form', 'userpoints_transaction_type', 5, 6);
        }

        unset($items[$path]);
      }
    }
    return $items;
  }

  /**
   * Implements EntityDefaultUIController::overviewTableRow().
   *
   * This is pretty much a straight rip from Entity API except the links are
   * generated without 'manage' in the path.
   */
  protected function overviewTableRow($conditions, $id, $entity, $additional_cols = array()) {
    $entity_uri = entity_uri($this->entityType, $entity);

    $row[] = array(
      'data' => array(
        '#theme' => 'entity_ui_overview_item',
        '#label' => entity_label($this->entityType, $entity),
        '#name' => !empty($this->entityInfo['exportable']) ? entity_id($this->entityType, $entity) : FALSE,
        '#url' => $entity_uri ? $entity_uri : FALSE,
        '#entity_type' => $this->entityType,
      ),
    );

    // Add in any passed additional cols.
    foreach ($additional_cols as $col) {
      $row[] = $col;
    }

    // Add a row for the exportable status.
    if (!empty($this->entityInfo['exportable'])) {
      $row[] = array('data' => array(
          '#theme' => 'entity_status',
          '#status' => $entity->{$this->statusKey},
        ));
    }
    // In case this is a bundle, we add links to the field ui tabs.
    $field_ui = !empty($this->entityInfo['bundle of']) && entity_type_is_fieldable($this->entityInfo['bundle of']) && module_exists('field_ui');
    // For exportable entities we add an export link.
    $exportable = !empty($this->entityInfo['exportable']);
    // If i18n integration is enabled, add a link to the translate tab.
    $i18n = !empty($this->entityInfo['i18n controller class']);

    // Add operations depending on the status.
    if (entity_has_status($this->entityType, $entity, ENTITY_FIXED)) {
      $row[] = array(
        'data' => l(t('clone'), $this->path . '/' . $id . '/clone'),
        'colspan' => $this->operationCount(),
      );
    }
    else {
      $row[] = l(t('edit'), $this->path . '/' . $id);

      if ($field_ui) {
        $row[] = l(t('manage fields'), $this->path . '/' . $id . '/fields');
        $row[] = l(t('manage display'), $this->path . '/' . $id . '/display');
      }
      if ($i18n) {
        $row[] = l(t('translate'), $this->path . '/' . $id . '/translate');
      }
      if ($exportable) {
        $row[] = l(t('clone'), $this->path . '/' . $id . '/clone');
      }

      if (empty($this->entityInfo['exportable']) || !entity_has_status($this->entityType, $entity, ENTITY_IN_CODE)) {
        $row[] = l(t('delete'), $this->path . '/' . $id . '/delete', array('query' => drupal_get_destination()));
      }
      elseif (entity_has_status($this->entityType, $entity, ENTITY_OVERRIDDEN)) {
        $row[] = l(t('revert'), $this->path . '/' . $id . '/revert', array('query' => drupal_get_destination()));
      }
      else {
        $row[] = '';
      }
    }
    if ($exportable) {
      $row[] = l(t('export'), $this->path . '/' . $id . '/export');
    }
    return $row;
  }

}
