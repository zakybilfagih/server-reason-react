module Raw = ReactQuery_raw;

let toExn = (thrown: Raw.thrown) => Js.Exn.anyToExnInternal(thrown);

module Key = {
  type t = Js.Json.t;
  let string = Js.Json.string;
  let int = value => Js.Json.number(Float.of_int(value));
  let float = Js.Json.number;
  let bool = Js.Json.boolean;
  let null = Js.Json.null;
  let list = values => Js.Json.array(Array.of_list(values));
  let obj = fields => Js.Json.object_(Js.Dict.fromList(fields));
  let hash = parts => Raw.hashKey(list(parts));
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

  let toRaw =
    fun
    | Ms(ms) => Js.Json.number(ms)
    | Infinite => Js.Json.number(infinity)
    | Static => Js.Json.string("static");
};

module Retry = {
  type t =
    | Never
    | Times(int)
    | When((int, exn) => bool);

  let toRaw =
    fun
    | Never => ((. _, _) => false)
    | Times(times) => ((. failures, _) => failures < times)
    | When(retry) => (
        (. failures, thrown) => retry(failures, toExn(thrown))
      );
};

let retryDelayToRaw = retryDelay =>
  (. failures, thrown) => retryDelay(failures, toExn(thrown));

module NetworkMode = {
  type t =
    | Online
    | Always
    | OfflineFirst;

  let toRaw =
    fun
    | Online => "online"
    | Always => "always"
    | OfflineFirst => "offlineFirst";
};

/* The server's data for a query: already settled, or a promise of it. */
type fromServer =
  | Data(Js.Json.t)
  | Later(Js.Promise.t(Js.Json.t));

/* What each query name needs to cross between clients as JSON: its
   message as TanStack's dehydrated query with decoded data, which raises
   when the data does not decode, and TanStack's [dehydrateQuery] with
   encoded data. */
type registered = {
  toDehydrated: (Js.Json.t, fromServer) => Raw.dehydratedQuery,
  dehydrate: Raw.query => Js.Json.t,
};

let registry: Hashtbl.t(string, registered) = Hashtbl.create(8);

let register = (name, codec: Codec.t(_)) =>
  Hashtbl.replace(
    registry,
    name,
    {
      toDehydrated: (message, server) =>
        switch (server) {
        | Data(json) =>
          Raw.dehydratedQuery(
            message,
            Js.Undefined.return(codec.decode(json)),
            Js.undefined,
          )
        | Later(promise) =>
          let promise =
            promise
            |> Js.Promise.then_(json =>
                 Js.Promise.resolve(codec.decode(json))
               );
          /* [hydrate] does not use the promise when the query is already
             fetching. */
          promise
          |> Js.Promise.then_(_ => Js.Promise.resolve())
          |> Js.Promise.catch(_ => Js.Promise.resolve())
          |> ignore;
          Raw.dehydratedQuery(
            message,
            Js.undefined,
            Js.Undefined.return(promise),
          );
        },
      dehydrate: query =>
        Raw.dehydrateQuery(query, (. data) => codec.encode(data)),
    },
  );

let field = (json, name) =>
  Option.bind(Js.Json.decodeObject(json), fields =>
    Js.Dict.get(fields, name)
  );

let messageName = message =>
  switch (Option.bind(field(message, "queryKey"), Js.Json.decodeArray)) {
  | Some(queryKey) when Array.length(queryKey) > 0 =>
    Js.Json.decodeString(queryKey[0])
  | _ => None
  };

/* A message with data that does not decode is logged and left out, so the
   browser fetches the query itself. */
let toDehydrated = (message, server) =>
  switch (Option.bind(messageName(message), Hashtbl.find_opt(registry))) {
  | Some(registered) =>
    switch (registered.toDehydrated(message, server)) {
    | dehydrated => Some(dehydrated)
    | exception error =>
      Js.Console.error3(
        "ReactQuery: could not decode a query from the server, fetching it instead:",
        message,
        error,
      );
      None;
    }
  | None =>
    Js.Console.error2("ReactQuery: no query is named in", message);
    None;
  };

module Query = {
  type t('key, 'data) = {
    name: string,
    key: 'key => list(Key.t),
    queryFn: ('key, Raw.queryFunctionContext) => Js.Promise.t('data),
    staleTime: option(StaleTime.t),
    gcTime: option(float),
    retry: option(Retry.t),
    retryDelay: option((int, exn) => float),
    networkMode: option(NetworkMode.t),
  };

  let queryKey = (query, key) =>
    Key.list([Key.string(query.name), ...query.key(key)]);

  let hash = (query, key) =>
    Key.hash([Key.string(query.name), ...query.key(key)]);

  let custom =
      (
        ~name,
        ~key,
        ~codec,
        ~queryFn,
        ~staleTime=?,
        ~gcTime=?,
        ~retry=?,
        ~retryDelay=?,
        ~networkMode=?,
        (),
      ) => {
    register(name, codec);
    {
      name,
      key,
      queryFn,
      staleTime,
      gcTime,
      retry,
      retryDelay,
      networkMode,
    };
  };

  let make =
      (
        ~name,
        ~key,
        ~codec,
        ~fetch,
        ~staleTime=?,
        ~gcTime=?,
        ~retry=?,
        ~retryDelay=?,
        ~networkMode=?,
        (),
      ) =>
    custom(
      ~name,
      ~key,
      ~codec,
      ~queryFn=(key, context) => fetch(~signal=Raw.signal(context), key),
      ~staleTime?,
      ~gcTime?,
      ~retry?,
      ~retryDelay?,
      ~networkMode?,
      (),
    );

  let queryFn = (query, key, context) => query.queryFn(key, context);

  let options =
      (
        ~enabled=?,
        ~staleTime=?,
        ~refetchInterval=?,
        ~refetchOnWindowFocus=?,
        ~select=?,
        query,
        key,
      ) => {
    let queryKey = queryKey(query, key);
    Raw.queryOptions(
      ~queryKey,
      ~queryFn=queryFn(query, key),
      ~staleTime=?
        Option.map(
          StaleTime.toRaw,
          Option.fold(~none=query.staleTime, ~some=Option.some, staleTime),
        ),
      ~gcTime=?query.gcTime,
      ~retry=?Option.map(Retry.toRaw, query.retry),
      ~retryDelay=?Option.map(retryDelayToRaw, query.retryDelay),
      ~networkMode=?Option.map(NetworkMode.toRaw, query.networkMode),
      ~enabled?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      ~select?,
      (),
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

let fetchStatusOfRaw =
  fun
  | "fetching" => Fetching
  | "paused" => Paused
  | _ => Idle;

let fetchStatus = result => fetchStatusOfRaw(Raw.fetchStatus(result));

module Stream = {
  type t('chunk) = unit => Js.Promise.t(option('chunk));
  let make = next => next;
  let fromArray = chunks => {
    let index = ref(0);
    () =>
      if (index^ < Array.length(chunks)) {
        incr(index);
        Js.Promise.resolve(Some(chunks[index^ - 1]));
      } else {
        Js.Promise.resolve(None);
      };
  };
};

module StreamedQuery = {
  type refetchMode =
    | Reset
    | Append
    | Replace;

  let refetchModeToRaw =
    fun
    | Reset => "reset"
    | Append => "append"
    | Replace => "replace";

  let make =
      (
        ~name,
        ~key,
        ~codec,
        ~stream,
        ~reducer,
        ~initialValue,
        ~refetchMode=Reset,
        ~staleTime=?,
        ~gcTime=?,
        ~retry=?,
        ~retryDelay=?,
        ~networkMode=?,
        (),
      ) =>
    Query.custom(
      ~name,
      ~key,
      ~codec,
      ~queryFn=
        (key, context) => {
          let queryFn =
            Raw.streamedQuery(
              Raw.streamedQueryParams(
                ~streamFn=
                  context =>
                    stream(~signal=Raw.signal(context), key)
                    |> Js.Promise.then_((next: Stream.t(_)) =>
                         Js.Promise.resolve(
                           Raw.asyncIterable(() =>
                             next()
                             |> Js.Promise.then_(chunk =>
                                  Js.Promise.resolve(
                                    Js.Undefined.fromOption(chunk),
                                  )
                                )
                           ),
                         )
                       ),
                ~reducer=(. data, chunk) => reducer(data, chunk),
                ~initialValue,
                ~refetchMode=refetchModeToRaw(refetchMode),
                (),
              ),
            );
          queryFn(. context);
        },
      ~staleTime?,
      ~gcTime?,
      ~retry?,
      ~retryDelay?,
      ~networkMode?,
      (),
    );
};

type infiniteData('page, 'param) = {
  pages: array('page),
  pageParams: array('param),
};

let decodeInfinite = (decodePage, decodeParam, json) => {
  let items = (name, decode) =>
    switch (Option.bind(field(json, name), Js.Json.decodeArray)) {
    | Some(items) => Array.map(decode, items)
    | None => failwith("ReactQuery: an infinite query needs " ++ name)
    };
  {
    pages: items("pages", decodePage),
    pageParams: items("pageParams", decodeParam),
  };
};

module InfiniteQuery = {
  type t('key, 'page, 'param) = {
    name: string,
    key: 'key => list(Key.t),
    initialPageParam: 'param,
    fetchPage: (~signal: Fetch.signal, 'key, 'param) => Js.Promise.t('page),
    getNextPageParam: infiniteData('page, 'param) => option('param),
    getPreviousPageParam:
      option(infiniteData('page, 'param) => option('param)),
    maxPages: option(int),
    staleTime: option(StaleTime.t),
    gcTime: option(float),
    retry: option(Retry.t),
    retryDelay: option((int, exn) => float),
    networkMode: option(NetworkMode.t),
  };

  let queryKey = (query, key) =>
    Key.list([Key.string(query.name), ...query.key(key)]);

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
        ~staleTime=?,
        ~gcTime=?,
        ~retry=?,
        ~retryDelay=?,
        ~networkMode=?,
        (),
      ) => {
    register(
      name,
      Codec.make(
        ~encode=
          ({ pages, pageParams }) =>
            Js.Json.object_(
              Js.Dict.fromList([
                (
                  "pages",
                  Js.Json.array(Array.map(pageCodec.Codec.encode, pages)),
                ),
                (
                  "pageParams",
                  Js.Json.array(
                    Array.map(paramCodec.Codec.encode, pageParams),
                  ),
                ),
              ]),
            ),
        ~decode=decodeInfinite(pageCodec.decode, paramCodec.decode),
      ),
    );
    {
      name,
      key,
      initialPageParam,
      fetchPage,
      getNextPageParam,
      getPreviousPageParam,
      maxPages,
      staleTime,
      gcTime,
      retry,
      retryDelay,
      networkMode,
    };
  };

  let queryFn = (query, key, context) =>
    query.fetchPage(
      ~signal=Raw.signal(context),
      key,
      Raw.pageParam(context),
    );

  let pageParamFn = (getPageParam, ()) =>
    (. _, pages, _, pageParams) =>
      Js.Undefined.fromOption(
        getPageParam({
          pages,
          pageParams,
        }),
      );

  let options =
      (
        ~enabled=?,
        ~staleTime=?,
        ~refetchInterval=?,
        ~refetchOnWindowFocus=?,
        ~pages=?,
        query,
        key,
      ) => {
    let queryKey = queryKey(query, key);
    Raw.infiniteQueryOptions(
      ~queryKey,
      ~queryFn=queryFn(query, key),
      ~initialPageParam=query.initialPageParam,
      ~getNextPageParam=pageParamFn(query.getNextPageParam, ()),
      ~getPreviousPageParam=?
        Option.map(
          getPageParam => pageParamFn(getPageParam, ()),
          query.getPreviousPageParam,
        ),
      ~maxPages=?query.maxPages,
      ~pages?,
      ~staleTime=?
        Option.map(
          StaleTime.toRaw,
          Option.fold(~none=query.staleTime, ~some=Option.some, staleTime),
        ),
      ~gcTime=?query.gcTime,
      ~retry=?Option.map(Retry.toRaw, query.retry),
      ~retryDelay=?Option.map(retryDelayToRaw, query.retryDelay),
      ~networkMode=?Option.map(NetworkMode.toRaw, query.networkMode),
      ~enabled?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      (),
    );
  };
};

module Filter = {
  type t = {
    key: option(Key.t),
    exact: bool,
    active: option(bool),
    stale: option(bool),
    fetchStatus: option(fetchStatus),
  };

  let all = {
    key: None,
    exact: false,
    active: None,
    stale: None,
    fetchStatus: None,
  };
  let exact = (query, key) => {
    ...all,
    key: Some(Query.queryKey(query, key)),
    exact: true,
  };
  let query = (query: Query.t(_, _)) => {
    ...all,
    key: Some(Key.list([Key.string(query.name)])),
  };
  let exactInfinite = (query, key) => {
    ...all,
    key: Some(InfiniteQuery.queryKey(query, key)),
    exact: true,
  };
  let infinite = (query: InfiniteQuery.t(_, _, _)) => {
    ...all,
    key: Some(Key.list([Key.string(query.name)])),
  };

  let narrow = (value, previous) =>
    switch (value) {
    | Some(_) => value
    | None => previous
    };

  let where = (~active=?, ~stale=?, ~fetchStatus=?, filter) => {
    ...filter,
    active: narrow(active, filter.active),
    stale: narrow(stale, filter.stale),
    fetchStatus: narrow(fetchStatus, filter.fetchStatus),
  };

  let toRaw = (~refetchType=?, filter) =>
    Raw.queryFilters(
      ~queryKey=?filter.key,
      ~exact=filter.exact,
      ~type_=?
        Option.map(active => active ? "active" : "inactive", filter.active),
      ~stale=?filter.stale,
      ~fetchStatus=?
        Option.map(
          fun
          | Fetching => "fetching"
          | Paused => "paused"
          | Idle => "idle",
          filter.fetchStatus,
        ),
      ~refetchType?,
      (),
    );
};

module Refetch = {
  type t =
    | Active
    | Inactive
    | All
    | Never;

  let toRaw =
    fun
    | Active => "active"
    | Inactive => "inactive"
    | All => "all"
    | Never => "none";
};

type queryState('data) = {
  data: option('data),
  error: option(exn),
  dataUpdatedAt: float,
  errorUpdatedAt: float,
  fetchStatus,
  isInvalidated: bool,
};

let queryState = state => {
  data: Js.Undefined.toOption(Raw.queryStateData(state)),
  error: Js.Null.toOption(Raw.queryStateError(state)) |> Option.map(toExn),
  dataUpdatedAt: Raw.stateDataUpdatedAt(state),
  errorUpdatedAt: Raw.queryStateErrorUpdatedAt(state),
  fetchStatus: fetchStatusOfRaw(Raw.queryStateFetchStatus(state)),
  isInvalidated: Raw.queryStateIsInvalidated(state),
};

let firstName = key =>
  switch (Option.map(Js.Json.decodeArray, key)) {
  | Some(Some(parts)) when Array.length(parts) > 0 =>
    Option.value(Js.Json.decodeString(parts[0]), ~default="")
  | _ => ""
  };

module CachedQuery = {
  type t = Raw.query;
  let name = query => firstName(Some(Raw.queryKeyOf(query)));
  let hash = Raw.queryHash;
  let hasData = query =>
    Option.is_some(
      Js.Undefined.toOption(Raw.queryStateData(Raw.stateOf(query))),
    );
};

module CachedMutation = {
  type t = Raw.mutation;
  let name = mutation =>
    firstName(
      Js.Undefined.toOption(
        Raw.mutationKeyOf(Raw.mutationOptionsOf(mutation)),
      ),
    );
};

module QueryClient = {
  type t = Raw.queryClient;

  let cachedQueries = client => Raw.getAllQueries(Raw.getQueryCache(client));

  let make =
      (
        ~staleTime=?,
        ~gcTime=?,
        ~retry=?,
        ~retryDelay=?,
        ~networkMode=?,
        ~refetchOnWindowFocus=?,
        ~refetchOnReconnect=?,
        ~onQueryError=?,
        ~onMutationError=?,
        (),
      ) =>
    Raw.makeQueryClient(
      Raw.clientConfig(
        ~defaultOptions={
          queries:
            Raw.defaultQueryOptions(
              ~staleTime=?Option.map(StaleTime.toRaw, staleTime),
              ~gcTime?,
              ~retry=?Option.map(Retry.toRaw, retry),
              ~retryDelay=?Option.map(retryDelayToRaw, retryDelay),
              ~networkMode=?Option.map(NetworkMode.toRaw, networkMode),
              ~refetchOnWindowFocus?,
              ~refetchOnReconnect?,
              (),
            ),
        },
        ~queryCache=?
          Option.map(
            onQueryError =>
              Raw.makeQueryCache(
                {
                  onError:
                    (. thrown, query) => onQueryError(toExn(thrown), query),
                }: Raw.queryCacheConfig,
              ),
            onQueryError,
          ),
        ~mutationCache=?
          Option.map(
            onMutationError =>
              Raw.makeMutationCache(
                {
                  onError:
                    (. thrown, _, _, mutation) =>
                      onMutationError(toExn(thrown), mutation),
                }: Raw.mutationCacheConfig,
              ),
            onMutationError,
          ),
        (),
      ),
    );

  let prefetch = (client, query, key) =>
    Raw.prefetchQuery(client, Query.options(query, key));

  let fetch = (client, query, key) =>
    Raw.fetchQuery(client, Query.options(query, key));

  let ensure = (client, query, key) =>
    Raw.ensureQueryData(client, Query.options(query, key));

  let prefetchInfinite = (client, ~pages=?, query, key) =>
    Raw.prefetchInfiniteQuery(
      client,
      InfiniteQuery.options(~pages?, query, key),
    );

  let fetchInfinite =
      (client, ~pages=?, query, key): Js.Promise.t(infiniteData(_, _)) =>
    Raw.fetchInfiniteQuery(
      client,
      InfiniteQuery.options(~pages?, query, key),
    );

  let invalidate = (client, ~refetch=?, filter) =>
    Raw.invalidateQueries(
      client,
      Filter.toRaw(~refetchType=?Option.map(Refetch.toRaw, refetch), filter),
    );

  let refetch = (client, filter) =>
    Raw.refetchQueries(client, Filter.toRaw(filter));

  let cancel = (client, filter) =>
    Raw.cancelQueries(client, Filter.toRaw(filter));

  let remove = (client, filter) =>
    Raw.removeQueries(client, Filter.toRaw(filter));

  let reset = (client, filter) =>
    Raw.resetQueries(client, Filter.toRaw(filter));

  let clear = Raw.clear;

  let isFetching = (client, filter) =>
    Raw.countFetching(client, Filter.toRaw(filter));

  let getQueryData = (client, query, key) =>
    Js.Undefined.toOption(
      Raw.getQueryData(client, Query.queryKey(query, key)),
    );

  let getQueriesData = (client, query) =>
    Raw.getQueriesData(client, Filter.toRaw(Filter.query(query)))
    |> Array.to_list
    |> List.filter_map(((_, data)) => Js.Undefined.toOption(data))
    |> Array.of_list;

  let getQueryState = (client, query, key) =>
    Js.Undefined.toOption(
      Raw.getQueryState(client, Query.queryKey(query, key)),
    )
    |> Option.map(queryState);

  let update = (update, data) =>
    Js.Undefined.fromOption(update(Js.Undefined.toOption(data)));

  let setQueryData = (client, query, key, change) =>
    Raw.updateQueryData(client, Query.queryKey(query, key), update(change));

  let setQueriesData = (client, query, change) =>
    Raw.setQueriesData(
      client,
      Filter.toRaw(Filter.query(query)),
      update(change),
    );

  let getInfiniteQueryData = (client, query, key) =>
    Js.Undefined.toOption(
      Raw.getQueryData(client, InfiniteQuery.queryKey(query, key)),
    );

  let setInfiniteQueryData = (client, query, key, change) =>
    Raw.updateQueryData(
      client,
      InfiniteQuery.queryKey(query, key),
      update(change),
    );

  module Provider = Raw.QueryClientProvider;
};

let useQueryClient = Raw.useQueryClient;

let useIsFetching = (~filter=Filter.all, ()) => {
  let client = useQueryClient();
  let subscribe =
    React.useCallback1(
      onChange => {
        let unsubscribe =
          Raw.subscribeQueryCache(
            Raw.getQueryCache(client),
            Raw.batchCalls(Raw.notifyManager, (.) => onChange()),
          );
        () => unsubscribe(.);
      },
      [|client|],
    );
  React.useSyncExternalStoreWithServer(
    ~subscribe,
    ~getSnapshot=() => Raw.countFetching(client, Filter.toRaw(filter)),
    ~getServerSnapshot=() => 0,
  );
};

let refetch = result =>
  Raw.refetch(result) |> Js.Promise.then_(_ => Js.Promise.resolve());

module QueryResult = {
  type t('data) = Raw.queryResult('data);
  let data = result => Js.Undefined.toOption(Raw.data(result));
  let error = result =>
    Js.Null.toOption(Raw.error(result)) |> Option.map(toExn);
  let status = result =>
    switch (Raw.status(result), error(result)) {
    | ("success", _) => Success(Raw.definedData(result))
    | ("error", Some(error)) => Error(error)
    | _ => Pending
    };
  let fetchStatus = fetchStatus;
  let isFetching = Raw.isFetching;
  let isStale = Raw.isStale;
  let dataUpdatedAt = Raw.dataUpdatedAt;
  let refetch = refetch;
};

module SuspenseResult = {
  type t('data) = Raw.queryResult('data);
  let data = Raw.definedData;
  let refetchError = QueryResult.error;
  let fetchStatus = fetchStatus;
  let isFetching = Raw.isFetching;
  let isStale = Raw.isStale;
  let dataUpdatedAt = Raw.dataUpdatedAt;
  let refetch = refetch;
};

let useQuery =
    (
      ~enabled=?,
      ~staleTime=?,
      ~refetchInterval=?,
      ~refetchOnWindowFocus=?,
      query,
      key,
    ) =>
  Raw.useQuery(
    Query.options(
      ~enabled?,
      ~staleTime?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      query,
      key,
    ),
  );

let useQueryOpt =
    (~staleTime=?, ~refetchInterval=?, ~refetchOnWindowFocus=?, query, key) =>
  Raw.useQuery(
    switch (key) {
    | Some(key) =>
      Query.options(
        ~staleTime?,
        ~refetchInterval?,
        ~refetchOnWindowFocus?,
        query,
        key,
      )
    | None =>
      Raw.skipQueryOptions(
        ~queryKey=Key.list([Key.string(query.name), Key.string("@@skip")]),
        ~queryFn=Raw.skipToken,
        (),
      )
    },
  );

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
  Raw.useQuery(
    Query.options(
      ~select,
      ~enabled?,
      ~staleTime?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      query,
      key,
    ),
  );

let useSuspenseQuery =
    (~staleTime=?, ~refetchInterval=?, ~refetchOnWindowFocus=?, query, key) =>
  Raw.useSuspenseQuery(
    Query.options(
      ~staleTime?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      query,
      key,
    ),
  );

module Queries = {
  type t('results) = {
    options: array(Raw.anyQueryOptions),
    read: (Raw.queriesResult, int) => 'results,
  };
  let one = (query, key) => {
    options: [|Raw.anyQueryOptions(Query.options(query, key))|],
    read: Raw.resultAt,
  };
  let both = (a, b) => {
    options: Array.append(a.options, b.options),
    read: (results, at) => (
      a.read(results, at),
      b.read(results, at + Array.length(a.options)),
    ),
  };
  let all = items => {
    options:
      Array.concat(Array.to_list(Array.map(item => item.options, items))),
    read: (results, at) => {
      let at = ref(at);
      Array.map(
        item => {
          let result = item.read(results, at^);
          at := at^ + Array.length(item.options);
          result;
        },
        items,
      );
    },
  };
};

module SuspenseQueries = Queries;

let useQueries = (queries: Queries.t(_)) =>
  queries.read(Raw.useQueries({ queries: queries.options }), 0);

let usePrefetchQuery = (query, key) =>
  Raw.usePrefetchQuery(Query.options(query, key));

let usePrefetchInfiniteQuery = (~pages=?, query, key) =>
  Raw.usePrefetchInfiniteQuery(InfiniteQuery.options(~pages?, query, key));

let useSuspenseQueries = (queries: Queries.t(_)) =>
  queries.read(Raw.useSuspenseQueries({ queries: queries.options }), 0);

let settledResult = (data, thrown) =>
  switch (Js.Null.toOption(thrown)) {
  | Some(thrown) => Stdlib.Error(toExn(thrown))
  | None => Stdlib.Ok(data)
  };

let pageFetch = promise =>
  promise |> Js.Promise.then_(_ => Js.Promise.resolve());

module InfiniteResult = {
  type t('page, 'param) = Raw.queryResult(infiniteData('page, 'param));
  let status = QueryResult.status;
  let data = QueryResult.data;
  let error = QueryResult.error;
  let fetchStatus = QueryResult.fetchStatus;
  let isFetching = QueryResult.isFetching;
  let isStale = QueryResult.isStale;
  let dataUpdatedAt = QueryResult.dataUpdatedAt;
  let refetch = QueryResult.refetch;
  let hasNextPage = Raw.hasNextPage;
  let hasPreviousPage = Raw.hasPreviousPage;
  let isFetchingNextPage = Raw.isFetchingNextPage;
  let isFetchingPreviousPage = Raw.isFetchingPreviousPage;
  let fetchNextPage = result => pageFetch(Raw.fetchNextPage(result));
  let fetchPreviousPage = result => pageFetch(Raw.fetchPreviousPage(result));
};

module SuspenseInfiniteResult = {
  include InfiniteResult;
  let data = Raw.definedData;
  let refetchError = QueryResult.error;
};

let useInfiniteQuery =
    (
      ~enabled=?,
      ~staleTime=?,
      ~refetchInterval=?,
      ~refetchOnWindowFocus=?,
      query,
      key,
    )
    : InfiniteResult.t(_, _) =>
  Raw.useInfiniteQuery(
    InfiniteQuery.options(
      ~enabled?,
      ~staleTime?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      query,
      key,
    ),
  );

let useSuspenseInfiniteQuery =
    (~staleTime=?, ~refetchInterval=?, ~refetchOnWindowFocus=?, query, key)
    : SuspenseInfiniteResult.t(_, _) =>
  Raw.useSuspenseInfiniteQuery(
    InfiniteQuery.options(
      ~staleTime?,
      ~refetchInterval?,
      ~refetchOnWindowFocus?,
      query,
      key,
    ),
  );

module Mutation = {
  type t('variables, 'data) =
    | Mutation({
        name: string,
        options: Raw.mutationOptions('variables, 'data, 'context),
      })
      : t('variables, 'data);

  let mutationKey = name => Key.list([Key.string(name)]);

  let make =
      (
        ~name,
        ~fn,
        ~onMutate=?,
        ~onSuccess=?,
        ~onError=?,
        ~onSettled=?,
        ~retry=?,
        ~retryDelay=?,
        ~scope=?,
        (),
      ) =>
    Mutation({
      name,
      options:
        Raw.mutationOptions(
          ~mutationKey=mutationKey(name),
          ~mutationFn=fn,
          ~onMutate=?
            Option.map(
              onMutate =>
                (. variables, context) =>
                  onMutate(Raw.mutationClient(context), variables),
              onMutate,
            ),
          ~onSuccess=?
            Option.map(
              onSuccess =>
                (. data, variables, onMutateResult, context) =>
                  onSuccess(
                    Raw.mutationClient(context),
                    data,
                    variables,
                    Js.Undefined.toOption(onMutateResult),
                  ),
              onSuccess,
            ),
          ~onError=?
            Option.map(
              onError =>
                (. thrown, variables, onMutateResult, context) =>
                  onError(
                    Raw.mutationClient(context),
                    toExn(thrown),
                    variables,
                    Js.Undefined.toOption(onMutateResult),
                  ),
              onError,
            ),
          ~onSettled=?
            Option.map(
              onSettled =>
                (. data, thrown, variables, onMutateResult, context) =>
                  onSettled(
                    Raw.mutationClient(context),
                    settledResult(data, thrown),
                    variables,
                    Js.Undefined.toOption(onMutateResult),
                  ),
              onSettled,
            ),
          ~retry=?Option.map(Retry.toRaw, retry),
          ~retryDelay=?Option.map(retryDelayToRaw, retryDelay),
          ~scope=?Option.map(id => { Raw.id: id }, scope),
          (),
        ),
    });

  let filters = name => {
    Raw.mutationKey: mutationKey(name),
    exact: true,
  };
};

type mutationStatus('variables, 'data) =
  | Idle
  | Running('variables)
  | Failed(exn, 'variables)
  | Succeeded('data, 'variables);

let mutationStatus = (~status, ~error, ~variables, ~data) =>
  switch (status, Js.Null.toOption(error)) {
  | ("pending", _) => Running(variables())
  | ("error", Some(thrown)) => Failed(toExn(thrown), variables())
  | ("success", _) => Succeeded(data(), variables())
  | _ => Idle
  };

module MutationResult = {
  type t('variables, 'data) = Raw.mutationResult('variables, 'data);
  let status = result =>
    mutationStatus(
      ~status=Raw.mutationStatus(result),
      ~error=Raw.mutationError(result),
      ~variables=() => Raw.mutationVariables(result),
      ~data=() => Raw.mutationData(result),
    );
  let isPending = Raw.mutationIsPending;
  let mutate = (~onSuccess=?, ~onError=?, ~onSettled=?, result, variables) =>
    Raw.mutate(
      result,
      variables,
      Raw.mutateOptions(
        ~onSuccess=?
          Option.map(
            onSuccess => (. data, variables) => onSuccess(data, variables),
            onSuccess,
          ),
        ~onError=?
          Option.map(
            onError =>
              (. thrown, variables) => onError(toExn(thrown), variables),
            onError,
          ),
        ~onSettled=?
          Option.map(
            onSettled =>
              (. data, thrown, variables) =>
                onSettled(settledResult(data, thrown), variables),
            onSettled,
          ),
        (),
      ),
    );
  let mutateAsync = Raw.mutateAsync;
  let reset = Raw.resetMutation;
};

let useMutation = (Mutation.Mutation({ options, _ })) =>
  Raw.useMutation(options);

let useMutationState = (Mutation.Mutation({ name, _ })) =>
  Raw.useMutationState({ filters: Mutation.filters(name) })
  |> Array.map(state =>
       mutationStatus(
         ~status=Raw.stateStatus(state),
         ~error=Raw.stateError(state),
         ~variables=() => Raw.stateVariables(state),
         ~data=() => Raw.stateData(state),
       )
     );

let useIsMutating = (~mutation=?, ()) =>
  Raw.useIsMutating(
    switch (mutation) {
    | Some(Mutation.Mutation({ name, _ })) =>
      Js.Undefined.return(Mutation.filters(name))
    | None => Js.undefined
    },
  );

module DehydratedState = {
  type t = array(Js.Json.t);
  let toJson = queries =>
    Js.Json.object_(
      Js.Dict.fromList([
        ("queries", Js.Json.array(queries)),
        ("mutations", Js.Json.array([||])),
      ]),
    );
  let fromJson = json =>
    switch (Option.bind(field(json, "queries"), Js.Json.decodeArray)) {
    | Some(queries) => queries
    | None =>
      Melange_json.of_json_msg_error("a dehydrated state needs queries")
    };
};

let dehydrate = client =>
  Raw.getAllQueries(Raw.getQueryCache(client))
  |> Array.to_list
  |> List.filter_map(query =>
       switch (
         Raw.queryStateStatus(Raw.stateOf(query)),
         Hashtbl.find_opt(registry, CachedQuery.name(query)),
       ) {
       | ("success", Some(registered)) =>
         switch (registered.dehydrate(query)) {
         | dehydrated => Some(dehydrated)
         | exception _ => None
         }
       | _ => None
       }
     )
  |> Array.of_list;

let decodeState = state =>
  state
  |> Array.to_list
  |> List.filter_map(message =>
       switch (
         Option.bind(field(message, "state"), state => field(state, "data"))
       ) {
       | Some(data) => toDehydrated(message, Data(data))
       | None =>
         Some(Raw.dehydratedQuery(message, Js.undefined, Js.undefined))
       }
     )
  |> Array.of_list;

let hydrate = (client, state) =>
  Raw.hydrate(client, { queries: decodeState(state) });

module HydrationBoundary = {
  [@react.component]
  let make = (~state, ~children) => {
    let state =
      React.useMemo1(
        (): Raw.dehydratedState => { queries: decodeState(state) },
        [|state|],
      );
    <Raw.HydrationBoundary state> children </Raw.HydrationBoundary>;
  };
};

module Persister = {
  /* Saves the client as [dehydrate] gives it, so each query's data is in
     its codec's JSON. */
  type t = Raw.persister;

  type persistedClient = {
    timestamp: float,
    buster: string,
    clientState: DehydratedState.t,
  };

  module Storage = {
    type t = unit => Js.undefined(Raw.storage);
    let local = () => Raw.windowStorage("localStorage");
    let session = () => Raw.windowStorage("sessionStorage");
    let make = (~getItem, ~setItem, ~removeItem, ()) =>
      Js.Undefined.return(
        Raw.storage(
          ~getItem=(. key) => Js.Null.fromOption(getItem(key)),
          ~setItem=(. key, value) => setItem(key, value),
          ~removeItem=(. key) => removeItem(key),
        ),
      );
  };

  let syncStorage = (~storage=Storage.local, ~key=?, ~throttleTime=?, ()) =>
    Raw.createSyncStoragePersister(
      Raw.syncStorageOptions(~storage=storage(), ~key?, ~throttleTime?, ()),
    );

  let make = (~persist, ~restore, ~remove) =>
    Raw.persister(
      ~persistClient=
        (. { Raw.timestamp, buster, clientState }) =>
          persist({
            timestamp,
            buster,
            clientState: DehydratedState.fromJson(clientState),
          }),
      ~restoreClient=
        (.) =>
          restore()
          |> Js.Promise.then_(restored =>
               Js.Promise.resolve(
                 Js.Undefined.fromOption(
                   Option.map(
                     ({ timestamp, buster, clientState }) =>
                       {
                         Raw.timestamp,
                         buster,
                         clientState: DehydratedState.toJson(clientState),
                       },
                     restored,
                   ),
                 ),
               )
             ),
      ~removeClient=(.) => remove(),
    );

  /* What TanStack's persist functions get: [persister] with the client
     encoded on the way out and decoded on the way in. */
  let forClient = (client, persister) =>
    Raw.persister(
      ~persistClient=
        (. { Raw.timestamp, buster, _ }) =>
          Raw.promisable(
            Raw.persistClient(
              persister,
              {
                timestamp,
                buster,
                clientState: DehydratedState.toJson(dehydrate(client)),
              },
            ),
          ),
      ~restoreClient=
        (.) =>
          Raw.promisable(Raw.restoreClient(persister))
          |> Js.Promise.then_(restored =>
               Js.Promise.resolve(
                 Js.Undefined.fromOption(
                   Option.map(
                     ({ Raw.timestamp, buster, clientState }) =>
                       {
                         Raw.timestamp,
                         buster,
                         clientState: {
                           Raw.queries:
                             decodeState(
                               DehydratedState.fromJson(clientState),
                             ),
                         },
                       },
                     Js.Undefined.toOption(restored),
                   ),
                 ),
               )
             ),
      ~removeClient=(.) => Raw.promisable(Raw.removeClient(persister)),
    );

  let options = (~client, ~persister, ~maxAge=?, ~buster=?, ()) =>
    Raw.persistOptions(
      ~queryClient=client,
      ~persister=forClient(client, persister),
      ~maxAge?,
      ~buster?,
      (),
    );
};

let persistQueryClientRestore =
    (~client, ~persister, ~maxAge=?, ~buster=?, ()) =>
  Raw.persistQueryClientRestore(
    Persister.options(~client, ~persister, ~maxAge?, ~buster?, ()),
  );

let persistQueryClientSave = (~client, ~persister, ~buster=?, ()) =>
  Raw.persistQueryClientSave(
    Persister.options(~client, ~persister, ~buster?, ()),
  );

let persistQueryClientSubscribe = (~client, ~persister, ~buster=?, ()) => {
  let unsubscribe =
    Raw.persistQueryClientSubscribe(
      Persister.options(~client, ~persister, ~buster?, ()),
    );
  () => unsubscribe(.);
};

let persistQueryClient = (~client, ~persister, ~maxAge=?, ~buster=?, ()) => {
  let (unsubscribe, restored) =
    Raw.persistQueryClient(
      Persister.options(~client, ~persister, ~maxAge?, ~buster?, ()),
    );
  (() => unsubscribe(.), restored);
};

let useIsRestoring = Raw.useIsRestoring;

module IsRestoringProvider = {
  [@react.component]
  let make = (~value, ~children) =>
    <Raw.IsRestoringProvider value> children </Raw.IsRestoringProvider>;
};

module PersistQueryClientProvider = {
  [@react.component]
  let make =
      (
        ~client,
        ~persister,
        ~maxAge=?,
        ~buster=?,
        ~onSuccess=?,
        ~onError=?,
        ~children,
      ) => {
    let persistOptions =
      React.useMemo4(
        () => Persister.options(~client, ~persister, ~maxAge?, ~buster?, ()),
        (client, persister, maxAge, buster),
      );
    <Raw.PersistQueryClientProvider client persistOptions ?onSuccess ?onError>
      children
    </Raw.PersistQueryClientProvider>;
  };
};

module QueryErrorResetBoundary = {
  type t = Raw.errorResetBoundary;
  let reset = Raw.resetErrorBoundary;
  let isReset = Raw.isResetErrorBoundary;
  let clearReset = Raw.clearResetErrorBoundary;

  [@react.component]
  let make = (~children) =>
    <Raw.QueryErrorResetBoundary> children </Raw.QueryErrorResetBoundary>;
};

let useQueryErrorResetBoundary = Raw.useQueryErrorResetBoundary;

module ErrorBoundary = {
  [@react.component]
  let make = (~fallback, ~children) => {
    let boundary = Raw.useQueryErrorResetBoundary();
    React.createElement(
      Raw.errorBoundary,
      {
        Raw.children,
        fallback:
          (. thrown, resetBoundary) =>
            fallback(
              ~error=toExn(thrown),
              ~retry=() => {
                Raw.resetErrorBoundary(boundary);
                resetBoundary();
              },
            ),
      },
    );
  };
};

/* Prefetches the server announced as pending, until their result arrives. */
let streaming: Hashtbl.t(string, Raw.deferred(Js.Json.t)) =
  Hashtbl.create(8);

let hydrateOne = (client, message, server) =>
  Option.iter(
    dehydrated => Raw.hydrate(client, { queries: [|dehydrated|] }),
    toDehydrated(message, server),
  );

/* A pending query's result settles the promise [hydrate] got, like the
   promises TanStack streams from server components. */
let hydrateQuery = (client, message) => {
  let stateField = name =>
    Option.bind(field(message, "state"), state => field(state, name));
  switch (
    Option.bind(field(message, "queryHash"), Js.Json.decodeString),
    Option.bind(stateField("status"), Js.Json.decodeString),
  ) {
  | (Some(queryHash), Some(status)) =>
    switch (status, Hashtbl.find_opt(streaming, queryHash)) {
    | ("pending", None) =>
      let deferred = Raw.deferred();
      Hashtbl.replace(streaming, queryHash, deferred);
      hydrateOne(client, message, Later(deferred.promise));
    | ("success", Some(deferred)) =>
      Hashtbl.remove(streaming, queryHash);
      switch (stateField("data")) {
      | Some(data) => deferred.resolve(. data)
      | None => deferred.redact(.)
      };
    | ("success", None) =>
      Option.iter(
        data => hydrateOne(client, message, Data(data)),
        stateField("data"),
      )
    | ("error", Some(deferred)) =>
      Hashtbl.remove(streaming, queryHash);
      deferred.redact(.);
    | _ => ()
    }
  | _ =>
    Js.Console.error2("ReactQuery: malformed query from the server", message)
  };
};

let listen: (array(Js.Json.t) => unit) => unit = [%mel.raw
  {|
  function (hydrate) {
    (self.__RQ || []).forEach(hydrate);
    self.__RQ = { push: hydrate };
  }
|}
];

let hydrateFromServer = client =>
  listen(messages => Array.iter(hydrateQuery(client), messages));

let cleanupToRaw = cleanup =>
  Js.Undefined.fromOption(Option.map(cleanup => (.) => cleanup(), cleanup));

module FocusManager = {
  let isFocused = () => Raw.isFocused(Raw.focusManager);
  let setFocused = focused =>
    Raw.setFocused(Raw.focusManager, Js.Undefined.fromOption(focused));
  let subscribe = listener => {
    let unsubscribe =
      Raw.subscribeFocus(Raw.focusManager, (. focused) => listener(focused));
    () => unsubscribe(.);
  };
  let setEventListener = setup =>
    Raw.setFocusEventListener(Raw.focusManager, (. setFocused) =>
      cleanupToRaw(
        setup(focused => setFocused(. Js.Undefined.fromOption(focused))),
      )
    );
};

module OnlineManager = {
  let isOnline = () => Raw.isOnline(Raw.onlineManager);
  let setOnline = online => Raw.setOnline(Raw.onlineManager, online);
  let subscribe = listener => {
    let unsubscribe =
      Raw.subscribeOnline(Raw.onlineManager, (. online) => listener(online));
    () => unsubscribe(.);
  };
  let setEventListener = setup =>
    Raw.setOnlineEventListener(Raw.onlineManager, (. setOnline) =>
      cleanupToRaw(setup(online => setOnline(. online)))
    );
};

module NotifyManager = {
  let batch = callback => Raw.batch(Raw.notifyManager, (.) => callback());
  let batchCalls = callback => {
    let scheduled =
      Raw.batchCallsWith(Raw.notifyManager, (. value) => callback(value));
    value => scheduled(. value);
  };
  let schedule = callback =>
    Raw.schedule(Raw.notifyManager, (.) => callback());
  let toRaw = (fn, callback) => fn(() => callback(.));
  let setNotifyFunction = fn =>
    Raw.setNotifyFunction(Raw.notifyManager, (. callback) =>
      toRaw(fn, callback)
    );
  let setBatchNotifyFunction = fn =>
    Raw.setBatchNotifyFunction(Raw.notifyManager, (. callback) =>
      toRaw(fn, callback)
    );
  let setScheduler = fn =>
    Raw.setScheduler(Raw.notifyManager, (. callback) => toRaw(fn, callback));
  let defaultScheduler = callback => Raw.defaultScheduler(. (.) => callback());
};
