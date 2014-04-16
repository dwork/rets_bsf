<?PHP

/**----------------------------------------------------------------------------------
 *  
 * function rets_bsf_gettid ($vname, $name) {  ... return $tid }  
 *       return taxonomy term name 
 *
 * function rets_bsf_ismember ($roles, $row) {  ... return $ismember }
 *       returns "1" if the agent is and OIP member, '' otherwise
 *
 * function rets_bsf_ismember_agent ($roles, $agent) { ... return 'Member' }
 *       returns 'Member' if the agent has a role in the target $roles array
 * 
 * function rets_bsf_loadroles () { ... return $roles }
 *       returns an array of roles for any roles that are OIP member agents
 *
 * function rets_bsf_openrets ($map) { ... return $map }
 *       returns array[$key] => $out, $type, $oiptable, $oipout
 *
 * function rets_bsf_msg($msg) { ... }   
 *       displays a drupal message, which appears on whichever admin screen is being viewed at the time
 *
 * function rets_bsf_mail($key, &$message, $params) {
 *       implements hook_mail for this module
 *
 * function rets_bsf_hotsheet($msg) { .... }
 *       emails $msg to the email address specified in the config
 */

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_gettid
 *  Retrieve the tid given the Vocabulary machine name and TID term name
 */

function rets_bsf_gettid ($vname, $name) {

  $voc = taxonomy_vocabulary_machine_name_load($vname);
  $query = new EntityFieldQuery;
  $result = $query
  ->entityCondition('entity_type', 'taxonomy_term')
  ->propertyCondition('name', $name)
  ->propertyCondition('vid', $voc->vid)
  ->execute();
  $keys = array_keys($result['taxonomy_term']);
  return $keys[0];
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_ismember
 *  Interface only listings that belong to OIP members
 */

function rets_bsf_ismember ($roles, $row) {

  $agent1 = $row['ListAgentID'];
  $agent2 = $row['CoListAgentID'];
  $saleagent = $row['SaleAgentID'];

  $ismember = rets_bsf_ismember_agent ($roles, $agent1);
  if (!$ismember) {
    $ismember = rets_bsf_ismember_agent ($roles, $agent2);
  }
  if (!$ismember) {
    $ismember = rets_bsf_ismember_agent ($roles, $saleagent);
  }
  return $ismember;
}

function rets_bsf_ismember_agent ($roles, $agent) {
     // See if this listing is in the Drupal database
    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', 'user')
      ->propertyCondition('status', '1')
      ->fieldCondition('field_agentid', 'value', $agent, '=')
      ->execute();
   if ($result) {
      $napps = user_load_multiple(array_keys($result['user']));
      $keys = array_keys($result['user']);
      $rkey = $keys[0];
      foreach ($napps[$rkey]->roles as $key => $value) {
        if (isset($roles[$key])) {
            return 'Member:'.$key.':'.$roles[$key];
        }
      }
   }
   return 'No';
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_loadroles
 *  Load roles for realtor members
 */

function rets_bsf_loadroles () {

    $role = array();
    $ary = user_roles();
    foreach ($ary as $key => $value) {
	switch ($value) {
	  case ('basic membership') :
            $role[$key] = 1;
	    break;
	  case ('gold membership') :
	    $role[$key] = 1;
	    break;
          case ('platinum membership') :
            $role[$key] = 1;
	    break;
	}
    }
    return $role;;
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_openrets
 *  Open the rets bsf "map" file and load up all the fields
 */

function rets_bsf_openrets ($map) {

  // Open the rets database
  $host = "athena.earthclick.net";
  $user = "rmls";
  $pw = "me5Y#2#3";
  $db = "rets";

  $mysql_access = mysql_connect($host, $user, $pw);
  if(!$mysql_access) {
     rets_bsf_msg ("Database $db not connected");
  }

  $con = mysql_select_db($db, $mysql_access);
  if (!$con) {
     rets_bsf_msg ("Can't use connection to $db");
  }

  // Read in "rets_map" and load the $map array
  $select = 'SELECT * FROM bsf_map WHERE fieldoutvalue<>""';
  $query = mysql_query($select);
  while ($row = mysql_fetch_array($query)) {
    $fld = chop($row['fieldinvalue']);
    $map[$fld]['out'] = chop($row['fieldoutvalue']);
    $map[$fld]['type'] = $row['fieldtype'];
    $map[$fld]['oiptable'] = $row['oiptable'];
    $map[$fld]['oipfield'] = $row['oipfield'];
  }
  return $map;
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_msg 
 *  Display message and return
 */

function rets_bsf_msg($msg) {
 drupal_set_message(t($msg));
 return;
}

/**----------------------------------------------------------------------------------- 
 * A very simple hook_mail() implementation, for module "mymodule".
 * This implementation expects to receive a $body, $subject, and $headers 
 * inside the $params sent from drupal_mail(), rather than construct
 * those here in the hook.
 * @see drupal_mail()
 */
function rets_bsf_mail($key, &$message, $params) {
  if (isset($params['subject'])) {
    $message['subject'] = $params['subject'];
  } 
  if (isset($params['body'])) {
    $message['body'][] = $params['body']; 
  }
  if (isset($params['headers']) && is_array($params['headers'])) {   
    $message['headers'] += $params['headers'];
  }
  
  // You should really have hook_mail() doing most of the formatting, 
  // rather than pass in a complete $body or a $subject from drupal_mail().  
  // To accomplish that, you'd branch this formatting based on the value of
  // $key sent with drupal_mail(), and using code like this:
  // switch ($key) {
  //  case 'key1':
  //    // do something specific for mails of type key1
  //    break;      
  //  case 'key2':
  //    // do something specific for mails of type key2
  //    break;
  // }
  
  // That's it - You don't do anything in your hook_mail() to actually
  // send the mail, that is taken care of by the mail system automatically. 
}

/****************************************************************************************
 *  rets_bsf_hotsheet
 *  Email hotsheet with changes
 */

function rets_bsf_hotsheet ($msg) {           // $msg is the message to be emailed
global $user;  // place at the top of the function you are calling from

  // Get the hotsheet variables set in rets_bsf.module
  $sendemail = variable_get('rets_bsf_sendhotsheet');

  if ($sendemail == 'Yes') {

	$to = variable_get('rets_bsf_emailto');
 	$from = variable_get('rets_bsf_emailfrom');
	$subject = variable_get('rets_bsf_emailsubject','Hotsheet');

	$to = 'dwork@oip.net';
	$from = 'admin@oip.net';

  	// ... other function code would be here

  	// Send email with $body and $subject constructed here:
  	$body = $msg;

  	// If you are using suhosin (the hardened PHP extension) and want to use
 	// Cc or Bcc, then make sure that suhosin.mail.protect is not set to 2, 
  	// or the mail will be rejected as an attack.
  	$header_bcc = '';
  	$header_cc = '';

  	// you can set 'message_key' to any key you want, and process 
  	// that in the hook_mail as shown in the commented out code.
  	// Or, just set it to some string and ignore it as we are here.
  	// You can add extra headers into the headers array here.  If you
  	// are trying to send an HTML formatted email, please use either
  	// the MIME Mail or HTMLMail modules, rather than doing it
	// by just setting the Content-Type.

  	$message = drupal_mail('rets_bsf', 'hotsheet', $to, user_preferred_language($user), array('body' => $body, 'subject' => $subject, 'headers' => array('Cc' => $header_cc, 'Bcc' => $header_bcc)), $from, TRUE);
  	// You can check $message['result'] to see if the message sent successfully.
  }
}
