from .graph import Graph
import json
import unittest

class Multigraph:

    GRAPHS = 'graphs'
    TYPE = 'type'
    LABEL = 'label'
    METADATA = 'metadata'

    def __init__(self, graphs=[], type=None, label=None, metadata=None):
        """Constructor of the Multigraph class.

        Arguments:
            graphs -- [Graph]       list of Graph objects that are part of the multigraph (default [])
            type -- string          (optionally) the typename of the multigraph (default None)
            label -- string         (optionally) the label of the multigraph (default None)
            metadata -- dictionary  (optionally) a dictionary representing the metadata that belongs to the multigraph (default None)
        """
        self._graphs = []
        self.set_graphs(graphs)
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

    def add_graph(self, graph):
        """Method to add a graph to the multigraph.

        Arguments:
            graph -- Graph      the graph to add
        """
        if graph == None:
            return
        if isinstance(graph, Graph):
            self._graphs.append(graph)
        else:
            raise TypeError("Adding graph to Multigraph failed: graph must of type Graph")


    def set_graphs(self, graphs):
        """Method to add a list of graphs.

        Arguments:
            graphs -- [Graph]   the list of graphs that need to be added
        """
        for graph in graphs:
            self.add_graph(graph)


    def set_type(self, type):
        """Method to set the type of the multigraph.

        Arguments:
            type -- string      the typename of the multigraph to set
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
                    raise TypeError("Type of type in Multigraph object needs to be a string (or string castable): " + str(exception))


    def set_label(self, label):
        """Method to set the label of the multigraph.

        Arguments:
            label -- string     the labelname of the multigraph to set
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
                    raise TypeError("Type of label in Multigraph object needs to be a string (or string castable): " + str(exception))


    def set_metadata(self, metadata):
        """Method to set the metadata of the multigraph.

        Arguments:
            metadata -- dictionary      the metadata to set on the multigraph
        """
        if metadata == None:
            self._metadata = None
        else:
            if isinstance(metadata, dict) and self._isJsonSerializable(metadata):
                self._metadata = metadata
            else:
                raise TypeError("metadata in Multigraph object needs to be json serializable")


    def get_graphs(self):
        """Method to get a list of all graphs in the multigraph.

        Returns:
            [Graph]     list of graphs present in the multigraph
        """
        return self._graphs


    def get_type(self):
        """Method to get the type of the multigraph.

        Returns:
            string      the typename of the multigraph if set, else None
        """
        return self._type


    def get_label(self):
        """Method to get the label of the multigraph.

        Returns:
            string      the label of the multigraph if set, else None
        """
        return self._label


    def get_metadata(self):
        """"Get the metadata of the multigraph.

        Returns:
            dictionary      the metadata of the multigraph if set, else None
        """
        return self._metadata


    def to_JSON(self, asString=False):
        """Convert the multigraph to JSON.

        Creates a dictionary object of the multigraph conforming to the JSON Graph Format.

        Arguments:
            asString -- bool    if set to True the method returns the JSON as string

        Returns:
            dictionary      the multigraph as dictionary ready to serialize
        """
        result = {}
        if self._label != None:
            result[Multigraph.LABEL] = self._label
        if self._type != None:
            result[Multigraph.TYPE] = self._type
        if self._metadata != None:
            result[Multigraph.METADATA] = self._metadata
        graphs = []
        for graph in self._graphs:
            graphs.append(graph.to_JSON())
        result[Multigraph.GRAPHS] = graphs
        if asString:
            return json.dumps(result)
        else:
            return result





class TestMultigraphClass(unittest.TestCase):

    def test_base(self):
        graph = Graph([], [], 'graphType', 'graphLabel', True, {'metaNumber': 11, 'metaString': 'hello world'})
        mgraph = Multigraph([graph], 'multigraphType', 'multigraphLabel', {'metaNumber': 11, 'metaString': 'hello world'})

        self.assertEqual(mgraph.get_type(), 'multigraphType')
        self.assertEqual(mgraph.get_label(), 'multigraphLabel')
        self.assertEqual(mgraph.get_metadata()['metaNumber'], 11)
        self.assertEqual(mgraph.get_metadata()['metaString'], 'hello world')
        self.assertEqual(mgraph.get_graphs()[0], graph)

    def test_setters(self):
        graph = Graph([], [], 'graphType', 'graphLabel', True, {'metaNumber': 11, 'metaString': 'hello world'})
        mgraph = Multigraph([], 'multigraphType', 'multigraphLabel', {'metaNumber': 11, 'metaString': 'hello world'})
        mgraph.set_label('new_multigraphLabel')
        mgraph.set_type('new_multigraphType')
        mgraph.set_metadata({'new_metaNumber': 13, 'new_metaString': 'world hello'})
        mgraph.set_graphs([graph])

        self.assertEqual(mgraph.get_type(), 'new_multigraphType')
        self.assertEqual(mgraph.get_label(), 'new_multigraphLabel')
        self.assertEqual(mgraph.get_metadata()['new_metaNumber'], 13)
        self.assertEqual(mgraph.get_metadata()['new_metaString'], 'world hello')
        self.assertEqual(mgraph.get_graphs()[0], graph)
        #TODO make unit test complete

    def test_to_JSON(self):
        self.assertEqual("TODO", "TODO")
        #TODO unittest json result

if __name__ == '__main__':
    unittest.main()
