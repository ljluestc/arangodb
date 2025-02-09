@startDocuBlock get_api_view_view

@RESTHEADER{GET /_api/view/{view-name}, Get information about a View, getView}

@RESTURLPARAMETERS

@RESTURLPARAM{view-name,string,required}
The name of the View.

@RESTDESCRIPTION
The result is an object briefly describing the View with the following attributes:
- `id`: The identifier of the View
- `name`: The name of the View
- `type`: The type of the View as string

@RESTRETURNCODES

@RESTRETURNCODE{404}
If the `view-name` is unknown, then a *HTTP 404* is returned.

@EXAMPLES

Using an identifier:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewIdentifierArangoSearch}
    var view = db._createView("productsView", "arangosearch");

    var url = "/_api/view/"+ view._id;
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);

    db._dropView("productsView");
@END_EXAMPLE_ARANGOSH_RUN

Using a name:

@EXAMPLE_ARANGOSH_RUN{RestViewGetViewNameArangoSearch}
    var view = db._createView("productsView", "arangosearch");

    var url = "/_api/view/productsView";
    var response = logCurlRequest('GET', url);
    assert(response.code === 200);
    logJsonResponse(response);

    db._dropView("productsView");
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock
