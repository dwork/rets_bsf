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
 *  helper functions        
 */

 function rets_show_feature ($entity,$featurename,$feature) {
     $ary = field_fetch_field_values('node',$entity,$feature); if (!empty($ary)) { $feat = $ary[0]; } else { $feat = ''; }
     if (!empty($feat)) { print $featurename.': '.$feat. "<BR>"; }
  }

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
  if ($result) {   
    $keys = array_keys($result['taxonomy_term']);
  } else {
    drupal_set_message(t('Error: rets_bsf_gettid (includes): Taxonomy term not found -- vname: '. $vname .'; name: '.$name));
    return '';
  }
  return $keys[0];
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_ismember
 *  Interface only listings that belong to OIP members
 *  DCW 6/6/2016 Get the team member's info if necessary
 */

function rets_bsf_ismember ($roles, $agent1, $agency1, $agent2, $agency2) {

  $ismember1 = rets_bsf_ismember_agent ($roles, $agent1);
  $ismember2 = rets_bsf_ismember_agent ($roles, $agent2);

  if ($ismember1 == 'No' && $ismember2 == 'No') {
    return 'No';
  }

  // Set the variables for TYPE and UID so we don't error out
  if ($ismember1 == 'No') {
    $type1 = '';
    $uid1 = '';
  } else {
    $type1 = $ismember1[0];
    $uid1 = $ismember1[1];
  }
  if ($ismember2 == 'No') {
    $type2 = '';
    $uid2 = '';
  } else {
    $type2 = $ismember2[0];
    $uid2 = $ismember2[1];
  }
 
  $IsMember = array( 
    'ListAgentID' => $agent1,
    'ListOfficeID' => $agency1,
    'ListAgentType' => $type1,
    'ListAgentUID' => $uid1,
    'CoListAgentID' => $agent2,
    'CoListOfficeID' => $agency2,
    'CoListAgentType' => $type2,
    'CoListAgentUID' => $uid2,
  );

  if ($ismember1 == 'No') {
    $IsMember['ListAgentID'] = $agent2;
    $IsMember['ListOfficeID'] = $agency2;
    $IsMember['ListAgentType'] = $type2;
    $IsMember['ListAgentUID'] = $uid2;
    $ismember2 = 'No';
  } 
  if ($ismember2 == 'No') {
    $IsMember['CoListAgentID'] = '';
    $IsMember['CoListOfficeID'] = '';
    $IsMember['CoListAgentType'] = '';
    $IsMember['CoListAgentUID'] = '';
	$TeamMember = rets_bsf_get_teammember($roles,$IsMember['ListAgentUID']);
	if ($TeamMember<>'No') {
	    $IsMember['CoListAgentID'] = $TeamMember[0];
	    $IsMember['CoListOfficeID'] = $TeamMember[1];
	    $IsMember['CoListAgentType'] = $TeamMember[2];
	    $IsMember['CoListAgentUID'] = $TeamMember[3];
	}
  }
 return $IsMember;
 
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
           return array($roles[$key],$napps[$rkey]->uid);
        } 
      }
   }
   return 'No';
}

function rets_bsf_get_teammember($roles,$uid) {
	if (!empty($uid) and $uid<>0) {

		$user = user_load($uid);
		if (!isset($user->field_teammate['und'][0]['target_id'])) {
			return 'No';
		}
		$user2uid = $user->field_teammate['und'][0]['target_id'];
		if (!empty($user2uid)) {
			$user2 = user_load ($user2uid);
			$wrapper2 = entity_metadata_wrapper('user',$user2);
			$agent2 = $wrapper2->field_agentid->value();
			$agency2 = $wrapper2->field_agency->field_offid->value();
			$ismember2 = rets_bsf_ismember_agent ($roles, $agent2);
			if ($ismember2 <> 'No') {
				return array($agent2,$agency2,$ismember2[0],$ismember2[1]);
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
            $role[$key] = 'Basic';
	    break;
	  case ('gold membership') :
	    $role[$key] = 'Gold';
	    break;
          case ('platinum membership') :
            $role[$key] = 'Platinum';
	    break;
          case ('team membership') :
            $role[$key] = 'Team';
	    break;
		case ('Agent') :						// agent sites only have "agents"
            	$role[$key] = 'Agent';
	    break;
	}
    }
    return $role;
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_openrets
 *  Open the rets bsf "map" file and load up all the fields
 */

function rets_bsf_openrets () {

  global $mysqli_link;

  $map = array();

  // Open the rets database
  $host = "athena.earthclick.net";
  $user = "rmls";
  $pw = "me5Y#2#3";
  $db = "rets";

  $mysqli_link = mysqli_connect($host, $user, $pw, $db);
  if(mysqli_connect_error()) {
     $msg = mysqli_connect_error();
     rets_bsf_msg("Connect failed: $msg");
     $map['Connect Error'] = 'Connect Failed: ' .$msg;
  }
  rets_bsf_msg("Connection successful to $db on $host");

  // Read in "rets_map" and load the $map array
  $select = 'SELECT * FROM bsf_map WHERE fieldoutvalue<>""';
  if ($result = mysqli_query($mysqli_link,$select)) {

    while ($row = mysqli_fetch_array($result)) {
      $fld = chop($row['fieldinvalue']);
      $map[$fld]['out'] = chop($row['fieldoutvalue']);
      $map[$fld]['type'] = $row['fieldtype'];
      $map[$fld]['oiptable'] = $row['oiptable'];
      $map[$fld]['oipfield'] = $row['oipfield'];
    }

  } else {
     $msg = 'Unable to open bsf_map, no mapping records retrieved : ' . mysqli_error($mysqli_link);
     rets_bsf_msg("Connect failed: $msg");

  }  
  mysqli_free_result($result);
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

  $time = time();

  // Get the hotsheet variables set in rets_bsf.module
    $sendemail = variable_get('rets_bsf_sendhotsheet');
    $to = variable_get('rets_bsf_emailto');
    $from = variable_get('rets_bsf_emailfrom');
    $subject = variable_get('rets_bsf_emailsubject','Hotsheet');

   // Append the hotsheet message to the hotsheet file
        $nnode = new stdClass();
        $nnode->title = $subject;
        $nnode->body['und'][0]['value'] = $msg;
        $nnode->nid = '';                  // add or update, depending on query for 'nid' above^M
        $nnode->type = 'hotsheet';   // Your specified content type^M
        $nnode->created = $time;
        $nnode->changed = $time;
        $nnode->language = 'und';
        $nnode->status = 1;
        $nnode->promote = 0;
        $nnode->sticky = 0;
        $nnode->format = 1;       // Filtered HTML^M
        $nnode->uid = $user->uid;
        $nnode = node_submit($nnode);
        node_save($nnode);

    rets_bsf_msg ("Email:" . $sendemail . " To:" . $to . " From: " . $from . " Subject:" . $subject);

  if ($sendemail == 'Yes') {


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

$to = 'hotsheet@oip.net';
$from = 'dwork@oip.net';

  	$message = drupal_mail('rets_bsf', 'hotsheet', $to, user_preferred_language($user), array('body' => $body, 'subject' => $subject, 'headers' => array('Cc' => $header_cc, 'Bcc' => $header_bcc)), $from, TRUE);
  	// You can check $message['result'] to see if the message sent successfully.
   }

}
