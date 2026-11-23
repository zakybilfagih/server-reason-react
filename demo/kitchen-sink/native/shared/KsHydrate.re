/* A loader's dehydrated queries, put in the client before its children
   read them, and the route's errorComponent for a query that then fails in
   the browser. */
[@react.client.component]
let make = (~state: string, ~children: React.element) =>
  <ReactQuery.HydrationBoundary
    state={ReactQuery.DehydratedState.fromJson(
      Melange_json.of_string(state),
    )}>
    <ReactQuery.ErrorBoundary
      fallback={(~error, ~retry as _) =>
        <KsErrorComponent message={KsData.errorMessage(error)} />
      }>
      children
    </ReactQuery.ErrorBoundary>
  </ReactQuery.HydrationBoundary>;
