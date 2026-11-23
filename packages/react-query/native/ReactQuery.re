module Key = {
  type t =
    | String(string)
    | Number(float)
    | Bool(bool)
    | Null
    | List(list(t))
    | Obj(list((string, t)));

  let string = value => String(value);
  let int = value => Number(Float.of_int(value));
  let float = value => Number(value);
  let bool = value => Bool(value);
  let null = Null;
  let list = values => List(values);
  let obj = fields => Obj(fields);

  /* JS objects list array-index keys first, in numeric order. */
  let arrayIndex = name =>
    switch (int_of_string_opt(name)) {
    | Some(index)
        when index >= 0 && index < 4294967295 && Int.to_string(index) == name =>
      Some(index)
    | _ => None
    };

  let compareFields = ((a, _), (b, _)) =>
    switch (arrayIndex(a), arrayIndex(b)) {
    | (Some(a), Some(b)) => Int.compare(a, b)
    | (Some(_), None) => (-1)
    | (None, Some(_)) => 1
    | (None, None) => String.compare(a, b)
    };

  let lastOfEachName = fields =>
    List.fold_left(
      (kept, (name, _) as field) =>
        List.mem_assoc(name, kept) ? kept : [field, ...kept],
      [],
      List.rev(fields),
    );

  let rec write = buffer =>
    fun
    | String(value) =>
      Buffer.add_string(buffer, Js.Json.stringify(Js.Json.string(value)))
    | Number(value) =>
      Buffer.add_string(buffer, Js.Json.stringify(Js.Json.number(value)))
    | Bool(value) => Buffer.add_string(buffer, value ? "true" : "false")
    | Null => Buffer.add_string(buffer, "null")
    | List(values) => {
        Buffer.add_char(buffer, '[');
        List.iteri(
          (index, value) => {
            if (index > 0) {
              Buffer.add_char(buffer, ',');
            };
            write(buffer, value);
          },
          values,
        );
        Buffer.add_char(buffer, ']');
      }
    | Obj(fields) => {
        Buffer.add_char(buffer, '{');
        List.iteri(
          (index, (name, value)) => {
            if (index > 0) {
              Buffer.add_char(buffer, ',');
            };
            write(buffer, String(name));
            Buffer.add_char(buffer, ':');
            write(buffer, value);
          },
          List.stable_sort(compareFields, lastOfEachName(fields)),
        );
        Buffer.add_char(buffer, '}');
      };

  let hash = parts => {
    let buffer = Buffer.create(64);
    write(buffer, List(parts));
    Buffer.contents(buffer);
  };
};

module Codec = {
  type t('data) = {
    encode: 'data => Melange_json.t,
    decode: Melange_json.t => 'data,
  };
  let make = (~encode, ~decode) => {
    encode,
    decode,
  };
};

module StaleTime = {
  type t =
    | Ms(float)
    | Infinite
    | Static;
};

module Retry = {
  type t =
    | Never
    | Times(int)
    | When((int, exn) => bool);
};

module NetworkMode = {
  type t =
    | Online
    | Always
    | OfflineFirst;
};

type entry =
  | Entry(Type.Id.t('data), Lwt.t('data), 'data => Melange_json.t): entry;

/* How [hydrate] puts a query's JSON data in the cache, by query name, and
   whether the query is infinite. */
let decoders: Hashtbl.t(string, (bool, Melange_json.t => entry)) =
  Hashtbl.create(8);

/* The query's data in the cache, for a fetch that writes to it as it goes. */
type cache('data) = {
  read: unit => option('data),
  write: 'data => unit,
};

module Query = {
  type t('key, 'data) = {
    name: string,
    key: 'key => list(Key.t),
    codec: Codec.t('data),
    fetch:
      (~signal: Fetch.signal, ~cache: cache('data), 'key) =>
      Js.Promise.t('data),
    id: Type.Id.t('data),
  };

  let custom = (~name, ~key, ~codec, ~fetch) => {
    let id = Type.Id.make();
    Hashtbl.replace(
      decoders,
      name,
      (
        false,
        json =>
          Entry(id, Lwt.return(codec.Codec.decode(json)), codec.encode),
      ),
    );
    {
      name,
      key,
      codec,
      fetch,
      id,
    };
  };

  let make =
      (
        ~name,
        ~key,
        ~codec,
        ~fetch,
        ~staleTime as _: option(StaleTime.t)=?,
        ~gcTime as _: option(float)=?,
        ~retry as _: option(Retry.t)=?,
        ~retryDelay as _: option((int, exn) => float)=?,
        ~networkMode as _: option(NetworkMode.t)=?,
        (),
      ) =>
    custom(~name, ~key, ~codec, ~fetch=(~signal, ~cache as _, key) =>
      fetch(~signal, key)
    );

  let hash = (query, key) =>
    Key.hash([Key.string(query.name), ...query.key(key)]);
};

module Stream = {
  type t('chunk) = unit => Js.Promise.t(option('chunk));
  let make = next => next;
  let fromArray = chunks => {
    let index = ref(0);
    () =>
      if (index^ < Array.length(chunks)) {
        incr(index);
        Lwt.return(Some(chunks[index^ - 1]));
      } else {
        Lwt.return(None);
      };
  };
};

module StreamedQuery = {
  type refetchMode =
    | Reset
    | Append
    | Replace;

  /* TanStack's [streamedQuery] on its first fetch, the only one a request
     makes: each chunk goes through the cache. */
  let make =
      (
        ~name,
        ~key,
        ~codec,
        ~stream,
        ~reducer,
        ~initialValue,
        ~refetchMode as _: option(refetchMode)=?,
        ~staleTime as _: option(StaleTime.t)=?,
        ~gcTime as _: option(float)=?,
        ~retry as _: option(Retry.t)=?,
        ~retryDelay as _: option((int, exn) => float)=?,
        ~networkMode as _: option(NetworkMode.t)=?,
        (),
      ) =>
    Query.custom(
      ~name,
      ~key,
      ~codec,
      ~fetch=(~signal, ~cache, key) => {
        let current = () => Option.value(cache.read(), ~default=initialValue);
        let%lwt next = stream(~signal, key);
        let rec pull = () =>
          switch%lwt (next()) {
          | None => Lwt.return()
          | Some(chunk) =>
            cache.write(reducer(current(), chunk));
            pull();
          };
        let%lwt () = pull();
        Lwt.return(current());
      },
    );
};

type infiniteData('page, 'param) = {
  pages: array('page),
  pageParams: array('param),
};

module InfiniteQuery = {
  type t('key, 'page, 'param) = {
    name: string,
    key: 'key => list(Key.t),
    encode: infiniteData('page, 'param) => Melange_json.t,
    initialPageParam: 'param,
    fetchPage: (~signal: Fetch.signal, 'key, 'param) => Js.Promise.t('page),
    getNextPageParam: infiniteData('page, 'param) => option('param),
    getPreviousPageParam:
      option(infiniteData('page, 'param) => option('param)),
    maxPages: option(int),
    id: Type.Id.t(infiniteData('page, 'param)),
  };

  let make =
      (
        ~name,
        ~key,
        ~pageCodec,
        ~paramCodec,
        ~initialPageParam,
        ~fetchPage,
        ~getNextPageParam,
        ~getPreviousPageParam=?,
        ~maxPages=?,
        ~staleTime as _: option(StaleTime.t)=?,
        ~gcTime as _: option(float)=?,
        ~retry as _: option(Retry.t)=?,
        ~retryDelay as _: option((int, exn) => float)=?,
        ~networkMode as _: option(NetworkMode.t)=?,
        (),
      ) => {
    let id = Type.Id.make();
    let encode = ({ pages, pageParams }) =>
      `Assoc([
        (
          "pages",
          `List(Array.to_list(Array.map(pageCodec.Codec.encode, pages))),
        ),
        (
          "pageParams",
          `List(
            Array.to_list(Array.map(paramCodec.Codec.encode, pageParams)),
          ),
        ),
      ]);
    let items = (name, decode, json) =>
      Yojson.Basic.Util.(member(name, json) |> to_list)
      |> List.map(decode)
      |> Array.of_list;
    Hashtbl.replace(
      decoders,
      name,
      (
        true,
        json =>
          Entry(
            id,
            Lwt.return({
              pages: items("pages", pageCodec.decode, json),
              pageParams: items("pageParams", paramCodec.decode, json),
            }),
            encode,
          ),
      ),
    );
    {
      name,
      key,
      encode,
      initialPageParam,
      fetchPage,
      getNextPageParam,
      getPreviousPageParam,
      maxPages,
      id,
    };
  };

  let hash = (query, key) =>
    Key.hash([Key.string(query.name), ...query.key(key)]);

  let hasNextPage = (query, data) =>
    Array.length(data.pages) > 0
    && Option.is_some(query.getNextPageParam(data));

  let hasPreviousPage = (query, data) =>
    Array.length(data.pages) > 0
    && (
      switch (query.getPreviousPageParam) {
      | Some(getPreviousPageParam) =>
        Option.is_some(getPreviousPageParam(data))
      | None => false
      }
    );

  let keepLast = (maxPages, items) =>
    switch (maxPages) {
    | Some(max) when max > 0 && Array.length(items) > max =>
      Array.sub(items, Array.length(items) - max, max)
    | _ => items
    };

  /* Fetches up to [pages] pages one after the other, like the browser. */
  let fetchPages = (query, ~signal, ~pages, key) => {
    let rec next = (data, param, remaining) => {
      let%lwt page = query.fetchPage(~signal, key, param);
      let data = {
        pages: keepLast(query.maxPages, Array.append(data.pages, [|page|])),
        pageParams:
          keepLast(
            query.maxPages,
            Array.append(data.pageParams, [|param|]),
          ),
      };
      switch (remaining > 1 ? query.getNextPageParam(data) : None) {
      | Some(param) => next(data, param, remaining - 1)
      | None => Lwt.return(data)
      };
    };
    next(
      {
        pages: [||],
        pageParams: [||],
      },
      query.initialPageParam,
      pages,
    );
  };
};

type status('data) =
  | Pending
  | Error(exn)
  | Success('data);

type fetchStatus =
  | Fetching
  | Paused
  | Idle;

module Filter = {
  type t = unit;
  let exact = (_, _) => ();
  let query = _ => ();
  let exactInfinite = (_, _) => ();
  let infinite = _ => ();
  let all = ();
  let where =
      (
        ~active as _: option(bool)=?,
        ~stale as _: option(bool)=?,
        ~fetchStatus as _: option(fetchStatus)=?,
        (),
      ) =>
    ();
};

module Refetch = {
  type t =
    | Active
    | Inactive
    | All
    | Never;
};

type queryState('data) = {
  data: option('data),
  error: option(exn),
  dataUpdatedAt: float,
  errorUpdatedAt: float,
  fetchStatus,
  isInvalidated: bool,
};

/* Cache events never fire on the server, so these are never built. */
module CachedQuery = {
  type t = {
    hash: string,
    hasData: bool,
  };
  let name = ({ hash, _ }) =>
    switch (Yojson.Basic.from_string(hash)) {
    | `List([`String(name), ..._]) => name
    | _
    | exception _ => ""
    };
  let hash = query => query.hash;
  let hasData = query => query.hasData;
};

module CachedMutation = {
  type t = string;
  let name = name => name;
};

/* [announced] holds the prefetches the browser was told are pending and has
   not yet heard the outcome of. [settled] holds the queries whose outcome is
   queued, so each is sent once. [infinite] holds the infinite queries.
   [updatedAt] and [updateCount] hold when each query's data arrived and how
   many times. [fetching] holds the queries whose fetch is running, and
   whether their data was replaced meanwhile.
   [queued] holds each query's latest message by hash. Once the shell is
   out, [send] writes queued messages to the stream right away. */
type client = {
  queries: Hashtbl.t(string, entry),
  infinite: Hashtbl.t(string, unit),
  updatedAt: Hashtbl.t(string, float),
  updateCount: Hashtbl.t(string, int),
  fetching: Hashtbl.t(string, bool),
  announced: Hashtbl.t(string, unit),
  settled: Hashtbl.t(string, unit),
  settledOne: Lwt_condition.t(unit),
  mutable queued: list((string, Yojson.Basic.t)),
  mutable send: option(string => unit),
};

let script = messages => {
  let json = Yojson.Basic.to_string(`List(List.rev_map(snd, messages)));
  let escaped = Buffer.create(String.length(json) + 48);
  Buffer.add_string(escaped, "<script>(self.__RQ=self.__RQ||[]).push(");
  String.iter(
    fun
    | '<' => Buffer.add_string(escaped, "\\u003c")
    | character => Buffer.add_char(escaped, character),
    json,
  );
  Buffer.add_string(escaped, ")</script>");
  Buffer.contents(escaped);
};

let drain = client =>
  switch (client.queued) {
  | [] => ""
  | messages =>
    client.queued = [];
    script(messages);
  };

let now = () => Unix.gettimeofday() *. 1000.;

/* A query in the shape of TanStack's [dehydrateQuery], for its [hydrate] in
   the browser. A pending query's result follows in a later message. The
   hash is the key as JSON, so it is also the key. */
let dehydrated = (client, ~queryHash, ~status, ~fetching=false, ~data=?, ()) => {
  let at = now();
  let success = status == "success";
  `Assoc(
    [
      ("queryKey", Yojson.Basic.from_string(queryHash)),
      ("queryHash", `String(queryHash)),
      ("dehydratedAt", `Float(at)),
      (
        "state",
        `Assoc(
          Option.fold(~none=[], ~some=data => [("data", data)], data)
          @ [
            (
              "dataUpdateCount",
              `Int(
                Option.value(
                  Hashtbl.find_opt(client.updateCount, queryHash),
                  ~default=0,
                ),
              ),
            ),
            (
              "dataUpdatedAt",
              `Float(
                success
                  ? Option.value(
                      Hashtbl.find_opt(client.updatedAt, queryHash),
                      ~default=at,
                    )
                  : 0.,
              ),
            ),
            ("error", `Null),
            ("errorUpdateCount", `Int(status == "error" ? 1 : 0)),
            ("errorUpdatedAt", `Float(status == "error" ? at : 0.)),
            ("fetchFailureCount", `Int(0)),
            ("fetchFailureReason", `Null),
            ("fetchMeta", `Null),
            ("isInvalidated", `Bool(false)),
            ("status", `String(status)),
            (
              "fetchStatus",
              `String(status == "pending" || fetching ? "fetching" : "idle"),
            ),
          ],
        ),
      ),
    ]
    @ (
      Hashtbl.mem(client.infinite, queryHash)
        ? [("queryType", `String("infinite"))] : []
    ),
  );
};

let queue = (client, ~queryHash, ~status, ~fetching=?, ~data=?, ()) => {
  client.queued = [
    (
      queryHash,
      dehydrated(client, ~queryHash, ~status, ~fetching?, ~data?, ()),
    ),
    ...List.filter(((queued, _)) => queued != queryHash, client.queued),
  ];
  switch (client.send) {
  | Some(send) => send(drain(client))
  | None => ()
  };
};

/* Hydrated data keeps its time and count; any other data is one more
   update, now. */
let record = (client, ~queryHash, ~updatedAt=?, ~count=?, ()) => {
  Hashtbl.replace(
    client.updatedAt,
    queryHash,
    Option.value(updatedAt, ~default=now()),
  );
  Hashtbl.replace(
    client.updateCount,
    queryHash,
    switch (count) {
    | Some(count) => count
    | None =>
      Option.value(
        Hashtbl.find_opt(client.updateCount, queryHash),
        ~default=0,
      )
      + 1
    },
  );
};

let settle =
    (client, ~queryHash, ~status, ~updatedAt=?, ~count=?, ~data=?, ()) =>
  if (!Hashtbl.mem(client.settled, queryHash)) {
    Hashtbl.add(client.settled, queryHash, ());
    if (status == "success") {
      record(client, ~queryHash, ~updatedAt?, ~count?, ());
    };
    Hashtbl.remove(client.announced, queryHash);
    queue(client, ~queryHash, ~status, ~data?, ());
    Lwt_condition.broadcast(client.settledOne, ());
  };

/* The server's error stays on the server, like TanStack's default
   [shouldRedactErrors]: the browser gets [Error("redacted")]. */
let fail = (client, ~queryHash) =>
  settle(client, ~queryHash, ~status="error", ());

let succeed = (client, ~encode, ~queryHash, ~updatedAt=?, ~count=?, data) =>
  switch (encode(data)) {
  | data =>
    settle(
      client,
      ~queryHash,
      ~status="success",
      ~updatedAt?,
      ~count?,
      ~data,
      (),
    )
  | exception _ => fail(client, ~queryHash)
  };

let find =
    (type data, client, ~id: Type.Id.t(data), ~name, queryHash)
    : option(Lwt.t(data)) =>
  switch (Hashtbl.find_opt(client.queries, queryHash)) {
  | Some(Entry(other, promise, _)) =>
    switch (Type.Id.provably_equal(id, other)) {
    | Some(Type.Equal) => Some(promise)
    | None => invalid_arg("ReactQuery: two queries are named " ++ name)
    }
  | None => None
  };

/* Runs a query once per request. Its outcome is queued as soon as it
   settles, so its data streams even when no component reads it. */
let run = (client, ~id, ~name, ~encode, ~infinite, ~queryHash, fetch) =>
  switch (find(client, ~id, ~name, queryHash)) {
  | Some(promise) => promise
  | None =>
    if (infinite) {
      Hashtbl.replace(client.infinite, queryHash, ());
    };
    /* In the cache before it starts, so the fetch can write to it. */
    let (promise, resolver) = Lwt.wait();
    Hashtbl.add(client.queries, queryHash, Entry(id, promise, encode));
    Hashtbl.replace(client.fetching, queryHash, false);
    let finish = () => {
      let replaced =
        Hashtbl.find_opt(client.fetching, queryHash) == Some(true);
      Hashtbl.remove(client.fetching, queryHash);
      replaced;
    };
    Lwt.on_any(
      promise,
      data => {
        /* Like TanStack's setData, the result replaces data set while the
           fetch ran. A shell not sent yet carries the result; one that
           carried the replaced data without a promise is not told again. */
        if (finish()) {
          Hashtbl.replace(
            client.queries,
            queryHash,
            Entry(id, Lwt.return(data), encode),
          );
          if (Option.is_none(client.send)) {
            Hashtbl.remove(client.settled, queryHash);
          };
          if (Hashtbl.mem(client.settled, queryHash)) {
            record(client, ~queryHash, ());
          };
        };
        succeed(client, ~encode, ~queryHash, data);
      },
      _ => {
        ignore(finish());
        fail(client, ~queryHash);
      },
    );
    let signal = Fetch.AbortController.(signal(make()));
    switch (fetch(~signal)) {
    | fetched =>
      Lwt.on_any(fetched, Lwt.wakeup(resolver), Lwt.wakeup_exn(resolver))
    | exception error => Lwt.wakeup_exn(resolver, error)
    };
    promise;
  };

let startInfinite =
    (client, ~pages=1, query: InfiniteQuery.t('key, 'page, 'param), key) => {
  let queryHash = InfiniteQuery.hash(query, key);
  (
    queryHash,
    run(
      client,
      ~id=query.id,
      ~name=query.name,
      ~encode=query.encode,
      ~infinite=true,
      ~queryHash,
      (~signal) =>
      InfiniteQuery.fetchPages(query, ~signal, ~pages, key)
    ),
  );
};

/* Tells the browser to wait for this server's result. */
let announce = (client, ~queryHash, promise) =>
  if (Lwt.is_sleeping(promise) && !Hashtbl.mem(client.announced, queryHash)) {
    Hashtbl.add(client.announced, queryHash, ());
    queue(client, ~queryHash, ~status="pending", ());
  };

let ignoreFailure = promise =>
  Lwt.catch(() => Lwt.map(_ => (), promise), _ => Lwt.return());

let promiseData = promise =>
  switch (Lwt.state(promise)) {
  | Lwt.Return(data) => Some(data)
  | Lwt.Fail(_)
  | Lwt.Sleep => None
  };

/* The new data replaces the query's outcome in the stream, including one
   already sent, and wins over a fetch that settles later. */
/* Puts [data] in the cache, for [setQueryData] and [hydrate]. While the
   query's own fetch runs, its result decides what a browser already waiting
   gets; a shell not sent yet carries [data] as fetching, without a promise,
   like TanStack's dehydrate. */
let replace =
    (
      client,
      ~id,
      ~encode,
      ~infinite,
      ~queryHash,
      ~updatedAt=?,
      ~count=?,
      data,
    ) => {
  if (infinite) {
    Hashtbl.replace(client.infinite, queryHash, ());
  };
  Hashtbl.replace(
    client.queries,
    queryHash,
    Entry(id, Lwt.return(data), encode),
  );
  if (Hashtbl.mem(client.fetching, queryHash)) {
    Hashtbl.replace(client.fetching, queryHash, true);
    record(client, ~queryHash, ~updatedAt?, ~count?, ());
    switch (client.send, encode(data)) {
    | (None, json) =>
      Hashtbl.remove(client.announced, queryHash);
      Hashtbl.replace(client.settled, queryHash, ());
      queue(
        client,
        ~queryHash,
        ~status="success",
        ~fetching=true,
        ~data=json,
        (),
      );
    | (Some(_), _)
    | exception _ => ()
    };
  } else {
    Hashtbl.remove(client.settled, queryHash);
    succeed(client, ~encode, ~queryHash, ~updatedAt?, ~count?, data);
  };
};

let write = (client, ~id, ~encode, ~infinite, ~queryHash, update, current) =>
  switch (update(current)) {
  | Some(data) => replace(client, ~id, ~encode, ~infinite, ~queryHash, data)
  | None => ()
  };

let start = (client, query: Query.t('key, 'data), key) => {
  let queryHash = Query.hash(query, key);
  let cache = {
    read: () =>
      Option.bind(
        find(client, ~id=query.id, ~name=query.name, queryHash),
        promiseData,
      ),
    write: data =>
      replace(
        client,
        ~id=query.id,
        ~encode=query.codec.encode,
        ~infinite=false,
        ~queryHash,
        data,
      ),
  };
  (
    queryHash,
    run(
      client,
      ~id=query.id,
      ~name=query.name,
      ~encode=query.codec.encode,
      ~infinite=false,
      ~queryHash,
      (~signal) =>
      query.fetch(~signal, ~cache, key)
    ),
  );
};

module QueryClient = {
  type t = client;

  let cachedQueries = client =>
    Hashtbl.fold(
      (hash, Entry(_, promise, _), queries) =>
        [
          {
            CachedQuery.hash,
            hasData:
              switch (Lwt.state(promise)) {
              | Return(_) => true
              | Fail(_)
              | Sleep => false
              },
          },
          ...queries,
        ],
      client.queries,
      [],
    )
    |> Array.of_list;

  let make =
      (
        ~staleTime as _: option(StaleTime.t)=?,
        ~gcTime as _: option(float)=?,
        ~retry as _: option(Retry.t)=?,
        ~retryDelay as _: option((int, exn) => float)=?,
        ~networkMode as _: option(NetworkMode.t)=?,
        ~refetchOnWindowFocus as _: option(bool)=?,
        ~refetchOnReconnect as _: option(bool)=?,
        ~onQueryError as _: option((exn, CachedQuery.t) => unit)=?,
        ~onMutationError as _: option((exn, CachedMutation.t) => unit)=?,
        (),
      ) => {
    queries: Hashtbl.create(8),
    infinite: Hashtbl.create(8),
    updatedAt: Hashtbl.create(8),
    updateCount: Hashtbl.create(8),
    fetching: Hashtbl.create(8),
    announced: Hashtbl.create(8),
    settled: Hashtbl.create(8),
    settledOne: Lwt_condition.create(),
    queued: [],
    send: None,
  };

  let fetch = (client, query, key) => {
    let (queryHash, promise) = start(client, query, key);
    announce(client, ~queryHash, promise);
    promise;
  };

  let prefetch = (client, query, key) =>
    ignoreFailure(fetch(client, query, key));

  let ensure = fetch;

  let fetchInfinite = (client, ~pages=?, query, key) => {
    let (queryHash, promise) = startInfinite(client, ~pages?, query, key);
    announce(client, ~queryHash, promise);
    promise;
  };

  let prefetchInfinite = (client, ~pages=?, query, key) =>
    ignoreFailure(fetchInfinite(client, ~pages?, query, key));

  let invalidate = (_, ~refetch as _: option(Refetch.t)=?, _) =>
    Lwt.return();
  let refetch = (_, _) => Lwt.return();
  let cancel = (_, _) => Lwt.return();
  let remove = (_, _) => ();
  let reset = (_, _) => Lwt.return();
  let clear = _ => ();
  let isFetching = (_, _) => 0;

  let findQuery = (client, query: Query.t(_, _), queryHash) =>
    find(client, ~id=query.id, ~name=query.name, queryHash);

  let getQueryData = (client, query, key) =>
    Option.bind(
      findQuery(client, query, Query.hash(query, key)),
      promiseData,
    );

  /* The hashes of the query's keys in the cache, with their promises. */
  let entries = (client, query) =>
    Hashtbl.to_seq_keys(client.queries)
    |> List.of_seq
    |> List.filter_map(queryHash =>
         switch (findQuery(client, query, queryHash)) {
         | Some(promise) => Some((queryHash, promise))
         | None => None
         | exception (Invalid_argument(_)) => None
         }
       );

  let getQueriesData = (client, query) =>
    entries(client, query)
    |> List.filter_map(((_, promise)) => promiseData(promise))
    |> Array.of_list;

  let getQueryState = (client, query, key) =>
    findQuery(client, query, Query.hash(query, key))
    |> Option.map(promise =>
         {
           data: promiseData(promise),
           error:
             switch (Lwt.state(promise)) {
             | Lwt.Fail(error) => Some(error)
             | Lwt.Return(_)
             | Lwt.Sleep => None
             },
           dataUpdatedAt: 0.,
           errorUpdatedAt: 0.,
           fetchStatus: Lwt.is_sleeping(promise) ? Fetching : Idle,
           isInvalidated: false,
         }
       );

  let setQueryData = (client, query: Query.t(_, _), key, update) =>
    write(
      client,
      ~id=query.id,
      ~encode=query.codec.encode,
      ~infinite=false,
      ~queryHash=Query.hash(query, key),
      update,
      getQueryData(client, query, key),
    );

  let setQueriesData = (client, query: Query.t(_, _), update) =>
    entries(client, query)
    |> List.iter(((queryHash, promise)) =>
         write(
           client,
           ~id=query.id,
           ~encode=query.codec.encode,
           ~infinite=false,
           ~queryHash,
           update,
           promiseData(promise),
         )
       );

  let getInfiniteQueryData = (client, query: InfiniteQuery.t(_, _, _), key) =>
    Option.bind(
      find(
        client,
        ~id=query.id,
        ~name=query.name,
        InfiniteQuery.hash(query, key),
      ),
      promiseData,
    );

  let setInfiniteQueryData =
      (client, query: InfiniteQuery.t(_, _, _), key, update) =>
    write(
      client,
      ~id=query.id,
      ~encode=query.encode,
      ~infinite=true,
      ~queryHash=InfiniteQuery.hash(query, key),
      update,
      getInfiniteQueryData(client, query, key),
    );

  let context: React.Context.t(option(t)) = React.createContext(None);

  module Provider = {
    let provider = React.Context.provider(context);

    [@react.component]
    let make = (~client: t, ~children: React.element) =>
      provider(React.Context.makeProps(~value=Some(client), ~children, ()));
  };
};

let useQueryClient = () =>
  switch (React.useContext(QueryClient.context)) {
  | Some(client) => client
  | None => failwith("ReactQuery: no QueryClient.Provider above this hook")
  };

let useIsFetching = (~filter as _: option(Filter.t)=?, ()) => 0;

module DehydratedState = {
  type t = list(Melange_json.t);
  let toJson = queries =>
    `Assoc([("queries", `List(queries)), ("mutations", `List([]))]);
  let fromJson =
    fun
    | `Assoc(fields) =>
      switch (List.assoc_opt("queries", fields)) {
      | Some(`List(queries)) => queries
      | _ =>
        Melange_json.of_json_msg_error("a dehydrated state needs queries")
      }
    | _ => Melange_json.of_json_msg_error("a dehydrated state is an object");
};

let dehydrate = client =>
  Hashtbl.to_seq(client.queries)
  |> List.of_seq
  |> List.filter_map(((queryHash, Entry(_, promise, encode))) =>
       switch (Option.map(encode, promiseData(promise))) {
       | Some(data) =>
         Some(
           dehydrated(
             client,
             ~queryHash,
             ~status="success",
             ~fetching=Hashtbl.mem(client.fetching, queryHash),
             ~data,
             (),
           ),
         )
       | None => None
       | exception _ => None
       }
     );

/* TanStack's [hydrate] keeps a query whose data is as new or newer. */
let updatedAt = (client, queryHash) =>
  switch (Hashtbl.find_opt(client.queries, queryHash)) {
  | Some(Entry(_, promise, _)) when Option.is_some(promiseData(promise)) =>
    Option.value(Hashtbl.find_opt(client.updatedAt, queryHash), ~default=0.)
  | _ => 0.
  };

let field = (name, json: Melange_json.t) =>
  switch (json) {
  | `Assoc(fields) => List.assoc_opt(name, fields)
  | _ => None
  };

let hydrateQuery = (client, message) => {
  let stateField = name =>
    Option.bind(field("state", message), field(name));
  switch (
    field("queryHash", message),
    field("queryKey", message),
    stateField("status"),
    stateField("data"),
    stateField("dataUpdatedAt"),
  ) {
  | (
      Some(`String(queryHash)),
      Some(`List([`String(name), ..._])),
      Some(`String("success")),
      Some(json),
      Some((`Float(_) | `Int(_)) as at),
    ) =>
    let at = Yojson.Basic.Util.to_number(at);
    switch (Hashtbl.find_opt(decoders, name)) {
    | Some((infinite, decode)) when at > updatedAt(client, queryHash) =>
      let count =
        switch (stateField("dataUpdateCount")) {
        | Some(`Int(count)) => Some(count)
        | _ => None
        };
      switch (decode(json)) {
      | Entry(id, promise, encode) =>
        Option.iter(
          replace(
            client,
            ~id,
            ~encode,
            ~infinite,
            ~queryHash,
            ~updatedAt=at,
            ~count?,
          ),
          promiseData(promise),
        )
      | exception _ => ()
      };
    | _ => ()
    };
  | _ => ()
  };
};

let hydrate = (client, state) => List.iter(hydrateQuery(client), state);

/* TanStack's [HydrationBoundary] hydrates new queries in render and newer
   data for cached ones in an effect, which never runs on the server. */
module HydrationBoundary = {
  [@react.component]
  let make = (~state: DehydratedState.t, ~children: React.element) => {
    let client = useQueryClient();
    state
    |> List.filter(message =>
         switch (field("queryHash", message)) {
         | Some(`String(queryHash)) =>
           !Hashtbl.mem(client.queries, queryHash)
         | _ => false
         }
       )
    |> hydrate(client);
    children;
  };
};

module QueryResult = {
  type t('data) = status('data);
  let status = result => result;
  let data =
    fun
    | Success(data) => Some(data)
    | Pending
    | Error(_) => None;
  let error =
    fun
    | Error(error) => Some(error)
    | Pending
    | Success(_) => None;
  let fetchStatus = _ => Idle;
  let isFetching = _ => false;
  let isStale = _ => false;
  let dataUpdatedAt = _ => 0.;
  let refetch = _ => Lwt.return();
};

module SuspenseResult = {
  type t('data) = 'data;
  let data = data => data;
  let refetchError = _ => None;
  let fetchStatus = _ => Idle;
  let isFetching = _ => false;
  let isStale = _ => false;
  let dataUpdatedAt = _ => 0.;
  let refetch = _ => Lwt.return();
};

/* A failed query renders [Pending], like the browser, which fetches it
   again instead of hydrating the server's error. */
let settled = (client, query: Query.t(_, _), key) =>
  switch (QueryClient.getQueryData(client, query, key)) {
  | Some(data) => Success(data)
  | None => Pending
  };

let useQuery =
    (
      ~enabled=true,
      ~staleTime as _: option(StaleTime.t)=?,
      ~refetchInterval as _: option(float)=?,
      ~refetchOnWindowFocus as _: option(bool)=?,
      query,
      key,
    ) => {
  let client = useQueryClient();
  enabled ? settled(client, query, key) : Pending;
};

let useQueryOpt =
    (
      ~staleTime as _: option(StaleTime.t)=?,
      ~refetchInterval as _: option(float)=?,
      ~refetchOnWindowFocus as _: option(bool)=?,
      query,
      key,
    ) => {
  let client = useQueryClient();
  switch (key) {
  | Some(key) => settled(client, query, key)
  | None => Pending
  };
};

let useQuerySelect =
    (
      ~select,
      ~enabled=?,
      ~staleTime=?,
      ~refetchInterval=?,
      ~refetchOnWindowFocus=?,
      query,
      key,
    ) =>
  switch (
    useQuery(
      ~enabled?,
      ~staleTime?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      query,
      key,
    )
  ) {
  | Success(data) => Success(select(data))
  | Pending => Pending
  | Error(error) => Error(error)
  };

let readSuspense = (client, query: Query.t(_, _), key) => {
  let (queryHash, promise) = start(client, query, key);
  let data = React.Experimental.usePromise(promise);
  /* Data that replaced a running fetch is not its result. */
  if (!Hashtbl.mem(client.fetching, queryHash)) {
    succeed(client, ~encode=query.codec.encode, ~queryHash, data);
  };
  data;
};

let useSuspenseQuery =
    (
      ~staleTime as _: option(StaleTime.t)=?,
      ~refetchInterval as _: option(float)=?,
      ~refetchOnWindowFocus as _: option(bool)=?,
      query,
      key,
    ) => {
  readSuspense(useQueryClient(), query, key);
};

module Queries = {
  type t('results) = client => 'results;
  let one = (query, key, client) => settled(client, query, key);
  let both = (a, b, client) => (a(client), b(client));
  let all = (items, client) => Array.map(item => item(client), items);
};

let useQueries = queries => queries(useQueryClient());

/* Every query starts before the first one suspends. */
module SuspenseQueries = {
  type t('results) = {
    start: client => unit,
    read: client => 'results,
  };
  let one = (query, key) => {
    start: client => ignore(start(client, query, key)),
    read: client => readSuspense(client, query, key),
  };
  let both = (a, b) => {
    start: client => {
      a.start(client);
      b.start(client);
    },
    read: client => (a.read(client), b.read(client)),
  };
  let all = items => {
    start: client => Array.iter(item => item.start(client), items),
    read: client => Array.map(item => item.read(client), items),
  };
};

let useSuspenseQueries = (queries: SuspenseQueries.t(_)) => {
  let client = useQueryClient();
  queries.start(client);
  queries.read(client);
};

let usePrefetchQuery = (query, key) =>
  ignore(QueryClient.prefetch(useQueryClient(), query, key));

let usePrefetchInfiniteQuery = (~pages=?, query, key) =>
  ignore(
    QueryClient.prefetchInfinite(useQueryClient(), ~pages?, query, key),
  );

module InfiniteResult = {
  type t('page, 'param) = {
    status: status(infiniteData('page, 'param)),
    hasNext: bool,
    hasPrevious: bool,
  };
  let status = result => result.status;
  let data = result => QueryResult.data(result.status);
  let error = result => QueryResult.error(result.status);
  let fetchStatus = _ => Idle;
  let isFetching = _ => false;
  let isStale = _ => false;
  let dataUpdatedAt = _ => 0.;
  let refetch = _ => Lwt.return();
  let hasNextPage = result => result.hasNext;
  let hasPreviousPage = result => result.hasPrevious;
  let isFetchingNextPage = _ => false;
  let isFetchingPreviousPage = _ => false;
  let fetchNextPage = _ => Lwt.return();
  let fetchPreviousPage = _ => Lwt.return();
};

module SuspenseInfiniteResult = {
  type t('page, 'param) = {
    data: infiniteData('page, 'param),
    hasNext: bool,
    hasPrevious: bool,
  };
  let data = result => result.data;
  let refetchError = _ => None;
  let fetchStatus = _ => Idle;
  let isFetching = _ => false;
  let isStale = _ => false;
  let dataUpdatedAt = _ => 0.;
  let refetch = _ => Lwt.return();
  let hasNextPage = result => result.hasNext;
  let hasPreviousPage = result => result.hasPrevious;
  let isFetchingNextPage = _ => false;
  let isFetchingPreviousPage = _ => false;
  let fetchNextPage = _ => Lwt.return();
  let fetchPreviousPage = _ => Lwt.return();
};

let useInfiniteQuery =
    (
      ~enabled=true,
      ~staleTime as _: option(StaleTime.t)=?,
      ~refetchInterval as _: option(float)=?,
      ~refetchOnWindowFocus as _: option(bool)=?,
      query,
      key,
    ) => {
  let client = useQueryClient();
  let data =
    enabled ? QueryClient.getInfiniteQueryData(client, query, key) : None;
  {
    InfiniteResult.status:
      switch (data) {
      | Some(data) => Success(data)
      | None => Pending
      },
    hasNext:
      Option.fold(~none=false, ~some=InfiniteQuery.hasNextPage(query), data),
    hasPrevious:
      Option.fold(
        ~none=false,
        ~some=InfiniteQuery.hasPreviousPage(query),
        data,
      ),
  };
};

let useSuspenseInfiniteQuery =
    (
      ~staleTime as _: option(StaleTime.t)=?,
      ~refetchInterval as _: option(float)=?,
      ~refetchOnWindowFocus as _: option(bool)=?,
      query: InfiniteQuery.t(_, _, _),
      key,
    ) => {
  let client = useQueryClient();
  let (queryHash, promise) = startInfinite(client, query, key);
  let data = React.Experimental.usePromise(promise);
  if (!Hashtbl.mem(client.fetching, queryHash)) {
    succeed(client, ~encode=query.encode, ~queryHash, data);
  };
  {
    SuspenseInfiniteResult.data,
    hasNext: InfiniteQuery.hasNextPage(query, data),
    hasPrevious: InfiniteQuery.hasPreviousPage(query, data),
  };
};

module Mutation = {
  type t('variables, 'data) =
    | Mutation;

  let make =
      (
        ~name as _: string,
        ~fn as _: 'variables => Js.Promise.t('data),
        ~onMutate as
          _: option((QueryClient.t, 'variables) => Js.Promise.t('context))=?,
        ~onSuccess as
          _:
            option(
              (QueryClient.t, 'data, 'variables, option('context)) =>
              Js.Promise.t(unit),
            )=?,
        ~onError as
          _:
            option(
              (QueryClient.t, exn, 'variables, option('context)) =>
              Js.Promise.t(unit),
            )=?,
        ~onSettled as
          _:
            option(
              (
                QueryClient.t,
                result('data, exn),
                'variables,
                option('context)
              ) =>
              Js.Promise.t(unit),
            )=?,
        ~retry as _: option(Retry.t)=?,
        ~retryDelay as _: option((int, exn) => float)=?,
        ~scope as _: option(string)=?,
        (),
      )
      : t('variables, 'data) =>
    Mutation;
};

type mutationStatus('variables, 'data) =
  | Idle
  | Running('variables)
  | Failed(exn, 'variables)
  | Succeeded('data, 'variables);

module MutationResult = {
  type t('variables, 'data) = Mutation.t('variables, 'data);
  let status = _ => Idle;
  let isPending = _ => false;
  let mutate =
      (
        ~onSuccess as _: option(('data, 'variables) => unit)=?,
        ~onError as _: option((exn, 'variables) => unit)=?,
        ~onSettled as _: option((result('data, exn), 'variables) => unit)=?,
        _: t('variables, 'data),
        _: 'variables,
      ) =>
    ();
  let mutateAsync = (_, _) =>
    Lwt.fail(Failure("ReactQuery: mutations do not run on the server"));
  let reset = _ => ();
};

let useMutation = mutation => mutation;

let useMutationState = _ => [||];

let useIsMutating = (~mutation as _: option(Mutation.t(_, _))=?, ()) => 0;

module Persister = {
  type persistedClient = {
    timestamp: float,
    buster: string,
    clientState: DehydratedState.t,
  };

  type t = {
    persist: persistedClient => Js.Promise.t(unit),
    restore: unit => Js.Promise.t(option(persistedClient)),
    remove: unit => Js.Promise.t(unit),
  };

  module Storage = {
    type storage = {
      getItem: string => option(string),
      setItem: (string, string) => unit,
      removeItem: string => unit,
    };
    type t = option(storage);
    let local = None;
    let session = None;
    let make = (~getItem, ~setItem, ~removeItem) =>
      Some({
        getItem,
        setItem,
        removeItem,
      });
  };

  let make = (~persist, ~restore, ~remove) => {
    persist,
    restore,
    remove,
  };

  let toJson = ({ timestamp, buster, clientState }) =>
    `Assoc([
      ("buster", `String(buster)),
      ("timestamp", `Float(timestamp)),
      ("clientState", DehydratedState.toJson(clientState)),
    ]);

  let fromJson = json =>
    switch (field("timestamp", json), field("buster", json)) {
    | (Some((`Float(_) | `Int(_)) as timestamp), Some(`String(buster))) => {
        timestamp: Yojson.Basic.Util.to_number(timestamp),
        buster,
        clientState:
          DehydratedState.fromJson(
            Option.value(field("clientState", json), ~default=`Null),
          ),
      }
    | _ => Melange_json.of_json_msg_error("a persisted client")
    };

  /* TanStack's [createSyncStoragePersister]: a save waits [throttleTime]
     and writes the latest client then. Without a storage it does nothing. */
  let syncStorage =
      (
        ~storage=Storage.local,
        ~key="REACT_QUERY_OFFLINE_CACHE",
        ~throttleTime=1000.,
        (),
      ) =>
    switch (storage) {
    | None => {
        persist: _ => Lwt.return(),
        restore: () => Lwt.return(None),
        remove: () => Lwt.return(),
      }
    | Some({ Storage.getItem, setItem, removeItem }) =>
      let latest = ref(None);
      {
        persist: client => {
          if (Option.is_none(latest^)) {
            Lwt.async(() => {
              let%lwt () = Lwt_unix.sleep(throttleTime /. 1000.);
              Option.iter(
                client =>
                  try(setItem(key, Yojson.Basic.to_string(toJson(client)))) {
                  | _ => ()
                  },
                latest^,
              );
              latest := None;
              Lwt.return();
            });
          };
          latest := Some(client);
          Lwt.return();
        },
        restore: () =>
          switch (getItem(key)) {
          | None
          | Some("") => Lwt.return(None)
          | Some(json) =>
            switch (fromJson(Yojson.Basic.from_string(json))) {
            | client => Lwt.return(Some(client))
            | exception error => Lwt.fail(error)
            }
          },
        remove: () => {
          removeItem(key);
          Lwt.return();
        },
      };
    };
};

/* TanStack's [persistQueryClientRestore]. */
let persistQueryClientRestore =
    (
      ~client,
      ~persister: Persister.t,
      ~maxAge=24. *. 60. *. 60. *. 1000.,
      ~buster="",
      (),
    ) =>
  Lwt.catch(
    () =>
      switch%lwt (persister.restore()) {
      | None => Lwt.return()
      | Some({ timestamp, _ }) when timestamp == 0. => persister.remove()
      | Some(persisted) =>
        if (now() -. persisted.timestamp > maxAge || persisted.buster != buster) {
          persister.remove();
        } else {
          hydrate(client, persisted.clientState);
          Lwt.return();
        }
      },
    error => {
      let%lwt () = persister.remove();
      Lwt.fail(error);
    },
  );

let persistQueryClientSave =
    (~client, ~persister: Persister.t, ~buster="", ()) =>
  persister.persist({
    timestamp: now(),
    buster,
    clientState: dehydrate(client),
  });

let persistQueryClientSubscribe =
    (~client as _, ~persister as _: Persister.t, ~buster as _=?, (), ()) =>
  ();

let persistQueryClient = (~client, ~persister, ~maxAge=?, ~buster=?, ()) => (
  () => (),
  persistQueryClientRestore(~client, ~persister, ~maxAge?, ~buster?, ()),
);

let isRestoring = React.createContext(false);

let useIsRestoring = () => React.useContext(isRestoring);

module IsRestoringProvider = {
  let provider = React.Context.provider(isRestoring);

  [@react.component]
  let make = (~value, ~children) =>
    provider(React.Context.makeProps(~value, ~children, ()));
};

/* The restore runs in an effect, so the server renders it as restoring. */
module PersistQueryClientProvider = {
  [@react.component]
  let make =
      (
        ~client,
        ~persister as _: Persister.t,
        ~maxAge as _: option(float)=?,
        ~buster as _: option(string)=?,
        ~onSuccess as _: option(unit => unit)=?,
        ~onError as _: option(unit => unit)=?,
        ~children,
      ) =>
    <QueryClient.Provider client>
      <IsRestoringProvider value=true> children </IsRestoringProvider>
    </QueryClient.Provider>;
};

/* A failed query renders its Suspense fallback on the server, so there is
   nothing to reset. */
module QueryErrorResetBoundary = {
  type t = unit;
  let reset = () => ();
  let isReset = () => false;
  let clearReset = () => ();

  [@react.component]
  let make = (~children: React.element) => children;
};

let useQueryErrorResetBoundary = () => ();

module ErrorBoundary = {
  [@react.component]
  let make =
      (
        ~fallback as _: (~error: exn, ~retry: unit => unit) => React.element,
        ~children: React.element,
      ) => children;
};

let insertBeforeHeadEnd = (html, scripts) => {
  let closing = "</head>";
  let rec find = index =>
    if (index + String.length(closing) > String.length(html)) {
      None;
    } else if (String.sub(html, index, String.length(closing)) == closing) {
      Some(index);
    } else {
      find(index + 1);
    };
  switch (find(0)) {
  | Some(index) =>
    String.sub(html, 0, index)
    ++ scripts
    ++ String.sub(html, index, String.length(html) - index)
  | None => html ++ scripts
  };
};

module Server = {
  /* The shell's queries go in its <head>, so they are in the cache before a
     deferred or async script can start hydrating. */
  let renderToStream = (~env=?, ~timeout=60_000., ~client, element) => {
    let%lwt (chunks, abort) = ReactDOM.renderToStream(~env?, element);
    let (stream, push) = Lwt_stream.create();
    let send = html =>
      if (html != "") {
        push(Some(html));
      };
    let (aborted, stop) = Lwt.wait();
    let abort = () => {
      if (Lwt.is_sleeping(aborted)) {
        Lwt.wakeup(stop, ());
      };
      abort();
    };
    let rec announcedSettle = () =>
      if (Hashtbl.length(client.announced) == 0) {
        Lwt.return();
      } else {
        Lwt.bind(Lwt_condition.wait(client.settledOne), announcedSettle);
      };
    let timedOut = () => {
      let%lwt () = Lwt_unix.sleep(timeout /. 1000.);
      prerr_endline(
        "ReactQuery: queries still pending "
        ++ Printf.sprintf("%.0f", timeout)
        ++ "ms after rendering finished: "
        ++ String.concat(
             ", ",
             List.of_seq(Hashtbl.to_seq_keys(client.announced)),
           ),
      );
      Lwt.return();
    };
    Lwt.async(() =>
      Lwt.finalize(
        () => {
          let%lwt () =
            Lwt_stream.iter(
              chunk =>
                switch (client.send) {
                | Some(_) => send(chunk)
                | None =>
                  send(insertBeforeHeadEnd(chunk, drain(client)));
                  client.send = Some(send);
                },
              chunks,
            );
          Lwt.pick([announcedSettle(), Lwt.protected(aborted), timedOut()]);
        },
        () => {
          client.send = None;
          push(None);
          Lwt.return();
        },
      )
    );
    Lwt.return((stream, abort));
  };
};

/* TanStack's [Subscribable] with the event listener of its managers: the
   setup runs when the first listener subscribes, unless an earlier one left
   a cleanup, and the cleanup runs when the last one leaves. */
module Manager = {
  type t('report) = {
    mutable listeners: list((int, unit => unit)),
    mutable next: int,
    mutable setup: 'report => option(unit => unit),
    mutable cleanup: option(unit => unit),
    report: 'report,
  };

  let make = report => {
    listeners: [],
    next: 0,
    setup: _ => None,
    cleanup: None,
    report,
  };

  let notify = manager =>
    List.iter(((_, listener)) => listener(), manager.listeners);

  let setEventListener = (manager, setup) => {
    manager.setup = setup;
    Option.iter(cleanup => cleanup(), manager.cleanup);
    manager.cleanup = setup(manager.report);
  };

  let subscribe = (manager, listener) => {
    let id = manager.next;
    manager.next = id + 1;
    manager.listeners = manager.listeners @ [(id, listener)];
    if (Option.is_none(manager.cleanup)) {
      setEventListener(manager, manager.setup);
    };
    () => {
      manager.listeners =
        List.filter(((other, _)) => other != id, manager.listeners);
      if (manager.listeners == []) {
        Option.iter(cleanup => cleanup(), manager.cleanup);
        manager.cleanup = None;
      };
    };
  };
};

module FocusManager = {
  let focused = ref(None);
  let isFocused = () => Option.value(focused^, ~default=true);
  let rec manager = lazy(Manager.make(report))
  and report =
    fun
    | Some(_) as next => setFocused(next)
    | None => Manager.notify(Lazy.force(manager))
  and setFocused = next =>
    if (next != focused^) {
      focused := next;
      Manager.notify(Lazy.force(manager));
    };
  let subscribe = listener =>
    Manager.subscribe(Lazy.force(manager), () => listener(isFocused()));
  let setEventListener = setup =>
    Manager.setEventListener(Lazy.force(manager), setup);
};

module OnlineManager = {
  let online = ref(true);
  let isOnline = () => online^;
  let rec manager = lazy(Manager.make(setOnline))
  and setOnline = next =>
    if (next != online^) {
      online := next;
      Manager.notify(Lazy.force(manager));
    };
  let subscribe = listener =>
    Manager.subscribe(Lazy.force(manager), () => listener(isOnline()));
  let setEventListener = setup =>
    Manager.setEventListener(Lazy.force(manager), setup);
};

module NotifyManager = {
  let defaultScheduler = callback =>
    Lwt.async(() => Lwt.map(callback, Lwt.pause()));
  let queue = ref([]);
  let transactions = ref(0);
  let notifyFunction = ref(callback => callback());
  let batchNotifyFunction = ref(callback => callback());
  let scheduler = ref(defaultScheduler);

  let schedule = callback =>
    if (transactions^ > 0) {
      queue := [callback, ...queue^];
    } else {
      scheduler^(() => notifyFunction^(callback));
    };

  let flush = () => {
    let callbacks = List.rev(queue^);
    queue := [];
    switch (callbacks) {
    | [] => ()
    | callbacks =>
      scheduler^(() =>
        batchNotifyFunction^(() =>
          List.iter(callback => notifyFunction^(callback), callbacks)
        )
      )
    };
  };

  let batch = callback => {
    incr(transactions);
    Fun.protect(
      callback,
      ~finally=() => {
        decr(transactions);
        if (transactions^ == 0) {
          flush();
        };
      },
    );
  };

  let batchCalls = (callback, value) => schedule(() => callback(value));
  let setNotifyFunction = fn => notifyFunction := fn;
  let setBatchNotifyFunction = fn => batchNotifyFunction := fn;
  let setScheduler = fn => scheduler := fn;
};
