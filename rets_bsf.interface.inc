<?PHP

/**-----------------------------------------------------------------------------------
 *  rets_bsf_interface
 *  control the hotsheet interface
 */

function rets_bsf_interface() {

  $roles = rets_bsf_loadroles();		// $roles[$role] = 1 means its a member

  $map = array();
  $map = rets_bsf_openrets($map);
  $adds = 0;
  $updates = 0;
  $changes = '';

  $count = 0;

  // If select, retrieve users with agentid's
   $interface = variable_get('rets_bsf_interface');
   $selectagent='';
   if ($interface <> 'All') {
      rets_bsf_msg ($selectagent);
      $users = entity_load('user');
      foreach ($users as $user) {
         if (!empty($user->field_agentid['und']['0']['safe_value'])) {
            if (strlen($selectagent)<>0) {
               $selectagent = $selectagent.',';
            }
            $selectagent = $selectagent . $user->field_agentid['und'][0]['safe_value'];
         }
      }
   }

  // Now, process the records in the "bsf_current" file
  $select = 'SELECT * FROM bsf_current';
  if ($interface <> 'All') {
     $select = $select . ' WHERE ListAgentID in (' . $selectagent . ') OR CoListAgentID in (' . $selectagent . ')';
  }

  $result = mysqli->query($select);
  $num = mysql_numi_rows->$result;
  
  rets_bsf_msg($select . ': recs ' . $num);

  while ($row = mysqli_fetch_array->$result)) {
 
     // Bypass the checking section if we're specific
     if ($interface == 'All') {

        // Check to see if one or more of the INPUT agents are OIP members
        $IsMember = rets_bsf_ismember ($roles, $row['ListAgentID'], $row['ListOfficeID'], $row['CoListAgentID'], $row['CoListOfficeID']);

        // if not a member, loop. Cleanup is performed elsewhere
        if ($IsMember == 'No') {
          continue;
        }

        $row['ListAgentID'] = $IsMember['ListAgentID'];
        $row['ListOfficeID'] = $IsMember['ListOfficeID'];
        $row['CoListAgentID'] = $IsMember['CoListAgentID'];
        $row['CoListOfficeID'] = $IsMember['CoListOfficeID'];

        // Set the owner of this listing to the first agent who is Gold or Platinum
        $editable = array ('Gold','Platinum');
        $owneruid = 1;							// Admin
        $ownername = 'admin';
        if (in_array($IsMember['ListAgentType'],$editable)) {
   	   $owneruid = $IsMember['ListAgentUID'];
           $ownername = $IsMember['ListAgentID'];
        } else if (in_array($IsMember['CoListAgentType'],$editable)) {
	   $owneruid = $IsMember['CoListAgentUID'];
           $ownername = $IsMember['CoListAgentID'];
        } 
     }

     $count++;

     // Check incoming variables and correct them if necessary
     $row = rets_bsf_prefix ($row);

	// watchdog('rets_bsf', 'Checking Listing '.$row['ListingID']);

     // See if this listing is in the Drupal database
    $query1 = new EntityFieldQuery();
    $result1 = $query1->entityCondition('entity_type', 'node')
      ->propertyCondition('type', 'listing')
      ->propertyCondition('title', $row['ListingID'], '=')
      ->execute();

    if ($result1) {  						// Update existing node
     
      	$keys = array_keys($result1['node']);
      	$nid = $keys[0];

      	$entities = entity_load('node', array_keys($result1['node']));
      	$node = $entities[$nid];
	
        // Loop through each Drupal map field for the values we're translating
	$change = '';
        if ($interface == 'All') {
           if ($node->uid <> $owneruid) {		// ownership changed
              $change .= "Owner changed to $ownername";
           }
        }

	foreach ($map as $key => $value) {
	  $out = $value['out'];
	  $type = $value['type'];
	  // if the Drupal field is present, figure out if there's an update or not.
          if (isset($node->{$out}['und'][0][$type])) {
            if ($type == 'value') {						// decode if html special characters are encoded 
		$row[$key] = htmlspecialchars_decode($row[$key]);
                $row[$key] = str_replace('&#44;', ',', $row[$key]);
	       }
	    if ($type == 'tid') {						// translate
	        $row[$key] = rets_bsf_gettid ('property_type',$row[$key]);
	    }

            if ($node->{$out}['und'][0][$type] <> $row[$key]) {			// changed
	      if ($type == 'lat' || $type == 'lon') {
		$diff = $node->{$out}['und'][0][$type] - $row[$key];
              } else {
		$diff = 9999;
	      }
              if (abs($diff) > .001) {
                $change .= $key. ': ('.$node->{$out}['und'][0][$type].') -> ('. $row[$key] .')';
//	        if (strlen(trim($row[$key])) == 0 || $row[$key] == '0000-00-00 00:00:00') {       // no input row or datetime is empty - delete the node variable
//		  unset ($node->{$out});
//	        } else {									  // replace the node value with the input value
                  $node->{$out}['und'][0][$type] = $row[$key];
//	        }
              }
            }
	  } else if (strlen($row[$key])<>0 && $row[$key]<>'0000-00-00 00:00:00') {
              $change .= $key. ': ( -new- ) -> ('. $row[$key] .')';
	      if (strlen($row[$key]) <> 0 && $row[$key] <> '0000-00-00 00:00:00') {       // no input row or datetime is empty - delete the node variable
                $node->{$out}['und'][0][$type] = $row[$key];
	      }
          }
	}
	if (strlen($change)<>0) {
	  $changes .= $row['ListingID'] . " changed *" . $node->field_status['und'][0]['value'] . "*<BR>$change<BR><BR>\n\n";
          $updates++;
	}

    } else { 	// Add new node

       	$node = new stdClass();
  	$node->type = 'listing';
  	node_object_prepare($node);
	$node->title = $row['ListingID'];
        $node->language = 'und';
	$change = 'Yes';

        // Loop through each Drupal map field for the values we're translating
        foreach ($map as $key => $value) {
          $out = $value['out'];
          $type = $value['type'];
          if ($type == 'value') {                                             // decode if html special characters are encoded
             $row[$key] = htmlspecialchars_decode($row[$key]);
             $row[$key] = str_replace('&#44;', ',', $row[$key]);
          }
	  if ($type == 'tid') {							// translate
	     $row[$key] = rets_bsf_gettid ('property_type',$row[$key]);
	  }
          if (strlen($row[$key]) <> 0 && $row[$key] <> '0000-00-00 00:00:00') {       // no input row or datetime is empty - delete the node variable
             $node->{$out}['und'][0][$type] = $row[$key];
          }
        }

	$changes .= $row['ListingID'] . " added (1:".$row['ListAgentFullName']." 2:".$row['CoListAgentFullName']. "<BR><BR>\n\n";
	$adds++;
      }
    
     if (strlen($change)<>0) {

	if (isset($node->field_expiredate['und'][0]['value'])) {
	//	watchdog('rets_bsf', 'Listing '.$row['ListingID'] . ' Before '.$node->field_expiredate['und'][0]['value']);
	} else {
	//	watchdog('rets_bsf', 'Listing '.$row['ListingID'] . ' undefined expiredate');
	}

      	// fix any problem fields, or make adjustments as necessary
      	$node = rets_bsf_postfix($node);

      	// update or add the node now
      	$node = node_submit($node);
        if ($interface == 'All') {
           $node->uid = $owneruid;			// this goes here, because node_submit changes the owner back to 1 ("admin")
        }
      	if ($node->validated) {
       	  node_save($node);
        }
	watchdog('rets_bsf', 'Listing '.$row['ListingID'] . ' updated $change:('.$change.')');
      }
  }

  // Finish up - send hotsheet email if configurted, and return message if running interactively
  $msg = $changes . "RETS BSF interface successful: $adds adds, $updates updates";
  rets_bsf_hotsheet ($msg);
  rets_bsf_msg ($msg);
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_prefix
 *  Make any field adjustments based on the other fields in $row prior to testing changes
 */

function rets_bsf_prefix ($row) {

  // Sometimes shortsale is blank. It should be "Yes" or "No"
  if (strlen(trim($row['ShortSaleYN'])) == 0) {
	$row['ShortSaleYN'] = 'No';
  }

  // INPUT dates may have be of the format YYYY-MM-DD HH:MM:SS; we need to reformat them to YYYY-MM-DD 00:00:00
  $row['DateList'] = substr($row['DateList'],0,11) . '00:00:00';

  // Also, based on status, clear the unwanted dates
  $row['DateOffMarket'] = substr($row['DateOffMarket'],0,11) . '00:00:00';
  $row['DateExpiration'] = substr($row['DateExpiration'],0,11) . '00:00:00';
  switch ($row['ListingStatus']) {
     case 'Expired':
    	$row['DateCanceled'] = '';
    	$row['DateWithdrawn'] = '';
    	$row['DateClose'] = '';
	break;
     case 'Canceled':
  	$row['DateCanceled'] = substr($row['DateCanceled'],0,11) . '00:00:00';
    	$row['DateWithdrawn'] = '';
    	$row['DateClose'] = '';
	break;
     case 'Withdrawn':
  	$row['DateWithdrawn'] = substr($row['DateWithdrawn'],0,11) . '00:00:00';
    	$row['DateCanceled'] = '';
    	$row['DateClose'] = '';
	break;
     case 'SOLD':
  	$row['DateClose'] = substr($row['DateClose'],0,11) . '00:00:00';
    	$row['DateCanceled'] = '';
    	$row['DateWithdrawn'] = '';
	break;
     case 'Active':
    	$row['DateCanceled'] = '';
    	$row['DateWithdrawn'] = '';
    	$row['DateClose'] = '';
	break;

     case 'Pending':
    	$row['DateCanceled'] = '';
    	$row['DateWithdrawn'] = '';
    	$row['DateClose'] = '';
	break;
  }

  // Clear some unwanted fields if their value is 0
  if ($row['Latitude'] == 0) {
	$row['Latitude'] = '';
  }
  if ($row['Longitude'] == 0) {
	$row['Longitude'] = '';
  }

  // Evidently, the SQFT fields are interchangeable to for some Realtors. We're only using the "Gross" column
  if ($row['SqFtApproximateTotal'] <> 0 && $row['SqFtApproximateGross'] == 0) {
	$row['SqFtApproximateGross'] = $row['SqFtApproximateTotal'];
  }
  $row['SqFtApproximateTotal'] = 0;

  // LotSize and LotSizeDescription might be interchangeable too
  if (strlen(trim($row['LotSize'])) <> 0 && strlen(trim('LotSizeDimension') == 0)) {
	$row['LotSizeDimension'] = $row['LotSize'];
  } 
  $row['LotSize'] = 0;

  return $row;
}

/**----------------------------------------------------------------------------------- 
 *  rets_bsf_postfix
 *  Make any field adjustments based on the other fields in $node
 */

function rets_bsf_postfix ($node) {
global $changes;

   // Source of data
   $node->source['und'][0]['value'] = 'MLS';

   // Load the body if its missing
   $fixit = 'N';
   if (!isset($node->body['und'][0]['value'])) {
	$fixit = 'Y';
   } else if (strlen($node->body['und'][0]['value']) == 0) {
   	$fixit = 'Y';
   }
   if ($fixit == 'Y') {
     if (isset($node->field_originaldescription['und'][0]['value'])) {
        $node->body['und'][0]['value'] = $node->field_originaldescription['und'][0]['value'];
     } else {
        $changes .= "ERROR: $node->title : Original Description was not created<BR><BR>\n\n";
     }
   }

   // Fix the original title
   $title = '';
   $fixit = 'N';
   if (!isset($node->field_originaladtitle['und'][0]['value'])) {
     $fixit = 'Y';
   } else if (strlen($node->field_originaladtitle['und'][0]['value']) == 0) {
     $fixit = 'Y';
   }
   if ($fixit = 'Y') {
     if (strlen($node->field_beds['und'][0]['value']) <> 0) {
       $title .= $node->field_beds['und'][0]['value'] . ' bed ';
     }
     if (strlen($node->field_baths['und'][0]['value']) <> 0) {
       $title .= $node->field_baths['und'][0]['value'] . ' baths ';
     }
     if (strlen($node->field_acreage['und'][0]['value']) <> 0) {
       $title .= $node->field_acreage['und'][0]['value'] . ' acres ';
     }
     $node->field_originaladtitle['und'][0]['value'] = $title;
   }

   // Fix the override title
   if (!isset($node->field_adtitle['und'][0]['value'])) {
     $node->field_adtitle['und'][0]['value'] = $node->field_originaladtitle['und'][0]['value'];
   }
   if (strlen($node->field_adtitle['und'][0]['value']) == 0) {
     $node->field_adtitle['und'][0]['value'] = $node->field_originaladtitle['und'][0]['value'];
   }

   // Fix OffMarketDate
   	if (isset($node->field_offmarketdate['und'][0]['value'])) {
	  	if ($node->field_offmarketdate['und'][0]['value'] == '0000-00-00 00:00:00') {
			unset($node->field_offmarketdate);
		}
	}
   	switch ($node->field_status['und'][0]['value']) {
     case ('Expired'):
       unset($node->field_solddate);
       break;
     case ('Canceled'):
       unset($node->field_expiredate);
       unset($node->field_solddate);
       break;
     case ('Withdrawn'):
       unset($node->field_expiredate);
       unset($node->field_solddate);
       break;
     case ('SOLD'): 
       unset($node->field_withdrawdate);
       break;
     case ('Active'):
       unset($node->field_solddate);
       unset($node->field_withdrawdate);
       break;
     case ('Pending'):
       unset($node->field_solddate);
       unset($node->field_withdrawdate);
       break;
     default:
       break;
   	}

   if (isset($node->field_expiredate['und'][0]['value']) && trim($node->field_expiredate['und'][0]['value']) == '') {
	unset ($node->field_expiredate);
   }

  return $node;
}

