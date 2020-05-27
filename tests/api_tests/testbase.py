#!/usr/bin/python3

import os
import json
import jsondiff
from pathlib import Path
from pattern_corrector import PatternCorrector

TEST_NODE_IS_HIVE = True
REFERENCE_NODE_IS_HIVE = False

# ignore this tags when comparing results from test and reference nodes
TAGS_TO_REMOVE = [
  "delayed_votes",
  "timestamp",
  "post_id",
  "id",
  "chain_id",
  "confidential_supply",
  "confidential_sbd_supply"
]

TAGS_TO_RENAME = {
  "STEEM_"                            : "HIVE_",
  "SBD_"                              : "HBD_",
  "steem"                             : "hive",
  "sbd"                               : "hbd",
  "reblogged_on"                      : "reblog_on",
  "sbd_interest_rate"                 : "hbd_interest_rate",
  "percent_steem_dollars"             : "percent_hbd",
  "current_sbd_supply"                : "current_hbd_supply",
  "init_sbd_supply"                   : "init_hbd_supply",
  "sbd_print_rate"                    : "hbd_print_rate",
  "sbd_start_percent"                 : "hbd_start_percent",
  "sbd_stop_percent"                  : "hbd_stop_percent",
  "sbd_volume"                        : "hbd_volume",
  "steem_volume"                      : "hbd_volume",
  "steem_revision"                    : "hive_revision",
  "sbd_exchange_rate"                 : "hbd_exchange_rate",
  "last_sbd_exchange_update"          : "last_hbd_exchange_update",
  "reward_sbd_balance"                : "reward_hbd_balance",
  "reward_steem_balance"              : "reward_hive_balance",
  "reward_vesting_steem"              : "reward_vesting_hive",
  "savings_sbd_balance"               : "savings_hbd_balance",
  "savings_sbd_last_interest_payment" : "savings_hbd_last_interest_payment",
  "savings_sbd_seconds"               : "savings_hbd_seconds",
  "savings_sbd_seconds_last_update"   : "savings_hbd_seconds_last_update",
  "sbd_balance"                       : "hbd_balance",
  "sbd_last_interest_payment"         : "hbd_last_interest_payment",
  "sbd_seconds"                       : "hbd_seconds",
  "sbd_seconds_last_update"           : "hbd_seconds_last_update",
  "pending_rewarded_vesting_steem"    : "pending_rewarded_vesting_hive",
  "total_reward_fund_steem"           : "total_reward_fund_hive",
  "total_vesting_fund_steem"          : "total_vesting_fund_hive",
  "percent_hive_dollars"              : "percent_hbd"
}

def rename_tags(data):
  for k, v in TAGS_TO_RENAME.items():
    data = rename_tag(data, k, v)
  return data

def rename_tag(data, old, new):
  if not isinstance(data, (dict, list)):
    return data
  if isinstance(data, list):
    return [rename_tag(v, old, new) for v in data]
  return {k.replace(old, new): rename_tag(v, old, new) for k, v in data.items()}

def remove_tag(data):
  if not isinstance(data, (dict, list)):
    return data
  if isinstance(data, list):
    return [remove_tag(v) for v in data]
  return {k: remove_tag(v) for k, v in data.items() if k not in TAGS_TO_REMOVE}

class SimpleJsonTest(object):
  def __init__(self, test_node, ref_node, work_dir):
    self._test_node = test_node
    self._ref_node = ref_node
    self._work_dir = Path(work_dir)

    self.create_work_dir()

  def create_work_dir(self):
    if self._work_dir.exists():
      if self._work_dir.is_file():
        os.remove(self._work_dir)

    if not self._work_dir.exists():
      self._work_dir.mkdir(parents = True)

  def create_file_name(self, file_name):
    return self._work_dir / Path(file_name)

  def write_to_file(self, dest_file, data, label):
    try:
      f = dest_file.open( "a" )
      f.write("{} Response:\n".format(label))
      json.dump(data, f, indent=2, sort_keys=True)
      f.close()
      return True
    except: 
      print( "Cannot open file:", dest_file )
    return False

  def json_pretty_string(self, json_obj):
    return json.dumps(json_obj, sort_keys=True, indent=4)

  def compare_results_internal(self, args, args2, print_response = False, paths = [], test_node_is_hive = TEST_NODE_IS_HIVE, reference_node_is_hive = REFERENCE_NODE_IS_HIVE):
    try:
      from requests import post

      print("Sending query:\n{}".format(self.json_pretty_string(args)))
      if args is not args2:
        print("Sending query:\n{}".format(self.json_pretty_string(args2)))

      resp1 = post(self._test_node, data=json.dumps(args))
      resp2 = post(self._ref_node, data=json.dumps(args2))

      filename1 = self.create_file_name("ref.log")
      filename2 = self.create_file_name("tested.log")

      # we will treat codes != 200 as test failed
      if resp1.status_code != 200 or resp2.status_code != 200:
        print("One of the nodes returned non 200 return codes. Test node: {}, reference node: {}".format(resp1.status_code, resp2.status_code))
        self.write_to_file(filename1, resp1, self._test_node)
        self.write_to_file(filename2, resp2, self._ref_node)
        return False

      json1 = resp1.json()
      json1 = remove_tag(json1)
      if reference_node_is_hive and not test_node_is_hive:
        json1 = rename_tags(json1)

      json2 = resp2.json()
      json2 = remove_tag(json2)
      if test_node_is_hive and not reference_node_is_hive:
        json2 = rename_tags(json2)

      pc1 = PatternCorrector( paths )
      pc2 = PatternCorrector( paths )

      json1 = pc1.run( json1 )
      json2 = pc2.run( json2 )

      if print_response:
        print("Response from test node at {}:\n{}".format(self._test_node, self.json_pretty_string(json1)))
        print("Response from ref node at {}:\n{}".format(self._ref_node, self.json_pretty_string(json2)))

      json1_error = json1.get("error", None)
      json2_error = json2.get("error", None)

      if json1_error is not None:
        print("Test node {} returned error code".format(self._test_node))
        self.write_to_file(filename1, json1, self._test_node)

      if json2_error is not None:
        print("Reference node {} returned error code".format(self._ref_node))
        self.write_to_file(filename2, json2, self._ref_node)

      if resp1.status_code != resp2.status_code or json1 != json2:
        print("Difference detected")

        # if print_response flag is not set print responses to compare, avoid double printing if flag is set to true
        if not print_response:
          print("Response from test node at {}:\n{}".format(self._test_node, self.json_pretty_string(json1)))
          print("Response from ref node at {}:\n{}".format(self._ref_node, self.json_pretty_string(json2)))

        json_diff = jsondiff.diff(json1, json2)
        if json_diff:
          print("Differences detected in jsons: {}".format(self.json_pretty_string(json_diff)))

        # avoid dobuble write in case of error detected and differences detected.
        if json1_error is None:
          self.write_to_file(filename1, json1, self._test_node)
        if json2_error is None:
          self.write_to_file(filename2, json2, self._ref_node)
        
        print( "Compare break with error")
        return False

      print( "Compare finished")
      return True

    except Exception as ex:
      # treat exception as test failed
      print("Exception during sending request: {}".format(ex))
      return False

  def compare_results(self, args, print_response = False, paths = [], test_node_is_hive = TEST_NODE_IS_HIVE, reference_node_is_hive = REFERENCE_NODE_IS_HIVE):
    return self.compare_results_internal( args, args, print_response, paths, test_node_is_hive, reference_node_is_hive )

  def compare_results_with_2_queries(self, args, args2, print_response = False, paths = [], test_node_is_hive = TEST_NODE_IS_HIVE, reference_node_is_hive = REFERENCE_NODE_IS_HIVE):
    if REFERENCE_NODE_IS_HIVE:
      return self.compare_results_internal( args, args, print_response, paths, test_node_is_hive, reference_node_is_hive )
    else:
      return self.compare_results_internal( args, args2, print_response, paths, test_node_is_hive, reference_node_is_hive )

if __name__ == "__main__":
  import argparse
  parser = argparse.ArgumentParser()

  parser.add_argument("api", type = str, help = "Full api name. i.e: database_api.get_block")
  parser.add_argument("params", type = str, help = "JSON with parameters")
  
  parser.add_argument("--test-node", dest = "test_node", type = str, default = "https://api.steem.house", help = "IP address of test node")
  parser.add_argument("--test-node-is-hive", dest = "test_node_is_hive", type = bool, default = True, help = "Test node is hive node?")
  parser.add_argument("--ref-node", dest = "ref_node", type = str, default = "https://api.steemit.com", help = "IP address of reference node")
  parser.add_argument("--ref-node-is-hive", dest = "ref_node_is_hive", type = bool, default = False, help = "Reference node is hive?")
  parser.add_argument("--work-dir", dest = "work_dir", type = str, default = "./", help = "Work dir")
  parser.add_argument("--print-response", dest = "print_response", type = bool, default = False, help = "Print response from nodes")

  args = parser.parse_args()
  tester = SimpleJsonTest(args.test_node, args.ref_node, args.work_dir)

  test_args = {
    "jsonrpc": "2.0",
    "id": 1,
    "method": args.api,
    "params": json.loads(args.params)
  }

  if tester.compare_results(test_args, args.print_response, args.test_node_is_hive, args.ref_node_is_hive):
    exit(0)
  exit(1)
