open ReactQuery;

let rec keyOfFixture = (json: Yojson.Basic.t) =>
  switch (json) {
  | `String(value) => Key.string(value)
  | `Int(value) => Key.int(value)
  | `Float(value) => Key.float(value)
  | `Bool(value) => Key.bool(value)
  | `Null => Key.null
  | `Assoc([("list", `List(values))]) =>
    Key.list(List.map(keyOfFixture, values))
  | `Assoc([("obj", `List(fields))]) =>
    Key.obj(
      List.map(
        fun
        | `List([`String(name), value]) => (name, keyOfFixture(value))
        | _ => failwith("malformed field"),
        fields,
      ),
    )
  | _ => failwith("malformed key")
  };

let hashMatchesTanStack = () =>
  Yojson.Basic.from_file("hash_key.json")
  |> Yojson.Basic.Util.to_list
  |> List.iter(case => {
       let key = Yojson.Basic.Util.member("key", case);
       let expected = Yojson.Basic.Util.(member("hash", case) |> to_string);
       let parts =
         switch (key) {
         | `Assoc([("list", `List(parts))]) =>
           List.map(keyOfFixture, parts)
         | _ => failwith("a key is a list")
         };
       Alcotest.(check(string))(expected, expected, Key.hash(parts));
     });

let codec =
  Codec.make(
    ~encode=Melange_json.Primitives.string_to_json,
    ~decode=Melange_json.Primitives.string_of_json,
  );

let query = (~name, fetch) =>
  Query.make(
    ~name,
    ~key=id => [Key.int(id)],
    ~codec,
    ~fetch=(~signal as _, id) => fetch(id),
    (),
  );

let render = (~timeout=?, client, body) => {
  let%lwt (stream, _abort) =
    Server.renderToStream(
      ~timeout?,
      ~client,
      <html>
        <head> <title> {React.string("Test")} </title> </head>
        <body>
          <QueryClient.Provider client> body </QueryClient.Provider>
        </body>
      </html>,
    );
  Lwt_stream.to_list(stream);
};

let indexOf = (haystack, needle) => {
  let rec go = index =>
    if (index + String.length(needle) > String.length(haystack)) {
      None;
    } else if (String.sub(haystack, index, String.length(needle)) == needle) {
      Some(index);
    } else {
      go(index + 1);
    };
  go(0);
};

let position = (html, needle) =>
  switch (indexOf(html, needle)) {
  | Some(index) => index
  | None => Alcotest.failf("expected %S in %s", needle, html)
  };

let scriptStart = "<script>(self.__RQ=self.__RQ||[]).push(";

/* Each streamed message with the position of its script. */
let messages = html => {
  let rec go = (from, found) =>
    switch (
      indexOf(
        String.sub(html, from, String.length(html) - from),
        scriptStart,
      )
    ) {
    | None => List.rev(found)
    | Some(offset) =>
      let start = from + offset;
      let body = start + String.length(scriptStart);
      let rest = String.sub(html, body, String.length(html) - body);
      let length = Option.get(indexOf(rest, ")</script>"));
      let batch =
        Yojson.Basic.from_string(String.sub(rest, 0, length))
        |> Yojson.Basic.Util.to_list
        |> List.map(message => (start, message));
      go(body + length, List.rev_append(batch, found));
    };
  go(0, []);
};

/* The position of the message about [hash] with [status], and [data] when
   given. */
let findMessage = (html, ~hash, ~status, ~data=?, ()) =>
  messages(html)
  |> List.find_map(((position, message)) => {
       open Yojson.Basic.Util;
       let state = member("state", message);
       member("queryHash", message) == `String(hash)
       && member("state", message)
       |> member("status") == `String(status)
       && (
         switch (data) {
         | Some(data) => member("data", state) == data
         | None => true
         }
       )
         ? Some(position) : None;
     });

let message = (html, ~hash, ~status, ~data=?, ()) =>
  switch (findMessage(html, ~hash, ~status, ~data?, ())) {
  | Some(position) => position
  | None =>
    Alcotest.failf("expected a %s message for %s in %s", status, hash, html)
  };

module Read = {
  [@react.component]
  let make = (~query, ~id) =>
    switch (QueryResult.status(useQuery(query, id))) {
    | Success(data) => React.string("success " ++ data)
    | Pending => React.string("pending")
    | Error(_) => React.string("error")
    };
};

module Suspended = {
  [@react.component]
  let make = (~query, ~id) =>
    React.string(SuspenseResult.data(useSuspenseQuery(query, id)));
};

let shellQueriesGoInTheHead = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let ready = query(~name="ready", _ => Lwt.return("now"));
      let never = query(~name="never", _ => fst(Lwt.wait()));
      let _ = QueryClient.prefetch(client, ready, 1);
      let _ = QueryClient.prefetch(client, never, 1);
      let%lwt chunks =
        render(
          ~timeout=20.,
          client,
          <> <Read query=ready id=1 /> <Read query=never id=1 /> </>,
        );
      let shell = List.hd(chunks);
      let head = position(shell, "</head>");
      Alcotest.(check(bool))(
        "ready data is in the head",
        true,
        message(
          shell,
          ~hash="[\"ready\",1]",
          ~status="success",
          ~data=`String("now"),
          (),
        )
        < head,
      );
      Alcotest.(check(bool))(
        "never is announced in the head",
        true,
        message(shell, ~hash="[\"never\",1]", ~status="pending", ()) < head,
      );
      Alcotest.(check(bool))(
        "useQuery reads settled data",
        true,
        position(shell, "success now") > head,
      );
      ignore(position(shell, "pending"));
      Alcotest.(check(option(int)))(
        "after the timeout the stream ends without never",
        None,
        findMessage(
          String.concat("", List.tl(chunks)),
          ~hash="[\"never\",1]",
          ~status="error",
          (),
        ),
      );
      Lwt.return();
    },
  );

let dataPrecedesItsBoundary = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let slow =
        query(~name="slow", id =>
          Lwt.map(() => "slow " ++ Int.to_string(id), Lwt_unix.sleep(0.01))
        );
      let%lwt chunks =
        render(
          client,
          <React.Suspense fallback={React.string("loading")}>
            <Suspended query=slow id=2 />
          </React.Suspense>,
        );
      let html = String.concat("", chunks);
      ignore(position(List.hd(chunks), "loading"));
      Alcotest.(check(bool))(
        "data before the boundary",
        true,
        message(
          html,
          ~hash="[\"slow\",2]",
          ~status="success",
          ~data=`String("slow 2"),
          (),
        )
        < position(html, "<div hidden id=\"S:"),
      );
      Lwt.return();
    },
  );

let failedQueriesFetchAgainInTheBrowser = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let broken = query(~name="broken", _ => Lwt.fail(Failure("secret")));
      let _ = QueryClient.prefetch(client, broken, 3);
      let%lwt chunks = render(client, <Read query=broken id=3 />);
      let html = String.concat("", chunks);
      ignore(message(html, ~hash="[\"broken\",3]", ~status="error", ()));
      ignore(position(html, "pending"));
      Alcotest.(check(option(int)))(
        "the error stays on the server",
        None,
        indexOf(html, "secret"),
      );
      Lwt.return();
    },
  );

let setQueryDataReplacesTheStreamedData = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let edited = query(~name="edited", _ => Lwt.return("old"));
      let%lwt () = QueryClient.prefetch(client, edited, 1);
      QueryClient.setQueryData(client, edited, 1, data =>
        Option.map(data => data ++ " new", data)
      );
      QueryClient.setQueryData(client, edited, 2, _ => None);
      Alcotest.(check(option(string)))(
        "the cache has the new data",
        Some("old new"),
        QueryClient.getQueryData(client, edited, 1),
      );
      Alcotest.(check(option(string)))(
        "None leaves a query unset",
        None,
        QueryClient.getQueryData(client, edited, 2),
      );
      let%lwt chunks = render(client, <Read query=edited id=1 />);
      let html = String.concat("", chunks);
      ignore(
        message(
          html,
          ~hash="[\"edited\",1]",
          ~status="success",
          ~data=`String("old new"),
          (),
        ),
      );
      ignore(position(html, "success old new"));
      Alcotest.(check(option(int)))(
        "the old data is not sent",
        None,
        findMessage(
          html,
          ~hash="[\"edited\",1]",
          ~status="success",
          ~data=`String("old"),
          (),
        ),
      );
      Lwt.return();
    },
  );

let cacheReadsAndWritesEveryKey = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let notes =
        query(~name="notes", id => Lwt.return("note " ++ Int.to_string(id)));
      let others = query(~name="others", _ => Lwt.return("other"));
      let%lwt one = QueryClient.fetch(client, notes, 1);
      let%lwt two = QueryClient.ensure(client, notes, 2);
      let%lwt _ = QueryClient.fetch(client, others, 1);
      Alcotest.(check(list(string)))(
        "fetch and ensure resolve with the data",
        ["note 1", "note 2"],
        [one, two],
      );
      QueryClient.setQueriesData(client, notes, data =>
        Option.map(String.uppercase_ascii, data)
      );
      Alcotest.(check(list(string)))(
        "setQueriesData changes only this query's keys",
        ["NOTE 1", "NOTE 2"],
        List.sort(
          compare,
          Array.to_list(QueryClient.getQueriesData(client, notes)),
        ),
      );
      Alcotest.(check(option(string)))(
        "other queries keep their data",
        Some("other"),
        QueryClient.getQueryData(client, others, 1),
      );
      let%lwt chunks = render(client, React.null);
      let html = String.concat("", chunks);
      ignore(
        message(
          html,
          ~hash="[\"notes\",2]",
          ~status="success",
          ~data=`String("NOTE 2"),
          (),
        ),
      );
      Lwt.return();
    },
  );

let fetchRejectsWithTheError = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let broken = query(~name="broken", _ => Lwt.fail(Failure("nope")));
      let%lwt result =
        Lwt.catch(
          () => Lwt.map(_ => None, QueryClient.fetch(client, broken, 1)),
          error => Lwt.return(Some(error)),
        );
      Alcotest.(check(bool))(
        "fetch rejects",
        true,
        result == Some(Failure("nope")),
      );
      Alcotest.(check(bool))(
        "the state has the error",
        true,
        switch (QueryClient.getQueryState(client, broken, 1)) {
        | Some({ error: Some(error), data: None, _ }) =>
          error == Failure("nope")
        | _ => false
        },
      );
      Lwt.return();
    },
  );

let pages =
  InfiniteQuery.make(
    ~name="pages",
    ~key=() => [],
    ~pageCodec=
      Codec.make(
        ~encode=Melange_json.Primitives.string_to_json,
        ~decode=Melange_json.Primitives.string_of_json,
      ),
    ~paramCodec=
      Codec.make(
        ~encode=Melange_json.Primitives.int_to_json,
        ~decode=Melange_json.Primitives.int_of_json,
      ),
    ~initialPageParam=10,
    ~fetchPage=
      (~signal as _, (), param) =>
        Lwt.return("page " ++ Int.to_string(param)),
    ~getNextPageParam=
      ({ pageParams, _ }) => {
        let last = pageParams[Array.length(pageParams) - 1];
        last < 12 ? Some(last + 1) : None;
      },
    (),
  );

module PageList = {
  [@react.component]
  let make = () => {
    let result = useInfiniteQuery(pages, ());
    switch (InfiniteResult.status(result)) {
    | Success({ pages, _ }) =>
      React.string(
        String.concat(",", Array.to_list(pages))
        ++ (InfiniteResult.hasNextPage(result) ? " more" : " end"),
      )
    | Pending
    | Error(_) => React.string("no pages")
    };
  };
};

let infinitePagesStreamWithTheirParams = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let%lwt () = QueryClient.prefetchInfinite(client, ~pages=2, pages, ());
      let%lwt chunks = render(client, <PageList />);
      let html = String.concat("", chunks);
      ignore(
        message(
          html,
          ~hash="[\"pages\"]",
          ~status="success",
          ~data=
            `Assoc([
              ("pages", `List([`String("page 10"), `String("page 11")])),
              ("pageParams", `List([`Int(10), `Int(11)])),
            ]),
          (),
        ),
      );
      Alcotest.(check(list(string)))(
        "an infinite query says so",
        ["infinite"],
        messages(html)
        |> List.filter_map(((_, message)) =>
             Yojson.Basic.Util.(
               member("queryType", message) |> to_string_option
             )
           ),
      );
      ignore(position(html, "page 10,page 11 more"));
      let%lwt all = QueryClient.fetchInfinite(client, ~pages=5, pages, ());
      Alcotest.(check(list(int)))(
        "a cached query is not fetched again",
        [10, 11],
        Array.to_list(all.pageParams),
      );
      let fresh = QueryClient.make();
      let%lwt all = QueryClient.fetchInfinite(fresh, ~pages=5, pages, ());
      Alcotest.(check(list(int)))(
        "pages stop when getNextPageParam returns None",
        [10, 11, 12],
        Array.to_list(all.pageParams),
      );
      Lwt.return();
    },
  );

let namesAreUnique = () => {
  let client = QueryClient.make();
  let _ =
    QueryClient.prefetch(
      client,
      query(~name="same", _ => Lwt.return("a")),
      1,
    );
  let other =
    Query.make(
      ~name="same",
      ~key=id => [Key.int(id)],
      ~codec=
        Codec.make(
          ~encode=Melange_json.Primitives.int_to_json,
          ~decode=Melange_json.Primitives.int_of_json,
        ),
      ~fetch=(~signal as _, id) => Lwt.return(id),
      (),
    );
  Alcotest.check_raises(
    "same name, other data",
    Invalid_argument("ReactQuery: two queries are named same"),
    () =>
    ignore(QueryClient.prefetch(client, other, 1))
  );
};

/* Each fetch reports how many fetches had started when it resolved. */
let countingQuery = (~name, started) =>
  query(
    ~name,
    _ => {
      incr(started);
      Lwt.map(() => name ++ Int.to_string(started^), Lwt_unix.sleep(0.01));
    },
  );

module SuspensePair = {
  [@react.component]
  let make = (~first, ~second) => {
    let (first, second) =
      useSuspenseQueries(
        SuspenseQueries.both(
          SuspenseQueries.one(first, 1),
          SuspenseQueries.one(second, 1),
        ),
      );
    React.string(
      SuspenseResult.data(first) ++ " " ++ SuspenseResult.data(second),
    );
  };
};

let suspenseQueriesStartTogether = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let started = ref(0);
      let first = countingQuery(~name="first", started);
      let second = countingQuery(~name="second", started);
      let%lwt chunks =
        render(
          client,
          <React.Suspense fallback={React.string("loading")}>
            <SuspensePair first second />
          </React.Suspense>,
        );
      let html = String.concat("", chunks);
      ignore(position(html, "first2 second2"));
      ignore(
        message(
          html,
          ~hash="[\"first\",1]",
          ~status="success",
          ~data=`String("first2"),
          (),
        ),
      );
      Lwt.return();
    },
  );

module Pair = {
  [@react.component]
  let make = (~ready, ~cold) => {
    let show = result =>
      switch (QueryResult.status(result)) {
      | Success(data) => data
      | Pending => "pending"
      | Error(_) => "error"
      };
    let (ready, cold) =
      useQueries(
        Queries.both(Queries.one(ready, 1), Queries.one(cold, 1)),
      );
    React.string(show(ready) ++ " and " ++ show(cold));
  };
};

let queriesReadEachQuery = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let ready = query(~name="ready-pair", _ => Lwt.return("ready"));
      let cold = query(~name="cold-pair", _ => Lwt.return("cold"));
      let%lwt () = QueryClient.prefetch(client, ready, 1);
      let%lwt chunks = render(client, <Pair ready cold />);
      ignore(position(String.concat("", chunks), "ready and pending"));
      Lwt.return();
    },
  );

let roundTrip = client =>
  DehydratedState.fromJson(
    Yojson.Basic.from_string(
      Yojson.Basic.to_string(DehydratedState.toJson(dehydrate(client))),
    ),
  );

let dehydratedQueriesHydrateAnotherClient = () =>
  Lwt_main.run(
    {
      let server = QueryClient.make();
      let notes =
        query(~name="hydrated", id =>
          Lwt.return("note " ++ Int.to_string(id))
        );
      let broken =
        query(~name="hydrated-broken", _ => Lwt.fail(Failure("no")));
      let%lwt () = QueryClient.prefetch(server, notes, 1);
      let%lwt () = QueryClient.prefetch(server, broken, 1);
      let%lwt () = QueryClient.prefetchInfinite(server, ~pages=2, pages, ());
      let state = roundTrip(server);
      Alcotest.(check(list(string)))(
        "failed queries are left out",
        ["[\"hydrated\",1]", "[\"pages\"]"],
        Yojson.Basic.Util.(
          DehydratedState.toJson(state)
          |> member("queries")
          |> to_list
          |> List.map(query => member("queryHash", query) |> to_string)
          |> List.sort(compare)
        ),
      );
      let browser = QueryClient.make();
      hydrate(browser, state);
      Alcotest.(check(option(string)))(
        "data",
        Some("note 1"),
        QueryClient.getQueryData(browser, notes, 1),
      );
      Alcotest.(check(option(list(string))))(
        "infinite pages",
        Some(["page 10", "page 11"]),
        Option.map(
          ({ pages, _ }: infiniteData(_, _)) => Array.to_list(pages),
          QueryClient.getInfiniteQueryData(browser, pages, ()),
        ),
      );
      Lwt.return();
    },
  );

let hydrateKeepsNewerData = () =>
  Lwt_main.run(
    {
      let notes = query(~name="dated", _ => Lwt.return("fetched"));
      let older = QueryClient.make();
      QueryClient.setQueryData(older, notes, 1, _ => Some("older"));
      let olderState = roundTrip(older);
      let%lwt () = Lwt_unix.sleep(0.005);
      let newer = QueryClient.make();
      QueryClient.setQueryData(newer, notes, 1, _ => Some("newer"));
      hydrate(newer, olderState);
      Alcotest.(check(option(string)))(
        "older data does not replace newer data",
        Some("newer"),
        QueryClient.getQueryData(newer, notes, 1),
      );
      hydrate(older, roundTrip(newer));
      Alcotest.(check(option(string)))(
        "newer data replaces older data",
        Some("newer"),
        QueryClient.getQueryData(older, notes, 1),
      );
      Lwt.return();
    },
  );

let hydrationBoundaryHydratesNewQueries = () =>
  Lwt_main.run(
    {
      let fresh =
        query(~name="boundary-fresh", _ => Lwt.return("from the loader"));
      let cached =
        query(~name="boundary-cached", _ => Lwt.return("from the loader"));
      let loader = QueryClient.make();
      let client = QueryClient.make();
      QueryClient.setQueryData(client, cached, 1, _ =>
        Some("already cached")
      );
      let%lwt () = Lwt_unix.sleep(0.005);
      let%lwt () = QueryClient.prefetch(loader, fresh, 1);
      let%lwt () = QueryClient.prefetch(loader, cached, 1);
      let%lwt chunks =
        render(
          client,
          <HydrationBoundary state={roundTrip(loader)}>
            <Read query=fresh id=1 />
            <Read query=cached id=1 />
          </HydrationBoundary>,
        );
      let html = String.concat("", chunks);
      ignore(position(html, "success from the loader"));
      ignore(
        message(
          html,
          ~hash="[\"boundary-fresh\",1]",
          ~status="success",
          ~data=`String("from the loader"),
          (),
        ),
      );
      ignore(position(html, "success already cached"));
      Alcotest.(check(option(string)))(
        "cached queries wait for an effect",
        Some("already cached"),
        QueryClient.getQueryData(client, cached, 1),
      );
      Lwt.return();
    },
  );

/* TanStack keeps data hydrated into a fetching query until the fetch
   settles, then the fetch's result replaces it. */
let hydrateWhileFetching = () =>
  Lwt_main.run(
    {
      let notes =
        query(~name="in-flight", _ =>
          Lwt.map(() => "fetched", Lwt_unix.sleep(0.02))
        );
      let client = QueryClient.make();
      let _ = QueryClient.prefetch(client, notes, 1);
      let loader = QueryClient.make();
      QueryClient.setQueryData(loader, notes, 1, _ => Some("hydrated"));
      hydrate(client, roundTrip(loader));
      Alcotest.(check(option(string)))(
        "the hydrated data while the fetch runs",
        Some("hydrated"),
        QueryClient.getQueryData(client, notes, 1),
      );
      let%lwt chunks =
        render(
          client,
          <React.Suspense fallback={React.string("loading")}>
            <Suspended query=notes id=1 />
          </React.Suspense>,
        );
      let shell = List.hd(chunks);
      ignore(position(shell, "hydrated"));
      Alcotest.(check(list(string)))(
        "the shell sends the hydrated data as fetching, once",
        ["fetching"],
        messages(String.concat("", chunks))
        |> List.map(((_, message)) =>
             Yojson.Basic.Util.(
               member("state", message) |> member("fetchStatus") |> to_string
             )
           ),
      );
      let%lwt () = Lwt_unix.sleep(0.03);
      Alcotest.(check(option(string)))(
        "the fetch's result replaces it",
        Some("fetched"),
        QueryClient.getQueryData(client, notes, 1),
      );
      Lwt.return();
    },
  );

module ReadAfter = {
  [@react.component]
  let make = (~first, ~second) => {
    let first = SuspenseResult.data(useSuspenseQuery(first, 1));
    let second = SuspenseResult.data(useSuspenseQuery(second, 1));
    React.string(first ++ " " ++ second);
  };
};

let setWhileAnnouncedFetchRuns = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let gate =
        query(~name="gate", _ =>
          Lwt.map(() => "gate", Lwt_unix.sleep(0.01))
        );
      let slow =
        query(~name="announced", _ =>
          Lwt.map(() => "fetched", Lwt_unix.sleep(0.02))
        );
      let late =
        query(~name="late", _ =>
          Lwt.map(() => "late", Lwt_unix.sleep(0.03))
        );
      let _ = QueryClient.prefetch(client, slow, 1);
      Lwt.async(() => {
        let%lwt () = Lwt_unix.sleep(0.005);
        QueryClient.setQueryData(client, slow, 1, _ => Some("set"));
        Lwt.return();
      });
      let%lwt chunks =
        render(
          client,
          <>
            <React.Suspense fallback={React.string("loading")}>
              <ReadAfter first=gate second=slow />
            </React.Suspense>
            <React.Suspense fallback={React.string("loading")}>
              <Suspended query=late id=1 />
            </React.Suspense>
          </>,
        );
      let html = String.concat("", chunks);
      ignore(
        message(html, ~hash="[\"announced\",1]", ~status="pending", ()),
      );
      ignore(
        message(
          html,
          ~hash="[\"announced\",1]",
          ~status="success",
          ~data=`String("fetched"),
          (),
        ),
      );
      Alcotest.(check(option(int)))(
        "the browser waiting for the fetch does not get the set data",
        None,
        findMessage(
          html,
          ~hash="[\"announced\",1]",
          ~status="success",
          ~data=`String("set"),
          (),
        ),
      );
      Lwt.return();
    },
  );

let prefetchesOutliveTheRender = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let slow =
        query(~name="outlives", _ =>
          Lwt.map(() => "late data", Lwt_unix.sleep(0.02))
        );
      let _ = QueryClient.prefetch(client, slow, 1);
      let%lwt chunks = render(client, React.string("done"));
      let html = String.concat("", chunks);
      Alcotest.(check(bool))(
        "the data follows the end of the render",
        true,
        message(
          html,
          ~hash="[\"outlives\",1]",
          ~status="success",
          ~data=`String("late data"),
          (),
        )
        > position(html, "</html>"),
      );
      Lwt.return();
    },
  );

let abortStopsWaiting = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let never = query(~name="aborted", _ => fst(Lwt.wait()));
      let _ = QueryClient.prefetch(client, never, 1);
      let%lwt (stream, abort) =
        Server.renderToStream(~client, React.string("done"));
      Lwt.async(() => Lwt.map(abort, Lwt_unix.sleep(0.01)));
      let%lwt ended =
        Lwt.pick([
          Lwt.map(_ => true, Lwt_stream.to_list(stream)),
          Lwt.map(() => false, Lwt_unix.sleep(1.)),
        ]);
      Alcotest.(check(bool))("the stream ends", true, ended);
      Lwt.return();
    },
  );

module Sequential = {
  [@react.component]
  let make = (~first, ~second) => {
    let first = SuspenseResult.data(useSuspenseQuery(first, 1));
    let second = SuspenseResult.data(useSuspenseQuery(second, 1));
    React.string(first ++ " " ++ second);
  };
};

module PrefetchSecond = {
  [@react.component]
  let make = (~first, ~second) => {
    usePrefetchQuery(second, 1);
    <React.Suspense fallback={React.string("loading")}>
      <Sequential first second />
    </React.Suspense>;
  };
};

let prefetchInRenderStartsEarly = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let started = ref(0);
      let first = countingQuery(~name="first-read", started);
      let second = countingQuery(~name="prefetched", started);
      let%lwt chunks = render(client, <PrefetchSecond first second />);
      let html = String.concat("", chunks);
      ignore(position(html, "first-read2 prefetched2"));
      ignore(
        message(html, ~hash="[\"prefetched\",1]", ~status="pending", ()),
      );
      Lwt.return();
    },
  );

/* Each chunk after a short wait, so the test sees the data grow. */
let slowStream = chunks => {
  let index = ref(0);
  Stream.make(() =>
    Lwt.map(
      () =>
        if (index^ < Array.length(chunks)) {
          incr(index);
          Some(chunks[index^ - 1]);
        } else {
          None;
        },
      Lwt_unix.sleep(0.005),
    )
  );
};

let words =
  StreamedQuery.make(
    ~name="words",
    ~key=(id: int) => [Key.int(id)],
    ~codec,
    ~stream=(~signal as _, _) => Lwt.return(slowStream([|"a", "b", "c"|])),
    ~reducer=(++),
    ~initialValue="",
    (),
  );

let streamedQueriesFillTheCache = () =>
  Lwt_main.run(
    {
      let early = QueryClient.make();
      let _ = QueryClient.prefetch(early, words, 1);
      let%lwt () = Lwt_unix.sleep(0.012);
      Alcotest.(check(option(string)))(
        "the chunks so far are in the cache",
        Some("ab"),
        QueryClient.getQueryData(early, words, 1),
      );
      let client = QueryClient.make();
      let%lwt chunks =
        render(
          client,
          <React.Suspense fallback={React.string("loading")}>
            <Suspended query=words id=1 />
          </React.Suspense>,
        );
      let html = String.concat("", chunks);
      ignore(position(html, "abc"));
      Alcotest.(check(list(int)))(
        "the stream sends the whole data once, as one update per chunk and the result",
        [4],
        messages(html)
        |> List.filter_map(((_, message)) =>
             Yojson.Basic.Util.(
               member("state", message)
               |> member("status") == `String("success")
                 ? Some(
                     member("state", message)
                     |> member("dataUpdateCount")
                     |> to_int,
                   )
                 : None
             )
           ),
      );
      ignore(
        message(
          html,
          ~hash="[\"words\",1]",
          ~status="success",
          ~data=`String("abc"),
          (),
        ),
      );
      Lwt.return();
    },
  );

module Restoring = {
  [@react.component]
  let make = () =>
    React.string(useIsRestoring() ? "restoring" : "not restoring");
};

let persistProviderRendersRestoring = () =>
  Lwt_main.run(
    {
      let client = QueryClient.make();
      let%lwt chunks =
        render(
          client,
          <>
            <Restoring />
            <PersistQueryClientProvider
              client persister={Persister.syncStorage()}>
              <Restoring />
            </PersistQueryClientProvider>
          </>,
        );
      let html = String.concat("", chunks);
      Alcotest.(check(bool))(
        "only under the provider",
        true,
        position(html, "not restoring") < position(html, ">restoring"),
      );
      Lwt.return();
    },
  );

Alcotest.run(
  "ReactQuery",
  [
    (
      "Key",
      [
        Alcotest.test_case(
          "hash matches TanStack",
          `Quick,
          hashMatchesTanStack,
        ),
      ],
    ),
    (
      "Server",
      [
        Alcotest.test_case(
          "shell queries go in the head",
          `Quick,
          shellQueriesGoInTheHead,
        ),
        Alcotest.test_case(
          "data precedes its boundary",
          `Quick,
          dataPrecedesItsBoundary,
        ),
        Alcotest.test_case(
          "failed queries fetch again in the browser",
          `Quick,
          failedQueriesFetchAgainInTheBrowser,
        ),
        Alcotest.test_case(
          "setQueryData replaces the streamed data",
          `Quick,
          setQueryDataReplacesTheStreamedData,
        ),
        Alcotest.test_case(
          "cache reads and writes every key",
          `Quick,
          cacheReadsAndWritesEveryKey,
        ),
        Alcotest.test_case(
          "fetch rejects with the error",
          `Quick,
          fetchRejectsWithTheError,
        ),
        Alcotest.test_case(
          "infinite pages stream with their params",
          `Quick,
          infinitePagesStreamWithTheirParams,
        ),
        Alcotest.test_case("names are unique", `Quick, namesAreUnique),
        Alcotest.test_case(
          "PersistQueryClientProvider renders as restoring",
          `Quick,
          persistProviderRendersRestoring,
        ),
        Alcotest.test_case(
          "streamed queries fill the cache",
          `Quick,
          streamedQueriesFillTheCache,
        ),
        Alcotest.test_case(
          "prefetches outlive the render",
          `Quick,
          prefetchesOutliveTheRender,
        ),
        Alcotest.test_case("abort stops waiting", `Quick, abortStopsWaiting),
      ],
    ),
    (
      "Hydration",
      [
        Alcotest.test_case(
          "useSuspenseQueries starts every query at once",
          `Quick,
          suspenseQueriesStartTogether,
        ),
        Alcotest.test_case(
          "usePrefetchQuery starts a query during render",
          `Quick,
          prefetchInRenderStartsEarly,
        ),
        Alcotest.test_case(
          "useQueries reads each query",
          `Quick,
          queriesReadEachQuery,
        ),
        Alcotest.test_case(
          "dehydrated queries hydrate another client",
          `Quick,
          dehydratedQueriesHydrateAnotherClient,
        ),
        Alcotest.test_case(
          "hydrate keeps newer data",
          `Quick,
          hydrateKeepsNewerData,
        ),
        Alcotest.test_case(
          "HydrationBoundary hydrates new queries",
          `Quick,
          hydrationBoundaryHydratesNewQueries,
        ),
        Alcotest.test_case(
          "hydrate while fetching keeps the fetch's result",
          `Quick,
          hydrateWhileFetching,
        ),
        Alcotest.test_case(
          "data set while an announced fetch runs",
          `Quick,
          setWhileAnnouncedFetchRuns,
        ),
      ],
    ),
  ],
);
