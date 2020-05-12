.. _http_json_replace:

json/replace
------------

``json/replace`` works similar to SphinxQL's :ref:`insert_and_replace_syntax`. It inserts a new document into an index and if the index already has a document with the same id, it is deleted before the new document is inserted. There's also a synonym endpoint, ``json/index``.

::

	{
	  "index":"test",
	  "id":1,
	  "doc":
	  {
	    "gid" : 10,
	    "content" : "updated document"
	  }
	}
	
The daemon will respond with a JSON object stating if the operation was successfull or not:

::
   
   {
    "_index": "test",
    "_id": 1,
    "created": false,
    "result": "updated"
  }	