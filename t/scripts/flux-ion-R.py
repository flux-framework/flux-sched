#!/bin/false
#
#  Run script as `flux ion-R ` with properly configured
#   FLUX_EXEC_PATH or `flux python flux-ion-R` if not to
#   avoid python version mismatch
#

import sys
import json
from flux.idset import IDset
from flux.hostlist import Hostlist
from jsongraph.objects.graph import Graph, Node, Edge

def idset_expand(s):
    return list(IDset(s))

uniqId = 0
def tick_uniq_id():
    global uniqId
    uniqId += 1

def get_uniq_id():
    global uniqId
    return uniqId

def postfix_digits(s):
    res = [int(i) for i in s.split() if i.isdigit()]

def extract_id_from_hn(s):
    import re
    global uniqId
    postfix = re.findall(r"(\d+$)", s)
    if len(postfix) == 1:
        return int(postfix[0])
    else:
        return uniqId

rv1 = json.loads(sys.stdin.read())
R_lite = rv1['execution']['R_lite']
nodeList = Hostlist(rv1['execution']['nodelist'])
graph = Graph()

#
# Create a cluster graph node
#
clusterNode = Node(get_uniq_id(),
                   metadata={ 'type': 'cluster',
                              'basename': 'cluster',
                              'name': 'cluster0',
                              'id' : get_uniq_id(),
                              'uniq_id' : get_uniq_id(),
                              'rank' : -1,
                              'exclusive' : True,
                              'unit' : '',
                              'size' : 1,
                              'paths' : {
                                  'containment': '/cluster0'
                              }
                            }
                   )
graph.add_node(clusterNode)
tick_uniq_id()

h = -1
for entry in R_lite:
    for rank in idset_expand(entry['rank']):
        #
        # Create a compute-node graph node
        #
        h += 1
        nodePath=f"/cluster0/{nodeList[h]}"
        node = Node(get_uniq_id(),
                    metadata={ 'type': 'node',
                               'basename': 'node',
                               'name': nodeList[h],
                               'id' : extract_id_from_hn (nodeList[h]),
                               'uniq_id' : get_uniq_id(),
                               'rank' : rank,
                               'exclusive' : True,
                               'unit' : '',
                               'size' : 1,
                               'paths' : {
                                   'containment': nodePath
                               }
                             }
                       )
        graph.add_node(node)
        edge = Edge(clusterNode.get_id(),
                    node.get_id(),
                    directed=True,
                    metadata={ 'name' : { 'containment' : 'contains' }},
                    )
        graph.add_edge(edge)
        tick_uniq_id()

        for k,v in entry['children'].items():
            for i in idset_expand(v):
                #
                # Create a core or gpu graph node
                #
                resourceType = str(k)
                childNodePath = f"{nodePath}/{resourceType}{i}"
                childNode=Node(uniqId,
                               metadata={ 'type': resourceType,
                                          'basename': resourceType,
                                          'name': resourceType + str(i),
                                          'id' : i,
                                          'uniq_id' : uniqId,
                                          'rank' : rank,
                                          'exclusive' : True,
                                          'unit' : '',
                                          'size' : 1,
                                          'paths' : {
                                              'containment': childNodePath
                                          }
                                        }
                              )
                graph.add_node(childNode)
                edge = Edge(node.get_id(),
                            childNode.get_id(),
                            directed=True,
                            metadata={ 'name' : { 'containment' : 'contains' }}
                           )
                graph.add_edge (edge)
                tick_uniq_id()

rv1['scheduling'] = graph.to_JSON()
print (json.dumps(rv1))

#
# vi: ts=4 sw=4 expandtab
#
