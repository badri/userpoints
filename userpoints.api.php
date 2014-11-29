<?php

/**
 * @file
 * API documentation for userpoints.module
 */

/**
 * @mainpage Userpoints API Documentation
 * API Documentation of the Userpoints module.
 *
 * - Topics:
 *  - @link userpoints_api API functions @endlink
 *  - @link userpoints_hooks Provided hooks @endlink
 */

/**
 * @defgroup userpoints_api API functions
 *
 * The function userpoints_grant_points() allows to grant a number of points,
 * positive or negative, to a user.
 */

/**
 * @defgroup userpoints_hooks Provided hooks
 *
 */

/**
 * @addtogroup userpoints_hooks
 * @{
 */

/**
 * Return information about registered operations.
 *
 * Modules can register operation strings
 *
 * @return
 *   An array whose keys are operation strings used in
 *   userpoints_userpoints_api() and the which has the following properties:
 *   - description: A string that is used as a reason when transactions are
 *     displayed. Either this or a callback (see below) is required.
 *   - description callback: If the reason is dynamic, because he for example
 *     includes the title of a node, a callback function can be given which
 *     receives the transaction object and (if existing) and entity object as
 *     arguments.
 *   - admin description: A description which is searched for and displayed in
 *     the operation autocomplete field in the add points form.
 *
 */
function hook_userpoints_info() {
  return array(
    'expiry' => array(
      'description' => t('!Points have expired.', userpoints_translation()),
      'admin description' => t('Expire an existing transaction'),
    )
  );
}

/**
 * Allows to customize the output of a the users by points page.
 *
 * @param $output
 *   Render array with the content.
 *
 * @see userpoints_list_users().
 */
function hook_userpoints_list_alter(&$output) {

}

/**
 * Allows to customize the output of a the my userpoints page.
 *
 * @param $output
 *   Render array with the content.
 *
 * @see userpoints_list_transactions().
 */
function hook_userpoints_list_transactions_alter(&$output) {

}

/**
 * Provide settings for the shared Userpoints administration settings form.
 *
 * @return
 *   A form structure consisting of one or multiple vertical_tabs belonging to
 *   the #group settings_additional.
 */
function hook_userpoints_settings() {
  drupal_add_js(drupal_get_path('module', 'userpoints_nc') . '/userpoints_nc.js');

  $form['userpoints_nc'] = array(
    '#type'        => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed'   => TRUE,
    '#title'       => t('Content'),
    '#group'       => 'settings_additional',
    '#weight'      => 15,
  );

  $form['userpoints_nc']['userpoints_nc_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Enabled by default.'),
    '#default_value' => userpoints_nc_get_setting('enabled', NULL, TRUE),
    '#description' => t('If checked, all content types award !points by default. This can be overridden for each content type on the content type edit page.', userpoints_translation()),
  );
}

/**
 * Invoked before a transaction is saved.
 *
 * To prohibit this transaction from being saved, use
 * UserpointsTransaction::deny().
 *
 * @param $transaction
 *   A UserpointsTransaction object which contains all information about the
 *   new or updated transaction.
 */
function hook_userpoints_transaction_before(UserpointsTransaction $transaction) {

}

/**
 * @}
 */
