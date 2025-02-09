
@startDocuBlock get_api_collection_collection

@RESTHEADER{GET /_api/collection/{collection-name}, Get the collection information, getCollection}

@HINTS
{% hint 'warning' %}
Accessing collections by their numeric ID is deprecated from version 3.4.0 on.
You should reference them via their names instead.
{% endhint %}

@RESTURLPARAMETERS

@RESTURLPARAM{collection-name,string,required}
The name of the collection.

@RESTDESCRIPTION
The result is an object describing the collection with the following
attributes:

- `id`: The identifier of the collection.

- `name`: The name of the collection.

- `status`: The status of the collection as number.
  - 3: loaded
  - 5: deleted

Every other status indicates a corrupted collection.

- `type`: The type of the collection as number.
  - 2: document collection (normal case)
  - 3: edge collection

- `isSystem`: If `true` then the collection is a system collection.

@RESTRETURNCODES

@RESTRETURNCODE{404}
If the `collection-name` is unknown, then a *HTTP 404* is
returned.
@endDocuBlock
