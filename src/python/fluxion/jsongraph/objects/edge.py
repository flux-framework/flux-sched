import json
import unittest

class Edge:
    """Edge class representing a edge in the JSON Graph Format."""

    SOURCE = 'source'
    TARGET = 'target'
    RELATION = 'relation'
    DIRECTED = 'directed'
    METADATA = 'metadata'

    def __init__(self, source, target, relation=None, directed=None, metadata=None):
        """Constructor of the Edge class.

        Arguments:
            source -- string        the id of the source-node
            target -- string        the is of the target-node
            relation -- string      (optionally) the name of the relationship of the edge (default None)
            directed -- bool        (optionally) boolean indicating whether the edge is directed (default None)
            metadata -- dictionary  (optionally) a dictionary representing the metadata that belongs to the node (default None)

        Returns:
            Edge     Edge object initialized with the provided arguments.
        """
        self.set_source(source)
        self.set_target(target)
        self._relation = None
        if relation != None:
            self.set_relation(relation)
        self._directed = None
        if directed != None:
            self.set_directed(directed)
        self._metadata = None
        if metadata != None:
            self.set_metadata(metadata)


    def _isJsonSerializable(self, dictionary):
        try:
            json.dumps(dictionary)
            return True
        except Exception:
            return False

    def set_source(self, source):
        """Method to the set source of the edge.

        Arguments:
            source -- string    the id of the source-node to set
        """
        if source == None:
            raise ValueError('Source of Edge can not be None')
        if isinstance(source, str):
            self._source = source
        else:
            try:
                stringSource = str(source)
                self._source = stringSource
            except Exception as exception:
                raise TypeError("Type of source in Edge needs to be a string (or string castable): " + str(exception))


    def set_target(self, target):
        """Method to set the target of the edge.

        Arguments:
            target -- string    the id of the target-node to set
        """
        if target == None:
            raise ValueError('Target of Edge can not be None')
        if isinstance(target, str):
            self._target = target
        else:
            try:
                stringTarget = str(target)
                self._target = stringTarget
            except Exception as exception:
                raise TypeError("Type of target in Edge needs to be a string (or string castable): " + str(exception))


    def set_relation(self, relation):
        """Method to set the name of the relationship of the edge.

        Arguments:
            relation -- string      the name of the relationship
        """
        if relation == None:
            self._relation = None
        else:
            if isinstance(relation, str):
                self._relation = relation
            else:
                try:
                    stringLabel = str(label)
                    self._label = stringLabel
                except Exception as exception:
                    raise TypeError("Type of label in Node object needs to be a string (or string castable): " + str(exception))


    def set_directed(self, directed):
        """Method to set the edge directed or not.

        Arguments:
            directed -- bool    boolean indicating whether the edge is directed or not
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
                    raise TypeError("Type of directed in Edge needs to be a boolean (or boolean castable): " + str(exception))


    def set_metadata(self, metadata):
        """Method to set the metadata of the edge.

        Arguments:
            metadata -- dictionary      the metadata to set on the edge
        """
        if metadata == None:
            self._metadata = None
        else:
            if isinstance(metadata, dict) and self._isJsonSerializable(metadata):
                self._metadata = metadata
            else:
                raise TypeError("metadata in Edge object needs to be json serializable")

    def get_source(self):
        """Get source of the edge.

        Returns:
            string      the id of the source-node set
        """
        return self._source


    def get_target(self):
        """Get target of the edge.

        Returns:
            string      the id of the target-node set
        """
        return self._target


    def get_relation(self):
        """Get relationname of the edge.

        Returns:
            string      the name of the relationship if set, else None
        """
        return self._relation


    def is_directed(self):
        """Get boolean indicating whether edge is directed.

        Returns:
            bool    True if edge is directed, if nothing set None
        """
        return self._directed


    def get_metadata(self):
        """Get the metadata of the edge.

        Returns:
            dictionary      the metadata of the edge if set, else None
        """
        return self._metadata

    def to_JSON(self):
        """Convert the edge to JSON.

        Creates a dictionary object of the edge conforming to the JSON Graph Format.

        Returns:
            dictionary      the edge as dictionary ready to serialize
        """
        json = {Edge.SOURCE: self._source, Edge.TARGET: self._target}
        if self._relation != None:
            json[Edge.RELATION] = self._relation #TODO change in original
        if self._directed != None:
            json[Edge.DIRECTED] = self._directed
        if self._metadata != None:
            json[Edge.METADATA] = self._metadata
        return json




class TestEdgeClass(unittest.TestCase):

    def test_base(self):
        edge = Edge('from', 'to', 'relation', True, {'metaNumber': 11, 'metaString': 'hello world'})

        self.assertEqual(edge.get_source(), 'from')
        self.assertEqual(edge.get_target(), 'to')
        self.assertEqual(edge.get_relation(), 'relation')
        self.assertTrue(edge.is_directed())
        self.assertEqual(edge.get_metadata()['metaNumber'], 11)
        self.assertEqual(edge.get_metadata()['metaString'], 'hello world')

    def test_setters(self):
        edge = Edge('from', 'to', 'relation', True, {'metaNumber': 11, 'metaString': 'hello world'})
        edge.set_source('new_from')
        edge.set_target('new_to')
        edge.set_relation('new_relation')
        edge.set_directed(False)
        edge.set_metadata({'new_metaNumber': 13, 'new_metaString': 'world hello'})

        self.assertEqual(edge.get_source(), 'new_from')
        self.assertEqual(edge.get_target(), 'new_to')
        self.assertEqual(edge.get_relation(), 'new_relation')
        self.assertFalse(edge.is_directed())
        self.assertEqual(edge.get_metadata()['new_metaNumber'], 13)
        self.assertEqual(edge.get_metadata()['new_metaString'], 'world hello')
        #TODO unittest error handling

    def test_to_JSON(self):
        self.assertEqual("TODO", "TODO")
        #TODO unittest json result

if __name__ == '__main__':
    unittest.main()
