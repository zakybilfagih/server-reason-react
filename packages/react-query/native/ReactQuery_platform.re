module type S = {
  type queryClient;

  module Server: {
    /** [ReactDOM.renderToStream] with the client's queries in the stream, for
        [ReactQuery.hydrateFromServer] in the browser. Queries queued while
        the shell renders go out in the shell's chunk. After that, each query
        goes out as soon as it settles, so its data precedes the Suspense
        boundary that shows it. Like TanStack Router's SSR integration, the
        stream stays open after rendering until every prefetch the browser
        was told about settles. After [timeout] milliseconds (default 60000)
        it ends without them, and they stay pending in the browser. Aborting
        ends it at once. */
    let renderToStream:
      (
        ~env:
          [
            | `Dev
            | `Prod
          ]
            =?,
        ~timeout: float=?,
        ~client: queryClient,
        React.element
      ) =>
      Lwt.t((Lwt_stream.t(string), unit => unit));
  };
};
