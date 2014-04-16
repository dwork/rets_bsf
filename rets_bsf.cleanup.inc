<?PHP

/**-----------------------------------------------------------------------------------
 *  rets_bsf_cleanup
 *  perform regular maintenance on the database
 *    1)  Run through ALL listings and make sure they are assigned to a member. "delete_node" if not
 */

function rets_bsf_cleanup() {

  $roles = rets_bsf_loadroles();		// $roles[$role] = 1 means its a member

  $adds = 0;
  $updates = 0;
  $changes = '';
  $count = 0;

  $deletes = 0;
  $changes = '';

  // Now, process all listing nodes to check the membership status of their agents

    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', 'node')
      ->propertyCondition('type', 'listing')
      ->propertyCondition('status', '1')
      ->execute();

    $nodes = node_load_multiple(array_keys($result['node']));
    foreach ($nodes as $node) {
 
       // Check to see if one or more of the INPUT agents are OIP members
       if (isset($node->field_listingagentid['und'][0]['value'])) {
         $agent1 = $node->field_listingagentid['und'][0]['value'];
       } else {
   	 $agent1;
       }

       if (isset($node->field_colistingagentid['und'][0]['value'])) {
         $agent2 = $node->field_colistingagentid['und'][0]['value'];
       } else {
	 $agent2 = '';
       }

       $IsMember = rets_bsf_ismember ($roles, $agent1, '', $agent2, '');

       if ($IsMember == 'No') {
	 node_delete($node->nid);
	 $changes .= $node->title . ' deleted, agents not OIP members ('.$agent1.','.$agent2.")<BR>";
	 $deletes++;
       }
    }
    if (strlen($changes) == 0) {
	$changes = 'Cleanup: no listings deleted';
    } else {
	$changes .= "Cleanup: $deletes records deleted<BR>";
    }
    rets_bsf_msg($changes);
}

