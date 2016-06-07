<?php

#------- include this as part of the cron in rets_bsf module --------------------------------------#

#------------------------------------------------------------------------------------------------------------
# First, get each Realtor and test for "gold" or "platinum". Build an array of specific listings #'s or another array of realtors with "all"
#------------------------------------------------------------------------------------------------------------

function rets_bsf_rotate () {
$listings = array();
$agents = array();

print "<PRE>";
$query = new EntityFieldQuery;
$result = $query
  ->entityCondition('entity_type', 'user')
  ->propertyCondition('status',1)
  ->execute();

// $interface is "All", we need to process by role. Otherwise, we simply select all the listings as below
   $interface = variable_get('rets_bsf_interface');

if (!empty($result['user'])) {
  $users = user_load_multiple(array_keys($result['user']));
  foreach ($users as $user) {
    if ($interface == 'All') {
      if (isset($user->roles[5]) || isset($user->roles[6]) ) {    # gold and platinum
        # if a specific listing has been selected for this realtor, reflag it
        if (!isset($user->field_rotatinglisting['und'][0]['value']) && !empty($user->field_rotatinglisting['und'][0]['value'])) {
               array_push($listings,$user->field_rotatinglisting['und'][0]['nid']);
        } else {         # else if this is a platinum realtor, reflag all their Active/Pending/bumpable listings
            if (isset($user->roles[6])) {
               array_push($agents,$user->field_agentid['und'][0]['value']);
            } else {
 //              print $user->field_lastname['und'][0]['value'];
 //              print "---->Agent is a 'gold' member, but no rotating listing selection was made. Notify the agent<BR>";
            }
        }
      }
    #  select all realtors in this database
    } else { 
        # If a specific list has been requestion, we won't rotate.
        if (!isset($user->field_rotatinglisting['und'][0]['value']) && !empty($user->field_rotatinglisting['und'][0]['value'])) {
            array_push($listings,$user->field_rotatinglisting['und'][0]['nid']);
        # Otherwise just load this agent into the selection array & pick all their Active/Pending/Bumpable listings
        } else { 
            if (isset($user->field_agentid['und'])) {
               array_push($agents,$user->field_agentid['und'][0]['value']);
            }
        }
    }
  }
} 

#------------------------------------------------------------------------------------------------------------
# Next, reset all the flags in the listing file. We need to do this to ensure that any non-active
# listings are not accidentally included
#------------------------------------------------------------------------------------------------------------

$query = new EntityFieldQuery;
$result = $query
  ->entityCondition('entity_type', 'node')
  ->propertyCondition('type', 'Listing')
  ->execute();

if (!empty($result['node'])) {
  $nodes = node_load_multiple(array_keys($result['node']));
  foreach ($nodes as $node) if ($node = node_submit($node)) { 
     $node->field_rotatelisting['und'][0]['value'] = 'No';
     if ($node = node_submit($node)) { 
        node_save($node);
     }       
  }
}

#------------------------------------------------------------------------------------------------------------
# Finally, select for gold and platinum agents' listing that are Active/Pending/Bumpable Buyer
#------------------------------------------------------------------------------------------------------------

$query = new EntityFieldQuery;
$result = $query
  ->entityCondition('entity_type', 'node')
  ->propertyCondition('type', 'Listing')
  ->fieldCondition('field_status', 'value', array('Active','Pending','Bumpable Buyer'), "IN")
  ->execute();

if (!empty($result['node'])) {
  $nodes = node_load_multiple(array_keys($result['node']));
  foreach ($nodes as $node) if ($node = node_submit($node)) { 
    $agent1 = '';
    $agent2 = '';
    if (isset($node->field_listingagentid['und']) ) {
       $agent1 = $node->field_listingagentid['und'][0]['value'];
       if (isset($node->field_listingagentid['und'][1]) ) {
           $agent2 = $node->field_listingagentid['und'][1]['value'];
       }
    }
    if (in_array($node->nid,$listings) || in_array($agent1,$agents) || in_array($agent2,$agents) ) {     # in the listings array
         $node->field_rotatelisting['und'][0]['value'] = 'Yes';
//         print "Included: $node->title $agent1 $agent2<BR>";
    } else {
         $node->field_rotatelisting['und'][0]['value'] = 'No';
//         print "Omitted: $node->title $agent1 $agent2<BR>";
    }
    if ($node = node_submit($node)) { 
           node_save($node);
    }       
  }
}

drupal_set_message(t('Rotating Feature flag successfully updated'));
}   # function rotate
