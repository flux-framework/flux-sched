import json
import unittest

class Node:
    """Node class representing a node in the JSON Graph Format."""

    ID = 'id'
    LABEL = 'label'
    METADATA = "metadata"

    def __init__(self, id, label=None, metadata=None):
        """Constructor of the Node class.

        Arguments:
            id -- string            the id of the node
            label -- string         (optionally) the label for the node (default None)
            metadata -- dictionary  (optionally) a dictionary representing the metadata that belongs to the node (default None)

        Returns:
            Node    Node object initialized with the provided arguments.
        """
        self.set_id(id)
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

    def set_id(self, id):
        """Method to set the id of the node.

        Arguments:
            id -- string    the id to set
        """
        if id == None:
            raise ValueError('Id of Node can not be None')
        if isinstance(id, str):
            self._id = id
        else:
            try:
                stringId = str(id)
                self._id = stringId
            except Exception as exception:
                raise TypeError("Type of id in Node needs to be a string (or string castable): " + str(exception))


    def set_label(self, label):
        """Method to set the label of the node.

        Arguments:
            label -- string     the label to set.
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
                    raise TypeError("Type of label in Node object needs to be a string (or string castable): " + str(exception))


    def set_metadata(self, metadata):
        """Method to set the metadata of the node.

        Arguments:
            metadata -- dictionary      the metadata to set.
        """
        if metadata == None:
            self._metadata = None
        else:
            if isinstance(metadata, dict) and self._isJsonSerializable(metadata):
                self._metadata = metadata
            else:
                raise TypeError("metadata in Node object needs to be json serializable")


    def get_id(self):
        """Get the id of the node.

        Returns:
            string      id of the node
        """
        return self._id

    def get_label(self):
        """Get the label of the node.

        Returns:
            string      label of the node if set, else None
        """
        return self._label

    def get_metadata(self):
        """Get the metadata of the node.

        Returns:
            dictionary      the metadata of the node if set, else None
        """
        return self._metadata

    def to_JSON(self):
        """Convert the node to JSON.

        Creates a dictionary object of the node conforming to the JSON Graph Format.

        Returns:
            dictionary      the node as dictionary ready to serialize
        """
        json = {Node.ID: self._id}
        if self._label != None:
            json[Node.LABEL] = self._label
        if self._metadata != None:
            json[Node.METADATA] = self._metadata
        return json




class TestNodeClass(unittest.TestCase):

    def test_base(self):
        node = Node('nodeId', 'nodeLabel', {'metaNumber': 11, 'metaString': 'hello world'})

        self.assertEqual(node.get_id(), 'nodeId')
        self.assertEqual(node.get_label(), 'nodeLabel')
        self.assertEqual(node.get_metadata()['metaNumber'], 11)
        self.assertEqual(node.get_metadata()['metaString'], 'hello world')

    def test_setters(self):
        node = Node('nodeId', 'nodeLabel', {'metaNumber': 11, 'metaString': 'hello world'})
        node.set_id('new_nodeId')
        node.set_label('new_nodeLabel')
        node.set_metadata({'new_metaNumber': 13, 'new_metaString': 'world hello'})

        self.assertEqual(node.get_id(), 'new_nodeId')
        self.assertEqual(node.get_label(), 'new_nodeLabel')
        self.assertEqual(node.get_metadata()['new_metaNumber'], 13)
        self.assertEqual(node.get_metadata()['new_metaString'], 'world hello')
        #TODO unittest error handling

    def test_to_JSON(self):
        self.assertEqual("TODO", "TODO")
        #TODO unittest json result

if __name__ == '__main__':
    unittest.main()
