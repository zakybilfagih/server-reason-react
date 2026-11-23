/* Prints what the server streams for each scenario of
   packages/react-query/test/hydration.test.mjs, as the batches of messages
   in the shell and in the chunks after it. Times are "<now>". */
open ReactQuery;

let string =
  Codec.make(
    ~encode=Melange_json.Primitives.string_to_json,
    ~decode=Melange_json.Primitives.string_of_json,
  );

let later = (value: Lwt.t(_)) =>
  Lwt.bind(Lwt_unix.sleep(0.01), () => value);

let query = (~name, fetch) =>
  Query.make(
    ~name,
    ~key=(id: int) => [Key.int(id)],
    ~codec=string,
    ~fetch=(~signal as _, _) => fetch(),
    (),
  );

module Read = {
  [@react.component]
  let make = (~query) =>
    React.string(SuspenseResult.data(useSuspenseQuery(query, 1)));
};

module ReadPages = {
  [@react.component]
  let make = (~query) =>
    React.string(
      String.concat(
        ",",
        Array.to_list(
          SuspenseInfiniteResult.data(useSuspenseInfiniteQuery(query, ())).
            pages,
        ),
      ),
    );
};

module Prefetch = {
  [@react.component]
  let make = (~query) => {
    usePrefetchQuery(query, 1);
    React.null;
  };
};

let scriptStart = "<script>(self.__RQ=self.__RQ||[]).push(";

let indexFrom = (haystack, from, needle) => {
  let rec go = index =>
    if (index + String.length(needle) > String.length(haystack)) {
      None;
    } else if (String.sub(haystack, index, String.length(needle)) == needle) {
      Some(index);
    } else {
      go(index + 1);
    };
  go(from);
};

let rec scrub = (json: Yojson.Basic.t) =>
  switch (json) {
  | `Assoc(fields) =>
    `Assoc(
      List.map(
        fun
        | ("dehydratedAt", _) => ("dehydratedAt", `String("<now>"))
        | (("dataUpdatedAt" | "errorUpdatedAt") as name, `Float(at))
            when at > 0. => (
            name,
            `String("<now>"),
          )
        | (name, value) => (name, scrub(value)),
        fields,
      ),
    )
  | `List(items) => `List(List.map(scrub, items))
  | json => json
  };

let batches = html => {
  let rec go = (from, found) =>
    switch (indexFrom(html, from, scriptStart)) {
    | None => List.rev(found)
    | Some(start) =>
      let body = start + String.length(scriptStart);
      let stop = Option.get(indexFrom(html, body, ")</script>"));
      go(
        stop,
        [
          scrub(
            Yojson.Basic.from_string(String.sub(html, body, stop - body)),
          ),
          ...found,
        ],
      );
    };
  go(0, []);
};

let render = (client, element) => {
  let%lwt (stream, _abort) =
    Server.renderToStream(
      ~client,
      <html>
        <head />
        <body>
          <QueryClient.Provider client> element </QueryClient.Provider>
        </body>
      </html>,
    );
  let%lwt chunks = Lwt_stream.to_list(stream);
  Lwt.return(
    `Assoc([
      ("shell", `List(batches(List.hd(chunks)))),
      ("later", `List(List.concat_map(batches, List.tl(chunks)))),
    ]),
  );
};

let pendingScenario = (name, fetch) => {
  let client = QueryClient.make();
  let query = query(~name, fetch);
  let _ = QueryClient.prefetch(client, query, 1);
  render(
    client,
    <React.Suspense fallback=React.null> <Read query /> </React.Suspense>,
  );
};

/* A loader prefetches into its own client, and the page passes the loader's
   dehydrated state to a [HydrationBoundary]. */
let boundaryScenario = (~name, ~cached) => {
  let client = QueryClient.make();
  let query = query(~name, () => Lwt.return("loader data"));
  if (cached) {
    QueryClient.setQueryData(client, query, 1, _ => Some("cached data"));
  };
  let%lwt () = Lwt_unix.sleep(0.005);
  let loader = QueryClient.make();
  let%lwt () = QueryClient.prefetch(loader, query, 1);
  render(
    client,
    <HydrationBoundary state={dehydrate(loader)}>
      React.null
    </HydrationBoundary>,
  );
};

/* The page's own fetch is still running when a loader's newer data is
   hydrated into it. */
let inFlightScenario = () => {
  let client = QueryClient.make();
  let query =
    query(~name="in-flight", () => later(Lwt.return("server data")));
  let _ = QueryClient.prefetch(client, query, 1);
  let loader = QueryClient.make();
  let%lwt () =
    QueryClient.prefetch(
      loader,
      Query.make(
        ~name="in-flight",
        ~key=(id: int) => [Key.int(id)],
        ~codec=string,
        ~fetch=(~signal as _, _) => Lwt.return("loader data"),
        (),
      ),
      1,
    );
  hydrate(client, dehydrate(loader));
  render(
    client,
    <React.Suspense fallback=React.null> <Read query /> </React.Suspense>,
  );
};

let scenarios = [
  (
    "resolved",
    () => {
      let client = QueryClient.make();
      let query = query(~name="resolved", () => Lwt.return("server data"));
      let%lwt () = QueryClient.prefetch(client, query, 1);
      render(client, React.null);
    },
  ),
  (
    "pending",
    () => pendingScenario("pending", () => later(Lwt.return("server data"))),
  ),
  (
    "stale",
    () => pendingScenario("stale", () => later(Lwt.return("server data"))),
  ),
  (
    "failing",
    () =>
      pendingScenario("failing", () => later(Lwt.fail(Failure("secret")))),
  ),
  (
    "retried",
    () =>
      pendingScenario("retried", () => later(Lwt.fail(Failure("secret")))),
  ),
  (
    "infinite",
    () => {
      let client = QueryClient.make();
      let query =
        InfiniteQuery.make(
          ~name="infinite",
          ~key=() => [],
          ~pageCodec=string,
          ~paramCodec=
            Codec.make(
              ~encode=Melange_json.Primitives.int_to_json,
              ~decode=Melange_json.Primitives.int_of_json,
            ),
          ~initialPageParam=0,
          ~fetchPage=
            (~signal as _, (), param) =>
              later(Lwt.return("page " ++ Int.to_string(param))),
          ~getNextPageParam=
            ({ pageParams, _ }) =>
              Some(pageParams[Array.length(pageParams) - 1] + 1),
          (),
        );
      let _ = QueryClient.prefetchInfinite(client, ~pages=2, query, ());
      render(
        client,
        <React.Suspense fallback=React.null>
          <ReadPages query />
        </React.Suspense>,
      );
    },
  ),
  ("boundary", () => boundaryScenario(~name="boundary", ~cached=false)),
  ("existing", () => boundaryScenario(~name="existing", ~cached=true)),
  ("in-flight", inFlightScenario),
  (
    "streamed",
    () => {
      let client = QueryClient.make();
      let chunks = ref(["a", "b", "c"]);
      let query =
        StreamedQuery.make(
          ~name="streamed",
          ~key=(id: int) => [Key.int(id)],
          ~codec=string,
          ~stream=
            (~signal as _, _) =>
              Lwt.return(
                Stream.make(() =>
                  later(
                    switch (chunks^) {
                    | [] => Lwt.return(None)
                    | [chunk, ...rest] =>
                      chunks := rest;
                      Lwt.return(Some(chunk));
                    },
                  )
                ),
              ),
          ~reducer=(++),
          ~initialValue="",
          (),
        );
      let _ = QueryClient.prefetch(client, query, 1);
      render(
        client,
        <React.Suspense fallback=React.null> <Read query /> </React.Suspense>,
      );
    },
  ),
  (
    "render-prefetch",
    () => {
      let client = QueryClient.make();
      let query =
        query(~name="render-prefetch", () =>
          later(Lwt.return("server data"))
        );
      render(client, <Prefetch query />);
    },
  ),
  (
    "outlives",
    () => {
      let client = QueryClient.make();
      let query =
        query(~name="outlives", () => later(Lwt.return("server data")));
      let _ = QueryClient.prefetch(client, query, 1);
      render(client, React.null);
    },
  ),
];

let () =
  Lwt_main.run(
    {
      let%lwt results =
        Lwt_list.map_s(
          ((name, run)) => Lwt.map(result => (name, result), run()),
          scenarios,
        );
      print_endline(Yojson.Basic.pretty_to_string(`Assoc(results)));
      Lwt.return();
    },
  );
