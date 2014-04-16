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

  // Now, process the records in the "bsf_current" file
  $select = 'SELECT * FROM bsf_current';
  $query = mysql_query($select);
  while ($row = mysql_fetch_array($query)) {
    
     // Check to see if one or more of the INPUT agents are OIP members
     $IsMember = rets_bsf_ismember ($roles, $row);

     // Check incoming variables and correct them if necessary
     $row = rets_bsf_prefix ($row);

     // See if this listing is in the Drupal database
    $query1 = new EntityFieldQuery();
    $result1 = $query1->entityCondition('entity_type', 'node')
      ->propertyCondition('type', 'listing')
      ->propertyCondition('title', $row['ListingID'], '=')
      ->execute();

    if ($result1) {  						// Update existing node
     
      	$keys = array_keys($result1['node']);
      	$nid = $keys[0];

        // if not a member, loop. Cleanup is performed elsewhere
      	if ($IsMember == 'No') {
	  continue;
    	}

      	$entities = entity_load('node', array_keys($result1['node']));
      	$node = $entities[$nid];
	
        // Loop through each Drupal map field for the values we're translating
	$change = '';
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
	        if (strlen($row[$key]) == 0 || $row[$key] == '0000-00-00 00:00:00') {       // no input row or datetime is empty - delete the node variable
		  unset ($node->{$out});
	        } else {									  // replace the node value with the input value
                  $node->{$out}['und'][0][$type] = $row[$key];
	        }
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
	  $changes .= $row['ListingID'] . " changed<BR>$change<BR><BR>\n\n";
          $updates++;
	}

    } else { 	// Add new node

      	// If this isn't a member's listing, just continue
    	if ($IsMember == 'No') {
            continue;
    	}

       	$node = new stdClass();
  	$node->type = 'listing';
  	node_object_prepare($node);
	$node->title = $row['ListingID'];
        $node->language = 'und';
        $node->uid = 1;

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

	$changes .= $row['ListingID'] . " added<BR><BR>\n\n";
	$adds++;
      }
    
      // fix any problem fields, or make adjustments as necessary
      $node = rets_bsf_postfix($node);

      // update or add the node now
      $node = node_submit($node);
      if ($node->validated) {
       node_save($node);
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
  switch ($row['ListingStatus']) {
     case 'Expired':
  	$row['DateExpiration'] = substr($row['DateExpiration'],0,11) . '00:00:00';
    	$row['DateCanceled'] = '';
    	$row['DateWithdrawn'] = '';
    	$row['DateClose'] = '';
	break;
     case 'Canceled':
  	$row['DateCanceled'] = substr($row['DateCanceled'],0,11) . '00:00:00';
    	$row['DateExpiration'] = '';
    	$row['DateWithdrawn'] = '';
    	$row['DateClose'] = '';
	break;
     case 'Withdrawn':
  	$row['DateWithdrawn'] = substr($row['DateWithdrawn'],0,11) . '00:00:00';
    	$row['DateExpiration'] = '';
    	$row['DateCanceled'] = '';
    	$row['DateClose'] = '';
	break;
     case 'SOLD':
  	$row['DateClose'] = substr($row['DateClose'],0,11) . '00:00:00';
    	$row['DateExpiration'] = '';
    	$row['DateCanceled'] = '';
    	$row['DateWithdrawn'] = '';
	break;
     case 'Active':
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
   switch ($node->field_status['und'][0]['value']) {
     case ('Expired'):
       $node->field_offmarketdate['und'][0]['value'] = $node->field_expiredate['und'][0]['value'];
       unset($node->field_withdrawdate);
       unset($node->field_canceldate);
       unset($node->field_solddate);
       break;
     case ('Canceled'):
       $node->field_offmarketdate['und'][0]['value'] = $node->field_canceldate['und'][0]['value'];
       unset($node->field_withdrawdate);
       unset($node->field_expiredate);
       unset($node->field_solddate);
       break;
     case ('Withdrawn'):
       $node->field_offmarketdate['und'][0]['value'] = $node->field_withdrawdate['und'][0]['value'];
       unset($node->field_expiredate);
       unset($node->field_canceldate);
       unset($node->field_solddate);
       break;
     case ('SOLD'): 
       $node->field_offmarketdate['und'][0]['value'] = $node->field_solddate['und'][0]['value'];
       unset($node->field_canceldate);
       unset($node->field_withdrawdate);
       break;
     case ('Active'):
       unset($node->field_offmarketdate['und'][0]['value']);
       unset($node->field_solddate);
       unset($node->field_canceldate);
       unset($node->field_withdrawdate);
       break;
     default:
       break;
   }

  return $node;
}

