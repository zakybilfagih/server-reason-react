let client =
  ReactQuery.QueryClient.make(
    ~staleTime=Ms(60_000.),
    ~retry=Times(1),
    ~onQueryError=
      (error, query) =>
        Js.Console.warn3(
          "Query failed:",
          ReactQuery.CachedQuery.hash(query),
          error,
        ),
    (),
  );
ReactQuery.hydrateFromServer(client);

let loaderState =
  switch (Webapi.Dom.Document.getElementById("data", Webapi.Dom.document)) {
  | Some(script) =>
    Webapi.Dom.Element.textContent(script)
    |> Melange_json.of_string
    |> ReactQuery.DehydratedState.fromJson
  | None =>
    ReactQuery.DehydratedState.fromJson(
      Melange_json.of_string("{\"queries\":[]}"),
    )
  };

switch (Webapi.Dom.Document.querySelector("#root", Webapi.Dom.document)) {
| Some(element) =>
  let _ =
    ReactDOM.Client.hydrateRoot(
      element,
      <ReactQuery.PersistQueryClientProvider
        client persister=SsrQueryDemo.persister>
        <SsrQueryDemo loaderState />
      </ReactQuery.PersistQueryClientProvider>,
    );
  ();
| None => Js.log("No root element found")
};
