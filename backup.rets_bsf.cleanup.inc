<?PHP

/**-----------------------------------------------------------------------------------
 *  rets_bsf_cleanup
 *  perform regular maintenance on the database
 *    1)  Run through ALL listings and make sure they are assigned to a member. "delete_node" if not
 */

function rets_bsf_cleanup() {

  $roles = rets_bsf_loadroles();		// $roles[$role] = 1 means its a member

  $map = array();
  $map = rets_bsf_openrets($map);
  $adds = 0;
  $updates = 0;
  $changes = '';

  $count = 0;

  // Now, process all listing nodes to check the membership status of their agents

    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', 'node')
      ->propertyCondition('type', 'listing')
      ->propertyCondition('status', '1')
      ->execute();

    $nodes = node_load_multiple(array_keys($result['node']));
    foreach ($nodes as $node) {
 
       // Check to see if one or more of the INPUT agents are OIP members
       $row = array();
       if (isset($node->field_listingagentid['und'][0]['value'])) {
         $row['ListAgentID'] = $node->field_listingagentid['und'][0]['value'];
       } else {
   	 $row['ListAgentID'] = '';
       }

       if (isset($node->field_colistingagentid['und'][0]['value'])) {
         $row['CoListAgentID'] = $node->field_colistingagentid['und'][0]['value'];
       } else {
	 $row['CoListAgentID'] = '';
       }

       if (isset($node->field_sellingagentid['und'][0]['value'])) {
         $row['SaleAgentID'] = $node->field_sellingagentid['und'][0]['value'];
       } else {
	 $row['SaleAgentID'] = '';
       }

       $IsMember = rets_bsf_ismember ($roles, $row);

print "<PRE";
print "Member: $IsMember<BR>";
       print_r ($node);
       exit;
    }
}

