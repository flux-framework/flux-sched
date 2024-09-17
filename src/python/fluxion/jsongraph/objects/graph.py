from .node import Node
from .edge import Edge
import json
import unittest

class Graph:
    """Graph class representing a single graph in the JSON Graph Format."""

    GRAPH = 'graph'
    NODES = 'nodes'
    EDGES = 'edges'
    TYPE = 'type'
    LABEL = 'label'
    DIRECTED = 'directed'
    METADATA = 'metadata'

    def __init__(self, nodes=[], edges=[], type=None, label=None, directed=True, metadata=None):
        """Constructor of the Graph class.

        Arguments:
            nodes -- [Nodes]        list of nodes that are a part of the graph
            edges -- [Edges]        list of edges that are a part of the graph
            type -- string          (optionally) the typename of the graph (default None)
            label -- string         (optionally) the label of the graph (default None)
            directed -- bool        (optionally) boolean indicating whether the graph is directed or not (default True)
            metadata -- dictionary  (optionally) a dictionary representing the metadata that belongs to the graph (default None)

        Returns:
            Graph     Graph object initialized with the provided arguments.
        """
        self._directed = None
        self.set_directed(directed)
        self._nodes = []
        self._edges = []
        self.set_nodes(nodes)
        self.set_edges(edges)
        self._type = None
        if type != None:
            self.set_type(type)
        self._label = None
        if label != None:
            self.set_label(label)
        self._metadata = None
        if metadata != None:
            self.set_metadata(metadata)



    def _isJsonSerializable(self, dictionary):
        try:
            json.dumps(dictionary)
            return True
        except Exception:
            return False

    def add_node(self, node):
        """Method to add one node to the graph.

        Arguments:
            node -- Node    the node to add
        """
        if node == None:
            return
        if isinstance(node, Node):
            self._nodes.append(node)
        else:
            raise TypeError("Adding node to graph failed: node must of type Node")


    def add_edge(self, edge, force_direction=False):#TODO check existence of node ids when adding
        """Method to add one edge to the graph.

        Arguments:
            edge -- Edge                the edge to add
            force_direction -- bool     (optionally) boolean indicating whether the direction-value (of the graph) needs to be enforced on the edge (default False)
        """
        if edge == None:
            return
        if isinstance(edge, Edge):
            if self._directed:
                if edge.is_directed() == None:
                    edge.set_directed(True)
                if not edge.is_directed() and not force_direction:
                    ValueError("Adding undirected edge to directed graph")
                if not edge.is_directed() and force_direction:
                    edge.set_directed(True)
            self._edges.append(edge)
        else:
            raise TypeError("Adding edge to graph failed: edge must be of type Edge")


    def set_nodes(self, nodes):
        """Method to add a list of nodes to the graph.

        Arguments:
            nodes -- [Node]     array of nodes that need to be added
        """
        for node in nodes:
            self.add_node(node)


    def set_edges(self, edges, force_direction=False):
        """Method to add a list of edges to the graph.

        Arguments:
            edges -- [edge]             array of nodes that need to be added
            force_direction -- bool     (optionally) boolean indicating whether the direction-value (of the graph) needs to be enforced on the edge (default False)
        """
        for edge in edges:
            self.add_edge(edge, force_direction)


    def set_type(self, type):
        """Method to set the type of the graph.

        Arguments:
            type -- string      the typename of the graph to set
        """
        if type == None:
            self._type = None
        else:
            if isinstance(type, str):
                self._type = type
            else:
                try:
                    stringType = str(type)
                    self._type = stringType
                except Exception as exception:
                    raise TypeError("Type of type in Graph object needs to be a string (or string castable): " + str(exception))


    def set_label(self, label):
        """Method to set the label of the graph.

        Arguments:
            label -- string     the labelname of the graph to set
        """
        if label == None:
            self._label = None
        else:
            if isinstance(label, str):
                self._label = label
            else:
                try:
                    stringLabel = str(label)
                    self._label = stringLabel
                except Exception as exception:
                    raise TypeError("Type of label in Graph object needs to be a string (or string castable): " + str(exception))


    def set_directed(self, directed=True):
        """Method to make the graph (un)directed.

        Arguments:
            directed -- bool    (optionally) boolean to indicate whether the graph is directed (default True)
        """
        if directed == None:
            self._directed = None
        else:
            if isinstance(directed, bool):
                self._directed = directed
            else:
                try:
                    boolDirected = bool(target)
                    self._directed = boolDirected
                except Exception as exception:
                    raise TypeError("Type of directed in Graph needs to be a boolean (or boolean castable): " + str(exception))


    def set_metadata(self, metadata):
        """Method to set the metadata of the graph.

        Arguments:
            metadata -- dictionary      the metadata to set on the graph
        """
        if metadata == None:
            self._metadata = None
        else:
            if isinstance(metadata, dict) and self._isJsonSerializable(metadata):
                self._metadata = metadata
            else:
                raise TypeError("metadata in Graph object needs to be json serializable")


    def get_nodes(self):
        """Method to get a list of the nodes in the graph.

        Returns:
            [Node]      the list of all nodes in the graph
        """
        return self._nodes


    def get_edges(self):
        """Method to get a list of the edges in the graph.

        Returns:
            [Edge]      the list of all edges in the graph
        """
        return self._edges


    def get_type(self):
        """Method to get the type of the graph.

        Returns:
            string      the typename of the graph if set, else None
        """
        return self._type


    def get_label(self):
        """Method to get the label of the graph.

        Returns:
            string      the label of the graph if set, else None
        """
        return self._label

    def is_directed(self):
        """Method to see whether the graph is directed or not.

        Returns:
            bool        True if the graph directed, else False
        """
        return self._directed


    def get_metadata(self):
        """"Get the metadata of the graph.

        Returns:
            dictionary      the metadata of the graph if set, else None
        """
        return self._metadata

    def to_JSON(self, asString=False):
        """Convert the graph to JSON.

        Creates a dictionary object of the graph conforming to the JSON Graph Format.

        Arguments:
            asString -- bool    if set to True the method returns the JSON as string

        Returns:
            dictionary      the graph as dictionary ready to serialize
        """
        graph = {}
        if self._label != None:
            graph[Graph.LABEL] = self._label
        if self._type != None:
            graph[Graph.TYPE] = self._type
        if self._directed != None:
            graph[Graph.DIRECTED] = self._directed
        if self._metadata != None:
            graph[Graph.METADATA] = self._metadata
        if len(self._nodes) > 0:
            nodes = []
            for node in self._nodes:
                nodes.append(node.to_JSON())
            graph[Graph.NODES] = nodes
        if len(self._edges) > 0:
            edges = []
            for edge in self._edges:
                edges.append(edge.to_JSON())
            graph[Graph.EDGES] = edges
        if asString:
            return json.dumps({Graph.GRAPH: graph})
        else:
            return {Graph.GRAPH: graph}





class TestGraphClass(unittest.TestCase):

    def test_base(self):
        node = Node('nodeId', 'nodeLabel', {'metaNumber': 11, 'metaString': 'hello world'})
        edge = Edge('from', 'to', 'relation', True, {'metaNumber': 11, 'metaString': 'hello world'})
        graph = Graph([node], [edge], 'graphType', 'graphLabel', True, {'metaNumber': 11, 'metaString': 'hello world'})

        self.assertEqual(graph.get_type(), 'graphType')
        self.assertEqual(graph.get_label(), 'graphLabel')
        self.assertTrue(graph.is_directed())
        self.assertEqual(graph.get_metadata()['metaNumber'], 11)
        self.assertEqual(graph.get_metadata()['metaString'], 'hello world')
        self.assertEqual(graph.get_nodes()[0], node)
        self.assertEqual(graph.get_edges()[0], edge)

    def test_setters(self):
        node = Node('nodeId', 'nodeLabel', {'metaNumber': 11, 'metaString': 'hello world'})
        edge = Edge('from', 'to', 'relation', False, {'metaNumber': 11, 'metaString': 'hello world'})
        graph = Graph([], [], 'graphType', 'graphLabel', True, {'metaNumber': 11, 'metaString': 'hello world'})
        graph.set_directed(False)
        graph.set_label('new_graphLabel')
        graph.set_type('new_graphType')
        graph.set_metadata({'new_metaNumber': 13, 'new_metaString': 'world hello'})
        graph.set_nodes([node])
        graph.set_edges([edge])

        self.assertEqual(graph.get_type(), 'new_graphType')
        self.assertEqual(graph.get_label(), 'new_graphLabel')
        self.assertFalse(graph.is_directed())
        self.assertEqual(graph.get_metadata()['new_metaNumber'], 13)
        self.assertEqual(graph.get_metadata()['new_metaString'], 'world hello')
        self.assertEqual(graph.get_nodes()[0], node)
        self.assertEqual(graph.get_edges()[0], edge)
        #TODO make unit test complete

    def test_to_JSON(self):
        self.assertEqual("TODO", "TODO")
        #TODO unittest json result

if __name__ == '__main__':
    unittest.main()
